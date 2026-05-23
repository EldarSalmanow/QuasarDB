#ifndef QUASARDB_REGISTRY_H
#define QUASARDB_REGISTRY_H

#include <qdb/core/tcp_client.h>

// #include <memory>
#include <shared_mutex>
#include <string>

namespace qdb::server {

struct StorageId {
    StorageId() = default;

    std::string table{};
};

enum class StorageState {
    Down,
    Up,
    Unknown
};

struct StorageNode {
    std::string host;
    std::uint32_t port;
    StorageState state;
    std::unique_ptr<qdb::core::TcpClient> client;

    StorageNode(std::string host, std::uint32_t port, StorageState state);
};

class Registry {
public:
    Registry();

public:
    static auto New() -> std::shared_ptr<Registry>;

public:
    auto CreateNode(const StorageId &id) -> bool;

    auto GetNode(const StorageId &id) -> std::optional<StorageNode &>;

    auto HasNode(const StorageId &id) -> bool;

    auto UpdateNode(const StorageId& id, StorageState state) -> bool;

    auto DropNode(const StorageId &id) -> bool;

    auto GetNodes() const -> std::vector<StorageNode>;

private:
    auto HasNodeNonSync(const StorageId &id) -> bool;

private:
    std::unordered_map<StorageId, StorageNode> nodes_;

    mutable std::shared_mutex nodes_mutex_;

    std::uint64_t next_node_id_{1};

    std::uint32_t next_port_{9001};
};

}

template<>
struct std::hash<qdb::server::StorageId> {
    size_t operator()(const qdb::server::StorageId &id) const noexcept {
        return std::hash<std::string>{}(id.table);
    }
};

#endif  // QUASARDB_REGISTRY_H
