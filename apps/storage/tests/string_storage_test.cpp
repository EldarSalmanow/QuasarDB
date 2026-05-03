#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "../include/qdb/storage/table.h"

namespace qdb::storage::test {

namespace fs = std::filesystem;

class StringStorageTest : public ::testing::Test {
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

TEST_F(StringStorageTest, FileCreation) {
    StringStorage storage(test_path);
    EXPECT_TRUE(fs::exists(test_path));
    storage.delete_file();
}

TEST_F(StringStorageTest, AppendAndRead) {
    StringStorage storage(test_path);
    std::string original = "Hello, world!";

    auto addr = storage.append(original);

    EXPECT_EQ(addr.size, original.size());
    EXPECT_EQ(storage.read(addr), original);
    storage.delete_file();
}

TEST_F(StringStorageTest, MultipleAppends) {
    StringStorage storage(test_path);
    std::string s1 = "First";
    std::string s2 = "Second string";
    std::string s3 = "Third";

    auto addr1 = storage.append(s1);
    auto addr2 = storage.append(s2);
    auto addr3 = storage.append(s3);

    EXPECT_EQ(storage.read(addr1), s1);
    EXPECT_EQ(storage.read(addr2), s2);
    EXPECT_EQ(storage.read(addr3), s3);
    storage.delete_file();
}

TEST_F(StringStorageTest, Persistence) {
    ExternalString addr;
    std::string text = "Persistent Data";
    {
        StringStorage storage(test_path);
        addr = storage.append(text);
    }
    {
        StringStorage storage(test_path);
        EXPECT_EQ(storage.read(addr), text);
        storage.delete_file();
    }
}

TEST_F(StringStorageTest, MoveOperations) {
    StringStorage storage(test_path);
    auto addr = storage.append("MoveMe");

    StringStorage moved_storage(std::move(storage));
    EXPECT_EQ(moved_storage.read(addr), "MoveMe");

    StringStorage another_storage(test_path.string() + ".extra");
    another_storage = std::move(moved_storage);
    EXPECT_EQ(another_storage.read(addr), "MoveMe");

    if (fs::exists(test_path.string() + ".extra")) {
        fs::remove(test_path.string() + ".extra");
    }
    another_storage.delete_file();
}

TEST_F(StringStorageTest, ReadSizeMismatch) {
    StringStorage storage(test_path);
    auto addr = storage.append("Short");
    addr.size = 999;
    EXPECT_THROW(storage.read(addr), std::runtime_error);
    storage.delete_file();
}

TEST_F(StringStorageTest, EmptyString) {
    StringStorage storage(test_path);
    std::string empty = "";
    auto addr = storage.append(empty);
    EXPECT_EQ(addr.size, 0);
    EXPECT_EQ(storage.read(addr), "");
    storage.delete_file();
}

}  // namespace qdb::storage::test