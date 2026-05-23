#ifndef QUASARDB_ASYNC_STATUS_H
#define QUASARDB_ASYNC_STATUS_H

#include <qdb/core/response.h>
#include <qdb/server/ast.h>
#include <qdb/server/task_queue.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace qdb::server {

enum class TaskStatus {
    Pending,
    Running,
    Completed,
    Failed
};

struct TaskResult {
    std::string guid;
    TaskStatus status;
    std::optional<qdb::core::Response> result;
    std::optional<std::string> error;
};

class TaskTracker {
public:
    using Handler = std::function<qdb::core::Response(const Statement&)>;

    explicit TaskTracker(Handler handler, std::size_t worker_count = 2,
                         std::chrono::seconds cleanup_interval = std::chrono::seconds(60));

    ~TaskTracker();

    auto Submit(std::unique_ptr<Statement> statement) -> std::string;

    auto GetStatus(const std::string& guid) -> std::optional<TaskResult>;

    auto Cancel(const std::string& guid) -> bool;

    auto Stop() -> void;

private:
    void CleanupLoop();

    TaskQueue queue_;
    std::unordered_map<std::string, TaskResult> results_;
    mutable std::mutex mutex_;

    std::thread cleanup_thread_;
    std::atomic<bool> running_{true};
    std::chrono::seconds cleanup_interval_;
};

}  // namespace qdb::server

#endif  // QUASARDB_ASYNC_STATUS_H
