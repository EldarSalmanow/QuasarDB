#include <qdb/server/logger.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace qdb::server {

TEST(LoggerTest, LogAndFlush) {
    auto test_dir = "test_logs_flush";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    {
        Logger logger(test_dir);
        logger.LogQuery("client1", "handler1", "SELECT 1", 5, "ok");
        logger.LogQuery("client2", "handler2", "INSERT INTO t VALUES (1)", 10, "ok");
        logger.Flush();

        bool found = false;
        for (const auto& entry : std::filesystem::directory_iterator(test_dir)) {
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("SELECT 1") != std::string::npos) found = true;
            }
        }
        ASSERT_TRUE(found);
    }

    std::filesystem::remove_all(test_dir);
}

TEST(LoggerTest, LogFormat) {
    auto test_dir = "test_logs_format";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    {
        Logger logger(test_dir);
        logger.LogQuery("alice", "w1", "SHOW TABLES", 42, "ok");
        logger.Flush();

        for (const auto& entry : std::filesystem::directory_iterator(test_dir)) {
            std::ifstream file(entry.path());
            std::string line;
            std::getline(file, line);
            ASSERT_NE(line.find("client_id=alice"), std::string::npos);
            ASSERT_NE(line.find("handler_id=w1"), std::string::npos);
            ASSERT_NE(line.find("SHOW TABLES"), std::string::npos);
            ASSERT_NE(line.find("duration_ms=42"), std::string::npos);
            ASSERT_NE(line.find("status=ok"), std::string::npos);
        }
    }

    std::filesystem::remove_all(test_dir);
}

TEST(LoggerTest, FileRotation) {
    auto test_dir = "test_logs_rotation";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    {
        Logger logger(test_dir, 50);
        for (int i = 0; i < 100; ++i) {
            logger.LogQuery("u", "h", "x", 1, "ok");
        }
        logger.Flush();

        int file_count = 0;
        for (const auto& _ : std::filesystem::directory_iterator(test_dir)) {
            ++file_count;
        }
        ASSERT_GE(file_count, 2);
    }

    std::filesystem::remove_all(test_dir);
}

TEST(LoggerTest, TimestampFormat) {
    auto test_dir = "test_logs_ts";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    {
        Logger logger(test_dir);
        logger.LogQuery("c", "h", "q", 1, "ok");
        logger.Flush();

        for (const auto& entry : std::filesystem::directory_iterator(test_dir)) {
            std::ifstream file(entry.path());
            std::string line;
            std::getline(file, line);
            ASSERT_TRUE(line.front() == '[' || std::isdigit(line[0]) || line[0] == '2');
        }
    }

    std::filesystem::remove_all(test_dir);
}

}  // namespace qdb::server
