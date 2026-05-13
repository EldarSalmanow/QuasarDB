#ifndef QUASARDB_LOGGER_H
#define QUASARDB_LOGGER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace qdb::server {

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string client_id;
    std::string handler_id;
    std::string query;
    std::uint64_t duration_ms;
    std::string status;
};

class Logger {
public:
    explicit Logger(std::string log_dir, std::uint64_t max_file_size = 100 * 1024 * 1024);
    ~Logger();

    void LogQuery(const std::string& client_id, const std::string& handler_id,
                  const std::string& query, std::uint64_t duration_ms,
                  const std::string& status);

    void Flush();

private:
    void WriteLoop();
    void RotateIfNeeded();
    auto FormatEntry(const LogEntry& entry) -> std::string;
    auto CurrentFilePath() -> std::string;

    std::string log_dir_;
    std::uint64_t max_file_size_;
    std::atomic<std::uint64_t> current_file_size_{0};

    std::queue<LogEntry> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread writer_thread_;
    std::atomic<bool> running_{true};

    std::ofstream current_file_;
    int file_index_{0};
};

}  // namespace qdb::server

#endif  // QUASARDB_LOGGER_H
