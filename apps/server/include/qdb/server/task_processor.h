#ifndef QUASARDB_TASK_PROCESSOR_H
#define QUASARDB_TASK_PROCESSOR_H

#include <qdb/core/response.h>
#include <qdb/core/thread_pool.h>
#include <qdb/server/ast.h>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace qdb::server {

enum class TaskStatus {
    Pending,
    Running,
    Completed,
    Failed,
};

struct TaskResult {
    std::string id;
    TaskStatus status{TaskStatus::Pending};
    std::optional<qdb::core::Response> response;
    std::optional<std::string> error;
};

class TaskProcessor final {
public:
    using Handler = std::function<qdb::core::Response(const Statement&)>;

    explicit TaskProcessor(Handler handler, std::size_t workers = 2);

    auto Submit(std::unique_ptr<Statement> statement) -> std::string;
    auto Get(const std::string& id) const -> std::optional<TaskResult>;
    auto Cancel(const std::string& id) -> bool;
    void Stop();

private:
    auto GenerateId() const -> std::string;
    void Update(const std::string& id, TaskStatus status, std::optional<qdb::core::Response> response = std::nullopt,
                std::optional<std::string> error = std::nullopt);

    Handler handler_;
    qdb::core::ThreadPool pool_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TaskResult> tasks_;
};

}  // namespace qdb::server

#endif  // QUASARDB_TASK_PROCESSOR_H
