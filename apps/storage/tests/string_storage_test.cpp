#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "../include/qdb/storage/interner.h"

namespace qdb::storage::test {

namespace fs = std::filesystem;

class InternerStorageTest : public ::testing::Test {
protected:
    fs::path test_path = "test_strings.bin";

    void SetUp() override {
        if (fs::exists(test_path)) {
            fs::remove(test_path);
        }
    }

    void TearDown() override {
        if (fs::exists(test_path)) {
            fs::remove(test_path);
        }
    }
};

TEST_F(InternerStorageTest, FileCreation) {
    Interner interner;
    interner.UseStorage(test_path);
    EXPECT_TRUE(fs::exists(test_path));
    interner.DeleteStorage();
}

TEST_F(InternerStorageTest, AppendAndRead) {
    Interner interner;
    interner.UseStorage(test_path);
    std::string original = "Hello, world!";

    auto id = interner.Intern(original).AsString();

    EXPECT_EQ(interner.View(id), original);
    interner.DeleteStorage();
}

TEST_F(InternerStorageTest, MultipleAppends) {
    Interner interner;
    interner.UseStorage(test_path);
    std::string s1 = "First";
    std::string s2 = "Second string";
    std::string s3 = "Third";

    auto id1 = interner.Intern(s1).AsString();
    auto id2 = interner.Intern(s2).AsString();
    auto id3 = interner.Intern(s3).AsString();

    EXPECT_EQ(interner.View(id1), s1);
    EXPECT_EQ(interner.View(id2), s2);
    EXPECT_EQ(interner.View(id3), s3);
    interner.DeleteStorage();
}

TEST_F(InternerStorageTest, Persistence) {
    StringId id;
    std::string text = "Persistent Data";
    {
        Interner interner;
        interner.UseStorage(test_path);
        id = interner.Intern(text).AsString();
    }
    {
        Interner interner;
        interner.UseStorage(test_path);
        EXPECT_EQ(interner.View(id), text);
        interner.DeleteStorage();
    }
}

TEST_F(InternerStorageTest, MoveOperations) {
    Interner interner;
    interner.UseStorage(test_path);
    auto id = interner.Intern("MoveMe").AsString();

    Interner moved_interner(std::move(interner));
    EXPECT_EQ(moved_interner.View(id), "MoveMe");

    Interner another_interner;
    another_interner = std::move(moved_interner);
    EXPECT_EQ(another_interner.View(id), "MoveMe");

    another_interner.DeleteStorage();
}

TEST_F(InternerStorageTest, MissingIdThrows) {
    Interner interner;
    interner.UseStorage(test_path);
    interner.Intern("Short");
    EXPECT_THROW(interner.View(StringId{999}), std::runtime_error);
    interner.DeleteStorage();
}

TEST_F(InternerStorageTest, EmptyString) {
    Interner interner;
    interner.UseStorage(test_path);
    std::string empty = "";
    auto id = interner.Intern(empty).AsString();
    EXPECT_EQ(interner.View(id), "");
    interner.DeleteStorage();
}

}  // namespace qdb::storage::test
