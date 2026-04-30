#include <gtest/gtest.h>
#include "../include/qdb/storage/table.h"

namespace fs = std::filesystem;

class TableTest : public ::testing::Test {
protected:
    fs::path table_root;

    void SetUp() override {
        const testing::TestInfo* const test_info = 
            testing::UnitTest::GetInstance()->current_test_info();
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
        auto table = Table(table_name, table_root, schema);
        EXPECT_EQ(table.name(), table_name);
        EXPECT_EQ(table.schema(), schema);
    }
    {
        auto table = Table(table_name, table_root);
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
        auto table = Table(table_name, table_root, schema);
    }
    {
        auto table = Table(table_name, table_root);
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
    auto table = Table(table_name, table_root, schema);
    {
        std::vector<Value> values_without_nulls = { Value(10), Value(20), Value("abc"), Value("def")};
        EXPECT_NO_THROW(table.insert_record(values_without_nulls));
    }
    {
        std::vector<Value> values_with_correct_nulls = { Value(nullptr), Value(20), Value(nullptr), Value("def") };
        EXPECT_NO_THROW(table.insert_record(values_with_correct_nulls));
    }
    {
        std::vector<Value> values_with_wrong_nulls = { Value(10), Value(nullptr), Value("abc"), Value(nullptr) };
        EXPECT_ANY_THROW(table.insert_record(values_with_wrong_nulls));
    }
}

TEST_F(TableTest, InsertNullInIndexedValidation) {
    std::vector<Column> columns;
    columns.push_back(Column("int", Column::ColumnType::INT));
    columns.push_back(Column("int_indexed", Column::ColumnType::INT, Column::INDEXED_FLAG));
    columns.push_back(Column("str", Column::ColumnType::STRING));
    columns.push_back(Column("str_indexed", Column::ColumnType::STRING, Column::INDEXED_FLAG));
    auto schema = Schema(columns);
    std::string table_name = "table_name";
    auto table = Table(table_name, table_root, schema);

    std::vector<Value> values_without_nulls = { Value(10), Value(20), Value("abc"), Value("def") };
    EXPECT_NO_THROW(table.insert_record(values_without_nulls));

    std::vector<Value> values_with_correct_nulls = { Value(nullptr), Value(30), Value(nullptr), Value("ghi") };
    EXPECT_NO_THROW(table.insert_record(values_with_correct_nulls));

    std::vector<Value> values_with_wrong_nulls = { Value(10), Value(nullptr), Value("jkl"), Value(nullptr) };
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
    auto table = Table(table_name, table_root, schema);

    std::vector<Value> some_values = { Value(10), Value(20), Value("abc"), Value("def") };
    EXPECT_NO_THROW(table.insert_record(some_values));

    std::vector<Value> values_without_dublicates = { Value(30), Value(40), Value("xyz"), Value("knm") };
    EXPECT_NO_THROW(table.insert_record(values_without_dublicates));

    std::vector<Value> values_with_dublicated_int_indexed_field = { Value(50), Value(20), Value("a"), Value("b") };
    EXPECT_ANY_THROW(table.insert_record(values_with_dublicated_int_indexed_field));

    std::vector<Value> values_with_dublicated_str_indexed_field = { Value(50), Value(60), Value("a"), Value("knm") };
    EXPECT_ANY_THROW(table.insert_record(values_with_dublicated_str_indexed_field));

    std::vector<Value> values_with_dublicated_not_indexed_field = { Value(30), Value(60), Value("abc"), Value("b") };
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
    auto table = Table(table_name, table_root, schema);

    std::vector<Value> values_1 = { Value(1), Value(2), Value("a"), Value("b") };
    auto record_1 = table.insert_record(values_1);
    std::vector<Value> values_2 = { Value(3), Value(4), Value("c"), Value("d") };
    auto record_2 = table.insert_record(values_2);
    record_1[0] = Value(3);
    EXPECT_NO_THROW(table.update_record(record_1));
    record_1[1] = Value(4);
    EXPECT_ANY_THROW(table.update_record(record_1));
    record_1[1] = Value(2);
    EXPECT_NO_THROW(table.update_record(record_1));
    record_1[3] = Value("d");
    EXPECT_ANY_THROW(table.update_record(record_1));
    record_1[3] = Value("b");
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
    auto table = Table(table_name, table_root, schema);

    std::vector<Value> values_1 = { Value(10), Value(10), Value(10),
        Value("a"), Value("b"), Value("z") };
    std::vector<Value> values_2 = { Value(10), Value(20), Value(20),
        Value(nullptr), Value("c"), Value("y") };
    std::vector<Value> values_3 = { Value(nullptr), Value(20), Value(30),
        Value("a"), Value("d"), Value("x") };
    std::vector<Value> values_4 = { Value(20), Value(20), Value(40),
        Value("a"), Value("d"), Value("w") };
    std::vector<Value> values_5 = { Value(20), Value(30), Value(50),
        Value("b"), Value("f"), Value("v") };
    std::vector<Value> values_6 = { Value(30), Value(30), Value(60),
        Value("c"), Value("f"), Value("u") };
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
    record_4[3] = Value(nullptr);
    record_5[4] = Value("g");
    record_6[5] = Value("t");
    
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