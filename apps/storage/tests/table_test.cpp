#include "../include/qdb/storage/table.h"
#include <gtest/gtest.h>
#include <sstream>
#include <thread>

namespace qdb::storage::test {

namespace fs = std::filesystem;

class TableTest : public ::testing::Test {
protected:
    fs::path table_root;
    Interner interner;

    void SetUp() override {
        const testing::TestInfo* const test_info = testing::UnitTest::GetInstance()->current_test_info();
        table_root = fs::temp_directory_path() / "TableTest" / test_info->name();
        fs::create_directories(table_root);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(table_root, ec);
    }
};

TEST_F(TableTest, TestSavingTableToDisk) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_not_null", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_not_null", Column::ColumnType::STRING, Column::NOT_NULL_FLAG));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name";
    {
        auto table = Table(table_name, table_root, schema, &interner);
        EXPECT_EQ(table.name(), table_name);
        EXPECT_EQ(table.schema(), schema);
    }
    {
        auto table = Table(table_name, table_root, &interner);
        EXPECT_EQ(table.name(), table_name);
        EXPECT_EQ(table.schema(), schema);
    }
}

TEST_F(TableTest, TestDropTable) {
    std::string table_name = "table_name";
    {
        std::vector<Column> columns;
        columns.push_back(Column("int", Column::ColumnType::INT));
        columns.push_back(Column("str", Column::ColumnType::STRING));
        auto schema = Schema(columns);
        auto table = Table(table_name, table_root, schema, &interner);
    }
    {
        auto table = Table(table_name, table_root, &interner);
        table.drop();
    }
    EXPECT_TRUE(fs::is_empty(table_root));
}

TEST_F(TableTest, InsertNullValidation) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_not_null", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_not_null", Column::ColumnType::STRING, Column::NOT_NULL_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name";
    auto table = Table(table_name, table_root, schema, &interner);
    {
        std::vector<Value> values_without_nulls =
            {Value(10), Value(20), interner.str_to_value("abc"), interner.str_to_value("def")};
        EXPECT_NO_THROW(table.insert_record(values_without_nulls));
    }
    {
        std::vector<Value> values_with_correct_nulls = {Value(), Value(20), Value(), interner.str_to_value("def")};
        EXPECT_NO_THROW(table.insert_record(values_with_correct_nulls));
    }
    {
        std::vector<Value> values_with_wrong_nulls = {Value(10), Value(), interner.str_to_value("abc"), Value()};
        EXPECT_ANY_THROW(table.insert_record(values_with_wrong_nulls));
    }
}

TEST_F(TableTest, InsertUsesDefaultsForOmittedColumns) {
    std::vector<Column> columns;
    columns.push_back(Column("id", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(
        Column("name", Column::ColumnType::STRING, Column::NOT_NULL_FLAG, Column::DefaultType::STRING, 0, "anon")
    );
    columns.push_back(Column("score", Column::ColumnType::INT, 0, Column::DefaultType::INT, 7));
    columns.push_back(Column("comment", Column::ColumnType::STRING));
    auto schema = Schema(columns);
    auto table = Table("defaults", table_root, schema, &interner);

    auto record = table.insert_record({Value(1)}, {"id"});

    EXPECT_EQ(record[0].as_int(), 1);
    EXPECT_EQ(record[1].to_string(), "anon");
    EXPECT_EQ(record[2].as_int(), 7);
    EXPECT_TRUE(record[3].is_null());
}

TEST_F(TableTest, InsertExplicitValueOverridesDefault) {
    std::vector<Column> columns;
    columns.push_back(Column("id", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(
        Column("name", Column::ColumnType::STRING, Column::NOT_NULL_FLAG, Column::DefaultType::STRING, 0, "anon")
    );
    auto schema = Schema(columns);
    auto table = Table("explicit_defaults", table_root, schema, &interner);

    auto record = table.insert_record({Value(1), interner.str_to_value("Alice")}, {"id", "name"});

    EXPECT_EQ(record[0].as_int(), 1);
    EXPECT_EQ(record[1].to_string(), "Alice");
}

TEST_F(TableTest, InsertNullInIndexedValidation) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name";
    auto table = Table(table_name, table_root, schema, &interner);

    std::vector<Value> values_without_nulls =
        {Value(10), Value(20), interner.str_to_value("abc"), interner.str_to_value("def")};
    EXPECT_NO_THROW(table.insert_record(values_without_nulls));

    std::vector<Value> values_with_correct_nulls = {Value(), Value(30), Value(), interner.str_to_value("ghi")};
    EXPECT_NO_THROW(table.insert_record(values_with_correct_nulls));

    std::vector<Value> values_with_wrong_nulls = {Value(10), Value(), interner.str_to_value("jkl"), Value()};
    EXPECT_ANY_THROW(table.insert_record(values_with_wrong_nulls));
}

TEST_F(TableTest, InsertUniqueConstraintByIndex) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name_1";
    auto table = Table(table_name, table_root, schema, &interner);

    std::vector<Value> some_values = {Value(10), Value(20), interner.str_to_value("abc"), interner.str_to_value("def")};
    EXPECT_NO_THROW(table.insert_record(some_values));

    std::vector<Value> values_without_dublicates =
        {Value(30), Value(40), interner.str_to_value("xyz"), interner.str_to_value("knm")};
    EXPECT_NO_THROW(table.insert_record(values_without_dublicates));

    std::vector<Value> values_with_dublicated_int_indexed_field =
        {Value(50), Value(20), interner.str_to_value("a"), interner.str_to_value("b")};
    EXPECT_ANY_THROW(table.insert_record(values_with_dublicated_int_indexed_field));

    std::vector<Value> values_with_dublicated_str_indexed_field =
        {Value(50), Value(60), interner.str_to_value("a"), interner.str_to_value("knm")};
    EXPECT_ANY_THROW(table.insert_record(values_with_dublicated_str_indexed_field));

    std::vector<Value> values_with_dublicated_not_indexed_field =
        {Value(30), Value(60), interner.str_to_value("abc"), interner.str_to_value("b")};
    EXPECT_NO_THROW(table.insert_record(values_with_dublicated_not_indexed_field));
}

TEST_F(TableTest, UpdateUniqueConstraintByIndex) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name_1";
    auto table = Table(table_name, table_root, schema, &interner);

    std::vector<Value> values_1 = {Value(1), Value(2), interner.str_to_value("a"), interner.str_to_value("b")};
    auto record_1 = table.insert_record(values_1);
    std::vector<Value> values_2 = {Value(3), Value(4), interner.str_to_value("c"), interner.str_to_value("d")};
    auto record_2 = table.insert_record(values_2);
    record_1[0] = Value(3);
    EXPECT_NO_THROW(table.update_record(record_1));
    record_1[1] = Value(4);
    EXPECT_ANY_THROW(table.update_record(record_1));
    record_1[1] = Value(2);
    EXPECT_NO_THROW(table.update_record(record_1));
    record_1[3] = interner.str_to_value("d");
    EXPECT_ANY_THROW(table.update_record(record_1));
    record_1[3] = interner.str_to_value("b");
    EXPECT_NO_THROW(table.update_record(record_1));
}

TEST_F(TableTest, CRUD_test) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_not_null", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_not_null", Column::ColumnType::STRING, Column::NOT_NULL_FLAG));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name_2";
    auto table = Table(table_name, table_root, schema, &interner);

    std::vector<Value> values_1 = {
        Value(10),
        Value(10),
        Value(10),
        interner.str_to_value("a"),
        interner.str_to_value("b"),
        interner.str_to_value("z")};
    std::vector<Value> values_2 =
        {Value(10), Value(20), Value(20), Value(), interner.str_to_value("c"), interner.str_to_value("y")};
    std::vector<Value> values_3 = {
        Value(),
        Value(20),
        Value(30),
        interner.str_to_value("a"),
        interner.str_to_value("d"),
        interner.str_to_value("x")};
    std::vector<Value> values_4 = {
        Value(20),
        Value(20),
        Value(40),
        interner.str_to_value("a"),
        interner.str_to_value("d"),
        interner.str_to_value("w")};
    std::vector<Value> values_5 = {
        Value(20),
        Value(30),
        Value(50),
        interner.str_to_value("b"),
        interner.str_to_value("f"),
        interner.str_to_value("v")};
    std::vector<Value> values_6 = {
        Value(30),
        Value(30),
        Value(60),
        interner.str_to_value("c"),
        interner.str_to_value("f"),
        interner.str_to_value("u")};
    auto record_1 = table.insert_record(values_1);
    auto record_2 = table.insert_record(values_2);
    auto record_3 = table.insert_record(values_3);
    auto record_4 = table.insert_record(values_4);
    auto record_5 = table.insert_record(values_5);
    auto record_6 = table.insert_record(values_6);

    EXPECT_EQ(record_1, *table.read_record(record_1.address()));
    EXPECT_EQ(record_2, *table.read_record(record_2.address()));
    EXPECT_EQ(record_3, *table.read_record(record_3.address()));
    EXPECT_EQ(record_4, *table.read_record(record_4.address()));
    EXPECT_EQ(record_5, *table.read_record(record_5.address()));
    EXPECT_EQ(record_6, *table.read_record(record_6.address()));

    record_1[0] = Value(10);
    record_2[1] = Value(30);
    record_3[2] = Value(70);
    record_4[3] = Value();
    record_5[4] = interner.str_to_value("g");
    record_6[5] = interner.str_to_value("t");

    EXPECT_NO_THROW(table.update_record(record_1));
    EXPECT_NO_THROW(table.update_record(record_2));
    EXPECT_NO_THROW(table.update_record(record_3));
    EXPECT_NO_THROW(table.update_record(record_4));
    EXPECT_NO_THROW(table.update_record(record_5));
    EXPECT_NO_THROW(table.update_record(record_6));

    EXPECT_EQ(record_1, *table.read_record(record_1.address()));
    EXPECT_EQ(record_2, *table.read_record(record_2.address()));
    EXPECT_EQ(record_3, *table.read_record(record_3.address()));
    EXPECT_EQ(record_4, *table.read_record(record_4.address()));
    EXPECT_EQ(record_5, *table.read_record(record_5.address()));
    EXPECT_EQ(record_6, *table.read_record(record_6.address()));

    EXPECT_NO_THROW(table.delete_record(record_1));
    EXPECT_NO_THROW(table.delete_record(record_3));
    EXPECT_NO_THROW(table.delete_record(record_5));
    EXPECT_EQ(table.read_record(record_1.address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_3.address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_5.address()), std::nullopt);
    EXPECT_EQ(*table.read_record(record_2.address()), record_2);
    EXPECT_EQ(*table.read_record(record_4.address()), record_4);
    EXPECT_EQ(*table.read_record(record_6.address()), record_6);

    EXPECT_NO_THROW(table.delete_record(record_2));
    EXPECT_NO_THROW(table.delete_record(record_4));
    EXPECT_NO_THROW(table.delete_record(record_6));
    EXPECT_EQ(table.read_record(record_2.address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_4.address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_6.address()), std::nullopt);
}

TEST_F(TableTest, RevertTest) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_not_null", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_not_null", Column::ColumnType::STRING, Column::NOT_NULL_FLAG));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name_3";
    auto table = Table(table_name, table_root, schema, &interner);

    std::vector<Value> values_1 = {
        Value(10),
        Value(10),
        Value(10),
        interner.str_to_value("a"),
        interner.str_to_value("b"),
        interner.str_to_value("z")};
    std::vector<Value> values_2 =
        {Value(10), Value(20), Value(20), Value(), interner.str_to_value("c"), interner.str_to_value("y")};
    std::vector<Value> values_3 = {
        Value(),
        Value(20),
        Value(30),
        interner.str_to_value("a"),
        interner.str_to_value("d"),
        interner.str_to_value("x")};
    std::vector<Value> values_4 = {
        Value(20),
        Value(20),
        Value(40),
        interner.str_to_value("a"),
        interner.str_to_value("d"),
        interner.str_to_value("w")};
    std::vector<Value> values_5 = {
        Value(20),
        Value(30),
        Value(50),
        interner.str_to_value("b"),
        interner.str_to_value("f"),
        interner.str_to_value("v")};
    std::vector<Value> values_6 = {
        Value(30),
        Value(30),
        Value(60),
        interner.str_to_value("c"),
        interner.str_to_value("f"),
        interner.str_to_value("u")};

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_1 = Journal::Track::get_now();
    std::stringstream table_str_dump_1;
    table_str_dump_1 << table;

    auto record_1 = table.insert_record(values_1);

    auto record_2 = table.insert_record(values_2);

    auto record_3 = table.insert_record(values_3);

    auto record_4 = table.insert_record(values_4);

    record_3[2] = Value(70);
    table.update_record(record_3);

    auto record_5 = table.insert_record(values_5);

    record_2[1] = Value(30);
    table.update_record(record_2);

    record_5[4] = interner.str_to_value("g");
    table.update_record(record_5);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_2 = Journal::Track::get_now();
    std::stringstream table_str_dump_2;
    table_str_dump_2 << table;

    auto record_6 = table.insert_record(values_6);

    record_1[0] = Value(10);
    table.update_record(record_1);

    record_4[3] = Value();
    table.update_record(record_4);

    table.delete_record(record_1);

    record_6[5] = interner.str_to_value("t");
    table.update_record(record_6);

    table.delete_record(record_3);

    table.delete_record(record_5);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_3 = Journal::Track::get_now();
    std::stringstream table_str_dump_3;
    table_str_dump_3 << table;

    table.delete_record(record_2);

    table.delete_record(record_4);

    table.delete_record(record_6);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_4 = Journal::Track::get_now();
    std::stringstream table_str_dump_4;
    table_str_dump_4 << table;

    table.revert(before_4);
    std::stringstream table_str_dump_rev4;
    table_str_dump_rev4 << table;

    ASSERT_EQ(table_str_dump_rev4.str(), table_str_dump_4.str());

    table.revert(before_3);
    std::stringstream table_str_dump_rev3;
    table_str_dump_rev3 << table;

    ASSERT_EQ(table_str_dump_rev3.str(), table_str_dump_3.str());

    table.revert(before_2);
    std::stringstream table_str_dump_rev2;
    table_str_dump_rev2 << table;

    ASSERT_EQ(table_str_dump_rev2.str(), table_str_dump_2.str());

    auto record_id2 = *table.read_record(table.get_id_to_addr()->search(2).back());
    record_id2[2] = Value(1000);
    table.update_record(record_id2);

    table.revert(before_1);
    std::stringstream table_str_dump_rev1;
    table_str_dump_rev1 << table;

    ASSERT_EQ(table_str_dump_rev1.str(), table_str_dump_1.str());
}

}  // namespace qdb::storage::test
