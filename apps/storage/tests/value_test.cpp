#include "../include/qdb/storage/value.h"
#include <gtest/gtest.h>
#include "../include/qdb/storage/interner.h"

namespace qdb::storage::test {

TEST(ValueTest, TypeChecks) {
    Value n;
    Value i(42);

    EXPECT_TRUE(n.is_null());
    EXPECT_EQ(n.get_type(), Value::Type::NULL_TYPE);
    EXPECT_EQ(n.type_name(), "NULL");

    EXPECT_TRUE(i.is_int());
    EXPECT_EQ(i.get_type(), Value::Type::INT);
    EXPECT_EQ(i.as_int(), 42);
}

TEST(ValueTest, ToStringConversions) {
    Interner interner;
    Value n;
    Value i(-123);
    Value s = interner.str_to_value("demo");

    EXPECT_EQ(n.to_string(), "NULL");
    EXPECT_EQ(i.to_string(), "-123");
    EXPECT_EQ(s.to_string(), "demo");
}

TEST(ValueTest, ThrowOnWrongTypeAccess) {
    Value i(10);
    EXPECT_THROW(i.as_string(), std::runtime_error);

    Value n;
    EXPECT_THROW(n.as_int(), std::runtime_error);
}

TEST(ValueTest, SqlEqualityOperators) {
    Value i1(50);
    Value i2(50);
    Value i3(100);
    Value n;

    EXPECT_EQ(i1 == i2, SqlBool::TRUE);
    EXPECT_EQ(i1 == i3, SqlBool::FALSE);
    EXPECT_EQ(i1 == n, SqlBool::UNKNOWN);

    EXPECT_EQ(i1 != i3, SqlBool::TRUE);
    EXPECT_EQ(i1 != i2, SqlBool::FALSE);
    EXPECT_EQ(i1 != n, SqlBool::UNKNOWN);
}

TEST(ValueTest, SqlComparisonOperators) {
    Value low(10);
    Value high(20);
    Value n;

    EXPECT_EQ(low < high, SqlBool::TRUE);
    EXPECT_EQ(low <= high, SqlBool::TRUE);
    EXPECT_EQ(low <= low, SqlBool::TRUE);

    EXPECT_EQ(high > low, SqlBool::TRUE);
    EXPECT_EQ(high >= low, SqlBool::TRUE);
    EXPECT_EQ(high >= high, SqlBool::TRUE);

    EXPECT_EQ(low < n, SqlBool::UNKNOWN);
    EXPECT_EQ(low >= n, SqlBool::UNKNOWN);

    EXPECT_EQ(low >= high, SqlBool::FALSE);
    EXPECT_EQ(low > high, SqlBool::FALSE);
    EXPECT_EQ(low > low, SqlBool::FALSE);

    EXPECT_EQ(high <= low, SqlBool::FALSE);
    EXPECT_EQ(high < low, SqlBool::FALSE);
    EXPECT_EQ(high < high, SqlBool::FALSE);
}

TEST(ValueTest, StrictEquality) {
    Interner interner;
    Value s1 = interner.str_to_value("abc");
    Value s2 = interner.str_to_value("abc");
    Value n1;
    Value n2;

    EXPECT_TRUE(n1.StrictEq(n2));
    EXPECT_TRUE(s1.StrictEq(s2));
    EXPECT_FALSE(s1.StrictEq(n1));
}

TEST(ValueTest, CompareDifferentTypesThrows) {
    Value i(10);
    Interner interner;
    Value s = interner.str_to_value("10");

    EXPECT_THROW(i == s, std::runtime_error);
    EXPECT_THROW(i < s, std::runtime_error);
}

}  // namespace qdb::storage::test