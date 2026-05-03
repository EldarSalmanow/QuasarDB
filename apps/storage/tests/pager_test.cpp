#include <gtest/gtest.h>
#include <filesystem>
#include "../include/qdb/storage/pager.h"

namespace fs = std::filesystem;

class PagerTest : public ::testing::Test {
protected:
    const std::string test_db = "test_database.db";
    const size_t PAGE_SIZE = 4096;

    void SetUp() override {
        if (fs::exists(test_db)) {
            fs::remove(test_db);
        }
    }

    void TearDown() override {
        if (fs::exists(test_db)) {
            fs::remove(test_db);
        }
    }
};

TEST_F(PagerTest, InitializesEmptyFile) {
    Pager pager(test_db, PAGE_SIZE);
    EXPECT_EQ(pager.get_total_pages(), 0);
    EXPECT_FALSE(pager.page_exists(0));
}

TEST_F(PagerTest, AppendsPages) {
    Pager pager(test_db, PAGE_SIZE);
    
    uint32_t page0 = pager.append_new_page();
    uint32_t page1 = pager.append_new_page();
    
    EXPECT_EQ(page0, 0);
    EXPECT_EQ(page1, 1);
    EXPECT_EQ(pager.get_total_pages(), 2);
    EXPECT_TRUE(pager.page_exists(0));
    EXPECT_TRUE(pager.page_exists(1));
}

TEST_F(PagerTest, WritesAndReadsData) {
    Pager pager(test_db, PAGE_SIZE);
    pager.append_new_page();

    std::vector<uint8_t> data_to_write(PAGE_SIZE, 0xAB);
    pager.write_page(0, data_to_write.data());

    std::vector<uint8_t> data_to_read(PAGE_SIZE);
    pager.read_page(0, data_to_read.data());

    EXPECT_EQ(data_to_read.size(), PAGE_SIZE);
    EXPECT_EQ(data_to_read, data_to_write);
}

TEST_F(PagerTest, PersistsDataAfterReopening) {
    {
        Pager pager(test_db, PAGE_SIZE);
        pager.append_new_page();
        std::vector<uint8_t> data(PAGE_SIZE, 0xCD);
        pager.write_page(0, data.data());
    }

    Pager pager_reopened(test_db, PAGE_SIZE);
    std::vector<uint8_t> read_data(PAGE_SIZE);
    pager_reopened.read_page(0, read_data.data());

    EXPECT_EQ(pager_reopened.get_total_pages(), 1);
    EXPECT_EQ(read_data[0], 0xCD);
}

TEST_F(PagerTest, ThrowsExceptionOnReadingNonExistentPage) {
    Pager pager(test_db, PAGE_SIZE);
    std::vector<uint8_t> data;
    EXPECT_THROW(pager.read_page(99, data.data()), std::runtime_error);
}
