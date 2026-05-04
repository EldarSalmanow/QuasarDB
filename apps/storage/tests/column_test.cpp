#include "../include/qdb/storage/column.h"
#include <gtest/gtest.h>
#include <sstream>

namespace qdb::storage::test {

TEST(ColumnTest, ConstructorAndGetters) {
    Column column("username", Column::ColumnType::STRING, Column::NOT_NULL_FLAG);

    EXPECT_EQ(column.name(), "username");
    EXPECT_TRUE(column.is_string());
    EXPECT_FALSE(column.is_int());
    EXPECT_TRUE(column.not_null());
    EXPECT_FALSE(column.indexed());
}

TEST(ColumnTest, IndexedImplicitlyNotNull) {
    Column column("id", Column::ColumnType::INT, Column::INDEXED_FLAG);
    EXPECT_TRUE(column.indexed());
    EXPECT_TRUE(column.not_null());
}

TEST(ColumnTest, BinarySerialization) {
    Column original("price", Column::ColumnType::INT, Column::NOT_NULL_FLAG | Column::INDEXED_FLAG);

    std::stringstream ss;
    ASSERT_TRUE(original.to_binary(ss));

    ss.seekg(0);
    auto restored = Column::from_binary(ss);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(original, *restored);
    EXPECT_EQ(restored->name(), "price");
    EXPECT_TRUE(restored->not_null());
    EXPECT_TRUE(restored->indexed());
}

TEST(ColumnTest, FromBinaryCorruptedData) {
    std::stringstream ss;
    uint32_t fake_large_size = 2 * 1024 * 1024;
    ss.write(reinterpret_cast<char*>(&fake_large_size), sizeof(fake_large_size));

    auto res = Column::from_binary(ss);
    EXPECT_FALSE(res.has_value());
}

}  // namespace qdb::storage::test