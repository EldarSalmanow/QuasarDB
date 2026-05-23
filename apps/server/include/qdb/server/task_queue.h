#ifndef QUASARDB_TASK_QUEUE_H
#define QUASARDB_TASK_QUEUE_H

#include <qdb/core/response.h>
#include <qdb/server/ast.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace qdb::server {

class TaskQueue {
public:
    using Handler = std::function<qdb::core::Response(const Statement&)>;

    explicit TaskQueue(Handler handler, std::size_t worker_count = 2);

    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue(TaskQueue&&) = delete;

    auto operator=(const TaskQueue&) -> TaskQueue& = delete;
    auto operator=(TaskQueue&&) -> TaskQueue& = delete;

public:
    auto Submit(std::unique_ptr<Statement> statement) -> std::string;

    auto Get(const std::string& id) const -> std::optional<qdb::core::Response>;

    auto Stop() -> void;

private:
    struct Task {
        std::string id;
        std::unique_ptr<Statement> statement;
    };

    auto Worker() -> void;

    auto Store(std::string id, qdb::core::Response response) -> void;

private:
    Handler handler_;

    mutable std::mutex results_mutex_;
    std::unordered_map<std::string, qdb::core::Response> results_;

    std::mutex queue_mutex_;
    std::condition_variable ready_;
    std::queue<Task> queue_;

    bool stopping_{false};
    std::vector<std::thread> workers_;
};

}  // namespace qdb::server

#endif  // QUASARDB_TASK_QUEUE_H
