#include <qdb/core/thread_pool.h>

#include <utility>

namespace qdb::core {

ThreadPool::ThreadPool(std::size_t workers) {
    workers = workers == 0 ? 1 : workers;
    workers_.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this] { Run(); });
    }
}

ThreadPool::~ThreadPool() {
    Stop();
}

auto ThreadPool::Submit(std::function<void()> task) -> bool {
    {
        std::lock_guard lock(mutex_);
        if (stopped_) {
            return false;
        }
        tasks_.push(std::move(task));
    }
    ready_.notify_one();
    return true;
}

void ThreadPool::Stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
    }
    ready_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::Run() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
            if (stopped_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

}  // namespace qdb::core
