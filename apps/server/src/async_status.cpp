#include <qdb/server/async_status.h>

#include <chrono>

namespace qdb::server {

TaskTracker::TaskTracker(Handler handler, std::size_t worker_count,
                         std::chrono::seconds cleanup_interval)
    : queue_(std::move(handler), worker_count)
    , cleanup_interval_(cleanup_interval) {
    cleanup_thread_ = std::thread([this] { CleanupLoop(); });
}

TaskTracker::~TaskTracker() {
    Stop();
}

auto TaskTracker::Submit(std::unique_ptr<Statement> statement) -> std::string {
    auto guid = queue_.Submit(std::move(statement));

    TaskResult entry;
    entry.guid = guid;
    entry.status = TaskStatus::Pending;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        results_[guid] = entry;
    }

    return guid;
}

auto TaskTracker::GetStatus(const std::string& guid) -> std::optional<TaskResult> {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = results_.find(guid);
        if (it != results_.end() &&
            (it->second.status == TaskStatus::Completed || it->second.status == TaskStatus::Failed))
        {
            return it->second;
        }

        auto base_result = queue_.Get(guid);
        if (!base_result.has_value()) {
            if (it != results_.end()) {
                if (it->second.status == TaskStatus::Pending) {
                    it->second.status = TaskStatus::Running;
                }
                return it->second;
            }
            return std::nullopt;
        }

        if (it == results_.end()) {
            TaskResult entry;
            entry.guid = guid;
            entry.result = base_result;
            if (base_result->IsPending()) {
                entry.status = TaskStatus::Running;
            } else {
                entry.status = base_result->IsSuccess() ? TaskStatus::Completed : TaskStatus::Failed;
                if (base_result->IsError()) {
                    entry.error = base_result->GetMessage();
                }
            }
            results_[guid] = entry;
            return entry;
        }

        it->second.result = base_result;
        if (base_result->IsPending()) {
            it->second.status = TaskStatus::Running;
        } else {
            it->second.status = base_result->IsSuccess() ? TaskStatus::Completed : TaskStatus::Failed;
            if (base_result->IsError()) {
                it->second.error = base_result->GetMessage();
            }
        }
        return it->second;
    }
}

auto TaskTracker::Cancel(const std::string& guid) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = results_.find(guid);
    if (it == results_.end()) return false;
    if (it->second.status == TaskStatus::Completed || it->second.status == TaskStatus::Failed) return false;
    it->second.status = TaskStatus::Failed;
    it->second.error = "cancelled";
    return true;
}

auto TaskTracker::Stop() -> void {
    running_.store(false, std::memory_order_relaxed);
    queue_.Stop();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

void TaskTracker::CleanupLoop() {
    while (running_.load(std::memory_order_relaxed)) {
        for (int i = 0; i < static_cast<int>(cleanup_interval_.count()); ++i) {
            if (!running_.load(std::memory_order_relaxed)) return;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = results_.begin(); it != results_.end(); ) {
            if (it->second.status == TaskStatus::Completed || it->second.status == TaskStatus::Failed) {
                it = results_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

}  // namespace qdb::server
