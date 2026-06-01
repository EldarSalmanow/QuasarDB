#include <qdb/server/task_processor.h>

#include <openssl/rand.h>

#include <array>
#include <atomic>
#include <iomanip>
#include <sstream>

namespace qdb::server {

TaskProcessor::TaskProcessor(Handler handler, std::size_t workers) : handler_(std::move(handler)), pool_(workers) {}

auto TaskProcessor::Submit(std::unique_ptr<Statement> statement) -> std::string {
    auto id = GenerateId();
    {
        std::lock_guard lock(mutex_);
        tasks_[id] = TaskResult{id, TaskStatus::Pending, std::nullopt, std::nullopt};
    }

    std::shared_ptr<Statement> task(std::move(statement));
    if (!pool_.Submit([this, id, task = std::move(task)] {
            Update(id, TaskStatus::Running);
            try {
                Update(id, TaskStatus::Completed, handler_(*task));
            } catch (const std::exception& exception) {
                Update(id, TaskStatus::Failed, std::nullopt, exception.what());
            }
        }))
    {
        Update(id, TaskStatus::Failed, std::nullopt, "task processor stopped");
    }

    return id;
}

auto TaskProcessor::Get(const std::string& id) const -> std::optional<TaskResult> {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(id);
    return it == tasks_.end() ? std::nullopt : std::optional<TaskResult>(it->second);
}

auto TaskProcessor::Cancel(const std::string& id) -> bool {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end() || it->second.status == TaskStatus::Completed) {
        return false;
    }
    it->second.status = TaskStatus::Failed;
    it->second.error = "cancelled";
    return true;
}

void TaskProcessor::Stop() { pool_.Stop(); }

auto TaskProcessor::GenerateId() const -> std::string {
    std::array<unsigned char, 16> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        static std::atomic_uint64_t fallback{0};
        return "task-" + std::to_string(fallback.fetch_add(1, std::memory_order_relaxed));
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            output << '-';
        }
        auto byte = bytes[i];
        output << std::setw(2) << static_cast<int>(byte);
    }
    return output.str();
}

void TaskProcessor::Update(
    const std::string& id,
    TaskStatus status,
    std::optional<qdb::core::Response> response,
    std::optional<std::string> error
) {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end() || it->second.status == TaskStatus::Failed) {
        return;
    }
    it->second.status = status;
    it->second.response = std::move(response);
    it->second.error = std::move(error);
}

}  // namespace qdb::server
