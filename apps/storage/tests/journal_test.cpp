#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "../include/qdb/storage/interner.h"
#include "../include/qdb/storage/journal.h"
#include "../include/qdb/storage/record.h"
#include "../include/qdb/storage/schema.h"
#include "../include/qdb/storage/serializer.h"

namespace qdb::storage::test {

namespace fs = std::filesystem;

class JournalTest : public ::testing::Test {
protected:
    fs::path test_root;
    std::string table_name = "test_table";
    Schema test_schema;
    StringStorage* str_storage = nullptr;
    Interner interner;
    Serializer serializer = Serializer(&interner, 4096);

    void SetUp() override {
        const testing::TestInfo* const test_info = testing::UnitTest::GetInstance()->current_test_info();
        test_root = fs::temp_directory_path() / "JournalTest" / test_info->name();
        fs::create_directories(test_root);

        std::vector<Column> columns;
        columns.push_back(Column("column_1", Column::ColumnType::INT));
        test_schema = Schema(columns);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_root, ec);
    }
};

TEST_F(JournalTest, InitializationCreatesFile) {
    {
        Journal journal(test_root, table_name);
        fs::path journal_path = test_root / (table_name + ".jnl");
        EXPECT_TRUE(fs::exists(journal_path));
    }
}

TEST_F(JournalTest, RevertInsertion) {
    Journal journal(test_root, table_name);
    Record record(42, 0);
    std::string target_time = journal.save_insertion(record);
    auto revert_data = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_FALSE(revert_data.time.empty());
    EXPECT_EQ(revert_data.type, Journal::Track::Type::DELETE);
    EXPECT_EQ(revert_data.record.id(), 42);
}

TEST_F(JournalTest, RevertDeletion) {
    Journal journal(test_root, table_name);
    Record record(100, 0);
    std::string target_time = journal.save_deletion(record, test_schema, &serializer);
    auto revert_data = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_FALSE(revert_data.time.empty());
    EXPECT_EQ(revert_data.type, Journal::Track::Type::INSERT);
    EXPECT_EQ(revert_data.record.id(), 100);
}

TEST_F(JournalTest, RevertUpdation) {
    Journal journal(test_root, table_name);
    Record old_record(77, std::vector<Value>({Value(0)}));
    Record new_record(77, std::vector<Value>({Value(1)}));
    std::string target_time = journal.save_updation(old_record, test_schema, &serializer);
    auto revert_data = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_FALSE(revert_data.time.empty());
    EXPECT_EQ(revert_data.type, Journal::Track::Type::UPDATE);
    EXPECT_EQ(revert_data.record.id(), 77);
    EXPECT_EQ(revert_data.record[0], 0);
}

TEST_F(JournalTest, RevertRespectsTargetTime) {
    Journal journal(test_root, table_name);
    Record record(1, 0);
    journal.save_insertion(record);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string future_time = Journal::Track::get_now();
    auto revert_data = journal.revert_last(future_time, test_schema, str_storage, &serializer);
    EXPECT_TRUE(revert_data.time.empty());
}

TEST_F(JournalTest, AppendAfterReopen) {
    std::string target_time;
    {
        Journal journal(test_root, table_name);
        Record record_1(10, {Value(0)});
        target_time = journal.save_insertion(record_1);
    }
    Journal journal(test_root, table_name);
    Record record_2(20, {Value(1)});
    EXPECT_NO_THROW(journal.save_insertion(record_2));

    auto revert_2 = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_EQ(revert_2.record.id(), 20);

    auto revert_1 = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_EQ(revert_1.record.id(), 10);
}

TEST_F(JournalTest, TruncateBranchAfterRevertAndNewWrite) {
    Journal journal(test_root, table_name);
    Record record_1(10, {Value(0)});
    Record record_2(20, {Value(0)});
    std::string target_time = journal.save_insertion(record_1);
    journal.save_insertion(record_2);
    auto revert_2 = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_EQ(revert_2.record.id(), 20);
    Record record_3(30, {Value(0)});
    EXPECT_NO_THROW(journal.save_insertion(record_3));
    auto revert_3 = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_EQ(revert_3.record.id(), 30);
    auto revert_1 = journal.revert_last(target_time, test_schema, str_storage, &serializer);
    EXPECT_EQ(revert_1.record.id(), 10);
}

}  // namespace qdb::storage::test
