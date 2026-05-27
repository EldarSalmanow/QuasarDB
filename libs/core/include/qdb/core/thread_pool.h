#ifndef QUASARDB_THREAD_POOL_H
#define QUASARDB_THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace qdb::core {

class ThreadPool final {
public:
    explicit ThreadPool(std::size_t workers = 2);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    auto operator=(const ThreadPool&) -> ThreadPool& = delete;
    auto operator=(ThreadPool&&) -> ThreadPool& = delete;

    auto Submit(std::function<void()> task) -> bool;
    void Stop();

private:
    void Run();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool stopped_{false};
};

}  // namespace qdb::core

#endif  // QUASARDB_THREAD_POOL_H
