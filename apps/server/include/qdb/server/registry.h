#ifndef QUASARDB_REGISTRY_H
#define QUASARDB_REGISTRY_H

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace qdb::server {

struct StorageId {
    StorageId() = default;

    explicit StorageId(std::string table);

    std::string table;

    auto operator==(const StorageId& other) const -> bool;
};

}  // namespace qdb::server

template<>
struct std::hash<qdb::server::StorageId> {
    auto operator()(const qdb::server::StorageId& id) const noexcept -> std::size_t {
        return std::hash<std::string>{}(id.table);
    }
};

namespace qdb::server {

enum class StorageState {
    Down,
    Up,
    Unknown
};

struct StorageNode {
    std::string host;
    std::uint32_t port;
    StorageState state;

    StorageNode() = default;

    StorageNode(std::string host, std::uint32_t port, StorageState state = StorageState::Unknown);
};

class Registry {
public:
    Registry();

public:
    static auto New() -> std::shared_ptr<Registry>;

public:
    auto CreateNode(const StorageId &id) -> bool;

    auto GetNode(const StorageId &id) const -> std::optional<StorageNode>;

    auto HasNode(const StorageId &id) const -> bool;

    auto UpdateNode(const StorageId& id, StorageState state) -> bool;

    auto DropNode(const StorageId &id) -> bool;

    auto GetNodes() const -> std::vector<std::pair<StorageId, StorageNode>>;

private:
    auto HasNodeNonSync(const StorageId &id) const -> bool;

private:
    std::unordered_map<StorageId, StorageNode> nodes_;

    mutable std::shared_mutex nodes_mutex_;

    std::uint32_t next_port_;
};

}  // namespace qdb::server

#endif  // QUASARDB_REGISTRY_H
