#include "../include/qdb/storage/column.h"
#include <gtest/gtest.h>
#include <sstream>

namespace qdb::storage::test {

TEST(ColumnTest, ConstructorAndGetters) {
    Column column("username", Column::ColumnType::STRING, Column::NOT_NULL_FLAG);

    EXPECT_EQ(column.Name(), "username");
    EXPECT_TRUE(column.IsString());
    EXPECT_FALSE(column.IsInt());
    EXPECT_TRUE(column.IsNotNull());
    EXPECT_FALSE(column.IsIndexed());
}

TEST(ColumnTest, IndexedImplicitlyNotNull) {
    Column column("id", Column::ColumnType::INT, Column::INDEXED_FLAG);
    EXPECT_TRUE(column.IsIndexed());
    EXPECT_TRUE(column.IsNotNull());
}

TEST(ColumnTest, BinarySerialization) {
    Column original("price", Column::ColumnType::INT, Column::NOT_NULL_FLAG | Column::INDEXED_FLAG);

    std::stringstream ss;
    ASSERT_TRUE(original.ToBinary(ss));

    ss.seekg(0);
    auto restored = Column::FromBinary(ss);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
    EXPECT_EQ(restored->Name(), "price");
    EXPECT_TRUE(restored->IsNotNull());
    EXPECT_TRUE(restored->IsIndexed());
}

TEST(ColumnTest, DefaultBinarySerialization) {
    Column original("name", Column::ColumnType::STRING, Column::NOT_NULL_FLAG, Column::DefaultType::STRING, 0, "anon");

    std::stringstream ss;
    ASSERT_TRUE(original.ToBinary(ss));

    ss.seekg(0);
    auto restored = Column::FromBinary(ss);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
    EXPECT_TRUE(restored->HasDefault());
    EXPECT_EQ(restored->GetDefaultType(), Column::DefaultType::STRING);
}

TEST(ColumnTest, RejectsInvalidDefault) {
    EXPECT_THROW(
        Column("id", Column::ColumnType::INT, 0, Column::DefaultType::STRING, 0, "oops"),
        std::invalid_argument
    );
    EXPECT_THROW(
        Column("id", Column::ColumnType::INT, Column::NOT_NULL_FLAG, Column::DefaultType::NULL_VALUE),
        std::invalid_argument
    );
}

TEST(ColumnTest, RejectsOversizedDefaultStringOnSerialization) {
    std::string large_default(1024 * 1024 + 1, 'x');
    Column original("name", Column::ColumnType::STRING, 0, Column::DefaultType::STRING, 0, large_default);

    std::stringstream ss;
    EXPECT_FALSE(original.ToBinary(ss));
}

TEST(ColumnTest, FromBinaryCorruptedData) {
    std::stringstream ss;
    uint32_t fake_large_size = 2 * 1024 * 1024;
    ss.write(reinterpret_cast<char*>(&fake_large_size), sizeof(fake_large_size));

    auto res = Column::FromBinary(ss);
    EXPECT_FALSE(res.has_value());
}

}  // namespace qdb::storage::test
