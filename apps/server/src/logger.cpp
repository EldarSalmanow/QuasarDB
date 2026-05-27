#include <qdb/server/logger.h>

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace qdb::server {

Logger::Logger(std::string log_dir, std::uint64_t max_file_size)
    : log_dir_(std::move(log_dir)), max_file_size_(max_file_size) {
    std::filesystem::create_directories(log_dir_);
    writer_thread_ = std::thread([this] { WriteLoop(); });
}

Logger::~Logger() {
    running_.store(false, std::memory_order_relaxed);
    cv_.notify_one();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void Logger::LogQuery(const std::string& client_id, const std::string& handler_id,
                      const std::string& query, std::uint64_t duration_ms,
                      const std::string& status) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.client_id = client_id;
    entry.handler_id = handler_id;
    entry.query = query;
    entry.duration_ms = duration_ms;
    entry.status = status;

    pending_writes_.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(entry));
    }

    cv_.notify_one();
}

void Logger::Flush() {
    while (pending_writes_.load(std::memory_order_relaxed) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Logger::WriteLoop() {
    while (running_.load(std::memory_order_relaxed) || pending_writes_.load(std::memory_order_relaxed) > 0) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {
            return !queue_.empty() || !running_.load(std::memory_order_relaxed);
        });

        while (!queue_.empty()) {
            auto entry = std::move(queue_.front());
            queue_.pop();
            lock.unlock();

            RotateIfNeeded();
            if (current_file_.is_open()) {
                auto line = FormatEntry(entry);
                current_file_ << line << std::endl;
                current_file_size_.fetch_add(line.size() + 1, std::memory_order_relaxed);
            }

            pending_writes_.fetch_sub(1, std::memory_order_relaxed);

            lock.lock();
        }
    }

    if (current_file_.is_open()) {
        current_file_.close();
    }
}

void Logger::RotateIfNeeded() {
    if (!current_file_.is_open()) {
        current_file_.open(CurrentFilePath(), std::ios::app);
        return;
    }

    if (current_file_size_.load(std::memory_order_relaxed) >= max_file_size_) {
        current_file_.close();
        current_file_size_.store(0, std::memory_order_relaxed);
        ++file_index_;
        current_file_.open(CurrentFilePath(), std::ios::app);
    }
}

auto Logger::FormatEntry(const LogEntry& entry) -> std::string {
    auto tt = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()).count() % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%S.")
        << std::setw(3) << std::setfill('0') << ms << "Z"
        << " client_id=" << entry.client_id
        << " handler_id=" << entry.handler_id
        << " query=\"" << entry.query << "\""
        << " duration_ms=" << entry.duration_ms
        << " status=" << entry.status;
    return oss.str();
}

auto Logger::CurrentFilePath() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&tt);

    std::ostringstream oss;
    oss << log_dir_ << "/access_"
        << std::put_time(&tm, "%Y-%m-%d")
        << "_" << file_index_ << ".log";
    return oss.str();
}

}  // namespace qdb::server
