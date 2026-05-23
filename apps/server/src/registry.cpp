#include <qdb/server/registry.h>

#include <mutex>
#include <utility>

namespace qdb::server {

StorageId::StorageId(std::string table)
        : table(std::move(table)) {}

auto StorageId::operator==(const StorageId& other) const -> bool {
    return table == other.table;
}

StorageNode::StorageNode(std::string host, std::uint32_t port, StorageState state)
        : host(std::move(host)), port(port), state(state) {}

Registry::Registry()
        : next_port_(9001) {}

auto Registry::New() -> std::shared_ptr<Registry> {
    return std::make_shared<Registry>();
}

auto Registry::CreateNode(const StorageId &id) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (HasNodeNonSync(id)) {
        return true;
    }

    nodes_[id] = {"127.0.0.1", next_port_++, StorageState::Unknown};

    return true;
}

auto Registry::GetNode(const StorageId &id) const -> std::optional<StorageNode> {
    std::shared_lock lock(nodes_mutex_);

    auto iterator = nodes_.find(id);

    if (iterator == nodes_.end()) {
        return std::nullopt;
    }

    return iterator->second;
}

auto Registry::HasNode(const StorageId &id) const -> bool {
    std::shared_lock lock(nodes_mutex_);

    return HasNodeNonSync(id);
}

auto Registry::UpdateNode(const StorageId &id, StorageState state) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNodeNonSync(id)) {
        return false;
    }

    nodes_[id].state = state;

    return true;
}

auto Registry::DropNode(const StorageId &id) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNodeNonSync(id)) {
        return false;
    }

    nodes_.erase(id);

    return true;
}

auto Registry::GetNodes() const -> std::vector<std::pair<StorageId, StorageNode>> {
    std::shared_lock lock(nodes_mutex_);

    std::vector<std::pair<StorageId, StorageNode>> nodes;
    nodes.reserve(nodes_.size());

    for (const auto& [id, node] : nodes_) {
        nodes.emplace_back(id, node);
    }

    return nodes;
}

auto Registry::HasNodeNonSync(const StorageId &id) const -> bool {
    return nodes_.find(id) != nodes_.end();
}

}  // namespace qdb::server
