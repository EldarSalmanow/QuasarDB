#ifndef QUASARDB_REGISTRY_H
#define QUASARDB_REGISTRY_H

#include <memory>
#include <optional>
#include <filesystem>
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
    explicit Registry(bool auto_start_storage = true,
                      std::string storage_binary = "",
                      std::string storage_root = "data");

    ~Registry();

public:
    static auto New(bool auto_start_storage = true,
                    std::string storage_binary = "",
                    std::string storage_root = "data") -> std::shared_ptr<Registry>;

public:
    auto CreateNode(const StorageId &id) -> bool;

    auto GetNode(const StorageId &id) const -> std::optional<StorageNode>;

    auto HasNode(const StorageId &id) const -> bool;

    auto UpdateNode(const StorageId& id, StorageState state) -> bool;

    auto DropNode(const StorageId &id) -> bool;

    auto GetNodes() const -> std::vector<std::pair<StorageId, StorageNode>>;

private:
    auto HasNodeNonSync(const StorageId &id) const -> bool;

    auto StartProcessNonSync(const StorageId& id, const StorageNode& node) -> bool;

    auto StopProcessNonSync(const StorageId& id) -> void;

    auto DataDirFor(const StorageId& id) const -> std::filesystem::path;

    auto ResolveStorageBinary() const -> std::filesystem::path;

    static auto WaitUntilReady(const StorageNode& node) -> bool;

private:
    std::unordered_map<StorageId, StorageNode> nodes_;

    std::unordered_map<StorageId, int> processes_;

    mutable std::shared_mutex nodes_mutex_;

    std::uint32_t next_port_;

    bool auto_start_storage_;

    std::string storage_binary_;

    std::filesystem::path storage_root_;
};

}  // namespace qdb::server

#endif  // QUASARDB_REGISTRY_H
