#include "../include/qdb/storage/schema.h"
#include <gtest/gtest.h>
#include <sstream>

namespace qdb::storage::test {

TEST(SchemaTest, DefaultConstructor) {
    Schema schema;
    EXPECT_EQ(schema.size(), 0);
    EXPECT_EQ(schema.record_id_count(), 0);
    EXPECT_EQ(schema.null_bitmap_size(), 0);
}

TEST(SchemaTest, ConstructorWithColumns) {
    std::vector<Column> columns;
    columns.emplace_back("id", Column::ColumnType::INT, Column::INDEXED_FLAG);
    columns.emplace_back("email", Column::ColumnType::STRING);
    columns.emplace_back("age", Column::ColumnType::INT, Column::NOT_NULL_FLAG);

    Schema schema(columns, 100);

    EXPECT_EQ(schema.size(), 3);
    EXPECT_EQ(schema.null_bitmap_size(), 1);

    EXPECT_THROW(schema.get_bitmap_idx(0), std::runtime_error);
    EXPECT_EQ(schema.get_bitmap_idx(1), 0);
    EXPECT_THROW(schema.get_bitmap_idx(2), std::runtime_error);
}

TEST(SchemaTest, DuplicateColumnNames) {
    std::vector<Column> columns = {{"data", Column::ColumnType::INT, 0}, {"data", Column::ColumnType::STRING, 0}};
    EXPECT_THROW({ Schema s(columns); }, std::runtime_error);
}

TEST(SchemaTest, NullBitmapSizeCalculation) {
    Schema schema_0({{"c1", Column::ColumnType::INT, Column::NOT_NULL_FLAG}});
    EXPECT_EQ(schema_0.null_bitmap_size(), 0);

    std::vector<Column> columns;
    for (int i = 0; i < 8; ++i) {
        columns.push_back({std::to_string(i), Column::ColumnType::INT});
    }

    Schema schema_1(columns);
    EXPECT_EQ(schema_1.null_bitmap_size(), 1);

    columns.push_back({"9", Column::ColumnType::INT});
    Schema schema_2(columns);
    EXPECT_EQ(schema_2.null_bitmap_size(), 2);
}

TEST(SchemaTest, IncrementRecordId) {
    Schema schema;
    uint32_t initial = schema.record_id_count();
    schema.increment_record_id_count();
    EXPECT_EQ(schema.record_id_count(), initial + 1);
}

TEST(SchemaTest, FullCycleSerialization) {
    std::vector<Column> cols =
        {{"f1", Column::ColumnType::INT, Column::NOT_NULL_FLAG}, {"f2", Column::ColumnType::STRING, 0}};
    Schema original(cols, 5);

    std::stringstream ss;
    ASSERT_TRUE(original.to_binary(ss));

    ss.seekg(0);
    auto restored = Schema::from_binary(ss);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, original);
    EXPECT_EQ(restored->record_id_count(), 5);
}

TEST(SchemaTest, ReadsLegacySchemaWithoutDefaults) {
    std::stringstream ss;
    ss.write("SCHEMA", 6);

    uint32_t record_id_count = 3;
    uint32_t columns_len = 1;
    ss.write(reinterpret_cast<char*>(&record_id_count), sizeof(record_id_count));
    ss.write(reinterpret_cast<char*>(&columns_len), sizeof(columns_len));

    std::string name = "name";
    uint32_t name_len = name.size();
    auto type = Column::ColumnType::STRING;
    uint8_t flags = 0;
    ss.write(reinterpret_cast<char*>(&name_len), sizeof(name_len));
    ss.write(name.c_str(), name_len);
    ss.write(reinterpret_cast<char*>(&type), sizeof(type));
    ss.write(reinterpret_cast<char*>(&flags), sizeof(flags));

    ss.seekg(0);
    auto restored = Schema::from_binary(ss);

    ASSERT_TRUE(restored.has_value());
    ASSERT_EQ(restored->size(), 1);
    EXPECT_EQ(restored->record_id_count(), 3);
    EXPECT_FALSE((*restored)[0].has_default());
}

TEST(SchemaTest, FromBinaryInvalidHeader) {
    std::stringstream ss;
    ss << "WRONG_HEADER" << uint32_t(0) << uint32_t(0);
    auto res = Schema::from_binary(ss);
    EXPECT_FALSE(res.has_value());
}

}  // namespace qdb::storage::test
