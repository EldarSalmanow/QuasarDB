#ifndef QUASARDB_MONITOR_H
#define QUASARDB_MONITOR_H

#include <qdb/server/registry.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>

namespace qdb::server {

class Monitor {
public:
    Monitor();

    explicit Monitor(
        std::shared_ptr<Registry> registry,
        std::chrono::milliseconds interval = std::chrono::seconds(5),
        std::uint32_t dead_threshold = 3
    );

    ~Monitor();

    Monitor(const Monitor&) = delete;

    Monitor(Monitor&&) = delete;

    auto operator=(const Monitor&) -> Monitor& = delete;

    auto operator=(Monitor&&) -> Monitor& = delete;

public:
    static auto New() -> std::unique_ptr<Monitor>;

    static auto New(
        std::shared_ptr<Registry> registry,
        std::chrono::milliseconds interval = std::chrono::seconds(5),
        std::uint32_t dead_threshold = 3
    ) -> std::unique_ptr<Monitor>;

public:
    auto Start() -> void;

    auto Stop() -> void;

    auto ProbeOnce() -> void;

private:
    static auto PingStorage(const StorageNode& node) -> bool;

private:
    std::shared_ptr<Registry> registry_;

    std::chrono::milliseconds interval_;

    std::uint32_t dead_threshold_;

    std::unordered_map<StorageId, std::uint32_t> missed_heartbeats_;

    std::atomic_bool running_{false};

    std::thread worker_;
};

}  // namespace qdb::server

#endif  // QUASARDB_MONITOR_H
