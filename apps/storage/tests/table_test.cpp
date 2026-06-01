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

TEST_F(TableTest, IndexLookupSurvivesUpdateAndReopen) {
    auto schema = Schema({
        Column("id", Column::ColumnType::INT, Column::INDEXED_FLAG),
        Column("code", Column::ColumnType::STRING, Column::INDEXED_FLAG),
    });

    {
        auto table = Table("indexed", table_root, schema, &interner);
        auto first = table.insert_record({Value(1), interner.Intern("abcdefgh1")});
        auto second = table.insert_record({Value(2), interner.Intern("abcdefgh2")});

        auto by_string = table.find_by_index("code", interner.Intern("abcdefgh2"));
        ASSERT_TRUE(by_string.has_value());
        ASSERT_EQ(by_string->size(), 1);
        EXPECT_EQ((*by_string)[0][0].AsInt(), 2);

        second[0] = Value(3);
        table.update_record(second);
        EXPECT_TRUE(table.find_by_index("id", Value(2))->empty());
        auto updated_code = table.find_by_index("id", Value(3))->at(0)[1].AsString();
        EXPECT_EQ(interner.View(updated_code), "abcdefgh2");
        (void)first;
    }

    Interner reopened_interner;
    auto reopened = Table("indexed", table_root, &reopened_interner);
    auto found = reopened.find_by_index("id", Value(3));
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->size(), 1);
    EXPECT_EQ(reopened_interner.View(found->at(0)[1].AsString()), "abcdefgh2");
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
            {Value(10), Value(20), interner.Intern("abc"), interner.Intern("def")};
        EXPECT_NO_THROW(table.insert_record(values_without_nulls));
    }
    {
        std::vector<Value> values_with_correct_nulls = {Value(), Value(20), Value(), interner.Intern("def")};
        EXPECT_NO_THROW(table.insert_record(values_with_correct_nulls));
    }
    {
        std::vector<Value> values_with_wrong_nulls = {Value(10), Value(), interner.Intern("abc"), Value()};
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

    EXPECT_EQ(record[0].AsInt(), 1);
    EXPECT_EQ(interner.View(record[1].AsString()), "anon");
    EXPECT_EQ(record[2].AsInt(), 7);
    EXPECT_TRUE(record[3].IsNull());
}

TEST_F(TableTest, InsertExplicitValueOverridesDefault) {
    std::vector<Column> columns;
    columns.push_back(Column("id", Column::ColumnType::INT, Column::NOT_NULL_FLAG));
    columns.push_back(
        Column("name", Column::ColumnType::STRING, Column::NOT_NULL_FLAG, Column::DefaultType::STRING, 0, "anon")
    );
    auto schema = Schema(columns);
    auto table = Table("explicit_defaults", table_root, schema, &interner);

    auto record = table.insert_record({Value(1), interner.Intern("Alice")}, {"id", "name"});

    EXPECT_EQ(record[0].AsInt(), 1);
    EXPECT_EQ(interner.View(record[1].AsString()), "Alice");
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

    std::vector<Value> values_without_nulls = {Value(10), Value(20), interner.Intern("abc"), interner.Intern("def")};
    EXPECT_NO_THROW(table.insert_record(values_without_nulls));

    std::vector<Value> values_with_correct_nulls = {Value(), Value(30), Value(), interner.Intern("ghi")};
    EXPECT_NO_THROW(table.insert_record(values_with_correct_nulls));

    std::vector<Value> values_with_wrong_nulls = {Value(10), Value(), interner.Intern("jkl"), Value()};
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

    std::vector<Value> some_values = {Value(10), Value(20), interner.Intern("abc"), interner.Intern("def")};
    EXPECT_NO_THROW(table.insert_record(some_values));

    std::vector<Value> values_without_duplicates =
        {Value(30), Value(40), interner.Intern("xyz"), interner.Intern("knm")};
    EXPECT_NO_THROW(table.insert_record(values_without_duplicates));

    std::vector<Value> values_with_duplicated_int_indexed_field =
        {Value(50), Value(20), interner.Intern("a"), interner.Intern("b")};
    EXPECT_ANY_THROW(table.insert_record(values_with_duplicated_int_indexed_field));

    std::vector<Value> values_with_duplicated_str_indexed_field =
        {Value(50), Value(60), interner.Intern("a"), interner.Intern("knm")};
    EXPECT_ANY_THROW(table.insert_record(values_with_duplicated_str_indexed_field));

    std::vector<Value> values_with_duplicated_not_indexed_field =
        {Value(30), Value(60), interner.Intern("abc"), interner.Intern("b")};
    EXPECT_NO_THROW(table.insert_record(values_with_duplicated_not_indexed_field));
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

    std::vector<Value> values_1 = {Value(1), Value(2), interner.Intern("a"), interner.Intern("b")};
    auto record_1 = table.insert_record(values_1);
    std::vector<Value> values_2 = {Value(3), Value(4), interner.Intern("c"), interner.Intern("d")};
    auto record_2 = table.insert_record(values_2);
    record_1[0] = Value(3);
    EXPECT_NO_THROW(table.update_record(record_1));
    record_1[1] = Value(4);
    EXPECT_ANY_THROW(table.update_record(record_1));
    record_1[1] = Value(2);
    EXPECT_NO_THROW(table.update_record(record_1));
    record_1[3] = interner.Intern("d");
    EXPECT_ANY_THROW(table.update_record(record_1));
    record_1[3] = interner.Intern("b");
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

    std::vector<Value> values_1 =
        {Value(10), Value(10), Value(10), interner.Intern("a"), interner.Intern("b"), interner.Intern("z")};
    std::vector<Value> values_2 =
        {Value(10), Value(20), Value(20), Value(), interner.Intern("c"), interner.Intern("y")};
    std::vector<Value> values_3 =
        {Value(), Value(20), Value(30), interner.Intern("a"), interner.Intern("d"), interner.Intern("x")};
    std::vector<Value> values_4 =
        {Value(20), Value(20), Value(40), interner.Intern("a"), interner.Intern("d"), interner.Intern("w")};
    std::vector<Value> values_5 =
        {Value(20), Value(30), Value(50), interner.Intern("b"), interner.Intern("f"), interner.Intern("v")};
    std::vector<Value> values_6 =
        {Value(30), Value(30), Value(60), interner.Intern("c"), interner.Intern("f"), interner.Intern("u")};
    auto record_1 = table.insert_record(values_1);
    auto record_2 = table.insert_record(values_2);
    auto record_3 = table.insert_record(values_3);
    auto record_4 = table.insert_record(values_4);
    auto record_5 = table.insert_record(values_5);
    auto record_6 = table.insert_record(values_6);

    EXPECT_EQ(record_1, *table.read_record(record_1.Address()));
    EXPECT_EQ(record_2, *table.read_record(record_2.Address()));
    EXPECT_EQ(record_3, *table.read_record(record_3.Address()));
    EXPECT_EQ(record_4, *table.read_record(record_4.Address()));
    EXPECT_EQ(record_5, *table.read_record(record_5.Address()));
    EXPECT_EQ(record_6, *table.read_record(record_6.Address()));

    record_1[0] = Value(10);
    record_2[1] = Value(30);
    record_3[2] = Value(70);
    record_4[3] = Value();
    record_5[4] = interner.Intern("g");
    record_6[5] = interner.Intern("t");

    EXPECT_NO_THROW(table.update_record(record_1));
    EXPECT_NO_THROW(table.update_record(record_2));
    EXPECT_NO_THROW(table.update_record(record_3));
    EXPECT_NO_THROW(table.update_record(record_4));
    EXPECT_NO_THROW(table.update_record(record_5));
    EXPECT_NO_THROW(table.update_record(record_6));

    EXPECT_EQ(record_1, *table.read_record(record_1.Address()));
    EXPECT_EQ(record_2, *table.read_record(record_2.Address()));
    EXPECT_EQ(record_3, *table.read_record(record_3.Address()));
    EXPECT_EQ(record_4, *table.read_record(record_4.Address()));
    EXPECT_EQ(record_5, *table.read_record(record_5.Address()));
    EXPECT_EQ(record_6, *table.read_record(record_6.Address()));

    EXPECT_NO_THROW(table.delete_record(record_1));
    EXPECT_NO_THROW(table.delete_record(record_3));
    EXPECT_NO_THROW(table.delete_record(record_5));
    EXPECT_EQ(table.read_record(record_1.Address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_3.Address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_5.Address()), std::nullopt);
    EXPECT_EQ(*table.read_record(record_2.Address()), record_2);
    EXPECT_EQ(*table.read_record(record_4.Address()), record_4);
    EXPECT_EQ(*table.read_record(record_6.Address()), record_6);

    EXPECT_NO_THROW(table.delete_record(record_2));
    EXPECT_NO_THROW(table.delete_record(record_4));
    EXPECT_NO_THROW(table.delete_record(record_6));
    EXPECT_EQ(table.read_record(record_2.Address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_4.Address()), std::nullopt);
    EXPECT_EQ(table.read_record(record_6.Address()), std::nullopt);
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
    auto snapshot = [&table]() {
        std::stringstream out;
        for (const auto& record : table.records()) {
            out << record.Id() << ":";
            for (uint32_t i = 0; i < record.Size(); ++i) {
                out << record[i].ToString() << ";";
            }
            out << "\n";
        }
        return out.str();
    };

    std::vector<Value> values_1 =
        {Value(10), Value(10), Value(10), interner.Intern("a"), interner.Intern("b"), interner.Intern("z")};
    std::vector<Value> values_2 =
        {Value(10), Value(20), Value(20), Value(), interner.Intern("c"), interner.Intern("y")};
    std::vector<Value> values_3 =
        {Value(), Value(20), Value(30), interner.Intern("a"), interner.Intern("d"), interner.Intern("x")};
    std::vector<Value> values_4 =
        {Value(20), Value(20), Value(40), interner.Intern("a"), interner.Intern("d"), interner.Intern("w")};
    std::vector<Value> values_5 =
        {Value(20), Value(30), Value(50), interner.Intern("b"), interner.Intern("f"), interner.Intern("v")};
    std::vector<Value> values_6 =
        {Value(30), Value(30), Value(60), interner.Intern("c"), interner.Intern("f"), interner.Intern("u")};

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_1 = Journal::Track::get_now();
    auto table_snapshot_1 = snapshot();

    auto record_1 = table.insert_record(values_1);

    auto record_2 = table.insert_record(values_2);

    auto record_3 = table.insert_record(values_3);

    auto record_4 = table.insert_record(values_4);

    record_3[2] = Value(70);
    table.update_record(record_3);

    auto record_5 = table.insert_record(values_5);

    record_2[1] = Value(30);
    table.update_record(record_2);

    record_5[4] = interner.Intern("g");
    table.update_record(record_5);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_2 = Journal::Track::get_now();
    auto table_snapshot_2 = snapshot();

    auto record_6 = table.insert_record(values_6);

    record_1[0] = Value(10);
    table.update_record(record_1);

    record_4[3] = Value();
    table.update_record(record_4);

    table.delete_record(record_1);

    record_6[5] = interner.Intern("t");
    table.update_record(record_6);

    table.delete_record(record_3);

    table.delete_record(record_5);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_3 = Journal::Track::get_now();
    auto table_snapshot_3 = snapshot();

    table.delete_record(record_2);

    table.delete_record(record_4);

    table.delete_record(record_6);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto before_4 = Journal::Track::get_now();
    auto table_snapshot_4 = snapshot();

    table.revert(before_4);
    ASSERT_EQ(snapshot(), table_snapshot_4);

    table.revert(before_3);
    ASSERT_EQ(snapshot(), table_snapshot_3);

    table.revert(before_2);
    ASSERT_EQ(snapshot(), table_snapshot_2);

    auto record_id2 = *table.read_by_id(2);
    record_id2[2] = Value(1000);
    table.update_record(record_id2);

    table.revert(before_1);
    ASSERT_EQ(snapshot(), table_snapshot_1);
}

}  // namespace qdb::storage::test
