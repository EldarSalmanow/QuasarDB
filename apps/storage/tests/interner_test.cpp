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

    auto id1 = interner.Intern(str1).AsString();
    auto id2 = interner.Intern(str2).AsString();
    std::string_view view1 = interner.View(id1);
    std::string_view view2 = interner.View(id2);

    EXPECT_EQ(id1, id2);
    EXPECT_EQ(view1.data(), view2.data());
    EXPECT_EQ(view1, view2);
}

TEST_F(InternerTest, StabilityAfterMultipleAppends) {
    auto first_id = interner.Intern("first").AsString();
    std::string_view first = interner.View(first_id);
    for (int i = 0; i < 1000; ++i) {
        interner.Intern("string_" + std::to_string(i));
    }
    EXPECT_EQ(first, "first");
    EXPECT_EQ(interner.View(interner.Intern("first").AsString()).data(), first.data());
}

TEST_F(InternerTest, StrToValueCreation) {
    Value val = interner.Intern("world");

    EXPECT_TRUE(val.IsString());
    EXPECT_EQ(val.GetType(), Value::Type::STRING);
    EXPECT_EQ(interner.View(val.AsString()), "world");
    EXPECT_NE(val.AsString().value, 0);
}

TEST_F(InternerTest, StrictEquality) {
    Value v1 = interner.Intern("test");
    Value v2 = interner.Intern("test");
    Value v3 = interner.Intern("other");
    Value v_int = Value(42);
    Value v_null;

    EXPECT_TRUE(v1.StrictEq(v2));
    EXPECT_FALSE(v1.StrictEq(v3));
    EXPECT_FALSE(v1.StrictEq(v_int));
    EXPECT_FALSE(v1.StrictEq(v_null));
}

}  // namespace qdb::storage::test
