#include <qdb/server/registry.h>

#include <mutex>

namespace qdb::server {

StorageNode::StorageNode(std::string host, std::uint32_t port, StorageState state)
        : host(std::move(host)), port(port), state(state), client(qdb::core::TcpClient::New(host, port)) {}

Registry::Registry()
        : nodes_(), nodes_mutex_(), next_node_id_(), next_port_() {}

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

auto Registry::GetNode(const StorageId &id) -> std::optional<StorageNode &> {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNode(id)) {
        return std::nullopt;
    }

    return std::make_optional(nodes_[id]);
}

auto Registry::HasNode(const StorageId &id) -> bool {
    std::unique_lock lock(nodes_mutex_);

    return HasNodeNonSync(id);
}

auto Registry::UpdateNode(const StorageId &id, StorageState state) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNodeNonSync(id)) {
        return false;
    }

    nodes_[id].state = state;
    // nodes_[id].last_heartbeat = std::chrono::steady_clock::now();
}

auto Registry::DropNode(const StorageId &id) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNodeNonSync(id)) {
        return false;
    }

    nodes_.erase(id);

    return true;
}

auto Registry::GetNodes() const -> std::vector<StorageNode> {
    std::unique_lock lock(nodes_mutex_);

    std::vector<StorageNode> nodes;
    for (auto [id, node] : nodes_) {
        nodes.emplace_back(node);
    }

    return nodes;
}

auto Registry::HasNodeNonSync(const StorageId &id) -> bool {
    return nodes_.find(id) != nodes_.end();
}

}
