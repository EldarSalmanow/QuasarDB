#include <qdb/server/router.h>

#include <algorithm>
#include <utility>

namespace qdb::server {

StorageNode::StorageNode() = default;

StorageNode::StorageNode(std::uint64_t node_id, std::string node_table, std::string node_address, bool node_alive)
    : id(node_id), table(std::move(node_table)), address(std::move(node_address)), alive(node_alive) {}

Router::Router(std::vector<StorageNode> nodes) {
    for (auto& node : nodes) {
        AddNode(std::move(node));
    }
}

auto Router::AddNode(StorageNode node) -> void {
    if (node.table.empty() || table_to_node_.contains(node.table)) {
        return;
    }

    next_node_id_ = std::max(next_node_id_, node.id + 1);
    table_to_node_[node.table] = nodes_.size();
    nodes_.push_back(std::move(node));
}

auto Router::CreateNode(const TableRef& table) -> StorageNode {
    const auto key = TableKey(table);
    if (auto existing = Resolve(table); existing.has_value()) {
        return existing.value();
    }

    StorageNode node(next_node_id_++, key, "storage://" + key);
    table_to_node_[key] = nodes_.size();
    nodes_.push_back(node);
    return node;
}

auto Router::DropNode(const TableRef& table) -> bool {
    const auto key = TableKey(table);
    auto it = table_to_node_.find(key);
    if (it == table_to_node_.end()) {
        return false;
    }

    nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(it->second));
    table_to_node_.clear();
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        table_to_node_[nodes_[index].table] = index;
    }

    return true;
}

auto Router::Resolve(const TableRef& table) const -> std::optional<StorageNode> {
    const auto it = table_to_node_.find(TableKey(table));
    if (it == table_to_node_.end()) {
        return std::nullopt;
    }

    const auto& node = nodes_[it->second];
    if (!node.alive) {
        return std::nullopt;
    }

    return node;
}

auto Router::Size() const -> std::size_t {
    return nodes_.size();
}

auto Router::TableKey(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

}  // namespace qdb::server
