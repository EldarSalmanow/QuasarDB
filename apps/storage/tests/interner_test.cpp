#include "../include/qdb/storage/interner.h"
#include <gtest/gtest.h>

namespace qdb::storage::test {

class InternerTest : public ::testing::Test {
protected:
    Interner interner;
};

TEST_F(InternerTest, InterningReturnsSamePointer) {
    std::string str1 = "hello";
    std::string str2 = "hello";

    std::string_view view1 = interner.intern(str1);
    std::string_view view2 = interner.intern(str2);

    EXPECT_EQ(view1.data(), view2.data());
    EXPECT_EQ(view1, view2);
}

TEST_F(InternerTest, StabilityAfterMultipleAppends) {
    std::string_view first = interner.intern("first");
    for (int i = 0; i < 1000; ++i) {
        interner.intern("string_" + std::to_string(i));
    }
    EXPECT_EQ(first, "first");
    EXPECT_EQ(interner.intern("first").data(), first.data());
}

TEST_F(InternerTest, StrToValueCreation) {
    ExternalString ext = {.offset = 100, .size = 5};
    Value val = interner.str_to_value("world", ext);

    EXPECT_TRUE(val.is_string());
    EXPECT_EQ(val.get_type(), Value::Type::STRING);
    EXPECT_EQ(val.to_string(), "world");

    InternedString interned_str = val.as_string();
    EXPECT_TRUE(interned_str.has_ext_addr);
    EXPECT_EQ(interned_str.ext_addr.offset, 100);
    EXPECT_EQ(interned_str.intern_view, "world");
}

TEST_F(InternerTest, StrictEquality) {
    Value v1 = interner.str_to_value("test");
    Value v2 = interner.str_to_value("test");
    Value v3 = interner.str_to_value("other");
    Value v_int = Value(42);
    Value v_null;

    EXPECT_TRUE(v1.StrictEq(v2));
    EXPECT_FALSE(v1.StrictEq(v3));
    EXPECT_FALSE(v1.StrictEq(v_int));
    EXPECT_FALSE(v1.StrictEq(v_null));
}

}  // namespace qdb::storage::test