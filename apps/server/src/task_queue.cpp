#include <qdb/server/task_queue.h>

#include <array>
#include <exception>
#include <random>
#include <utility>

namespace qdb::server {

namespace {

auto Pending(std::string message, nlohmann::json data = nlohmann::json::object()) -> qdb::core::Response {
    return qdb::core::ResponseBuilder::Pending()
        .Message(std::move(message))
        .Data(std::move(data))
        .Build();
}

auto Error(std::string message) -> qdb::core::Response {
    return qdb::core::ResponseBuilder::Error()
        .Message(std::move(message))
        .Build();
}

auto NewGuid() -> std::string {
    static constexpr char hex[] = "0123456789abcdef";

    std::array<unsigned char, 16> bytes{};
    std::random_device random;
    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(random());
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    std::string guid;
    guid.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            guid += '-';
        }
        guid += hex[bytes[i] >> 4];
        guid += hex[bytes[i] & 0x0F];
    }

    return guid;
}

}  // namespace

TaskQueue::TaskQueue(Handler handler, std::size_t worker_count)
        : handler_(std::move(handler)) {
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this] { Worker(); });
    }
}

TaskQueue::~TaskQueue() {
    Stop();
}

auto TaskQueue::Submit(std::unique_ptr<Statement> statement) -> std::string {
    auto id = NewGuid();
    Store(id, Pending("Operation is running in background", {{"task_id", id}}));

    {
        std::lock_guard lock(queue_mutex_);
        queue_.push(Task{id, std::move(statement)});
    }

    ready_.notify_one();
    return id;
}

auto TaskQueue::Get(const std::string& id) const -> std::optional<qdb::core::Response> {
    std::lock_guard lock(results_mutex_);

    auto result = results_.find(id);
    if (result == results_.end()) {
        return std::nullopt;
    }

    return result->second;
}

auto TaskQueue::Stop() -> void {
    {
        std::lock_guard lock(queue_mutex_);
        stopping_ = true;
    }

    ready_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

auto TaskQueue::Worker() -> void {
    while (true) {
        Task task;
        {
            std::unique_lock lock(queue_mutex_);
            ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) {
                return;
            }

            task = std::move(queue_.front());
            queue_.pop();
        }

        Store(task.id, Pending("Operation is still running", {{"task_id", task.id}}));

        try {
            Store(task.id, handler_(*task.statement));
        } catch (const std::exception& exception) {
            Store(task.id, Error(exception.what()));
        }
    }
}

auto TaskQueue::Store(std::string id, qdb::core::Response response) -> void {
    std::lock_guard lock(results_mutex_);
    results_[std::move(id)] = std::move(response);
}

}  // namespace qdb::server
