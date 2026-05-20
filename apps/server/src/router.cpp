#include <qdb/server/router.h>
#include <qdb/server/analyzer.h>

#include <algorithm>
#include <utility>

namespace qdb::server {

StorageNode::StorageNode() = default;

StorageNode::StorageNode(std::uint64_t node_id, std::string node_table, std::string node_host, std::uint32_t node_port, bool node_alive)
    : id(node_id), table(std::move(node_table)), host(std::move(node_host)), port(node_port), alive(node_alive) {}

Router::Router(std::vector<StorageNode> nodes) {
    for (auto& node : nodes) {
        const auto key = node.table;
        if (!key.empty() && !table_to_node_.contains(key)) {
            next_node_id_ = std::max(next_node_id_, node.id + 1);
            next_port_ = std::max(next_port_, node.port + 1);
            table_to_node_[key] = nodes_.size();
            nodes_.push_back(std::move(node));
        }
    }
}

auto Router::Route(const Statement& statement) -> qdb::core::Response {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) {
        auto& node = CreateNode(create->Table);
        return SendToStorage(node, statement);
    }

    if (const auto* drop = dynamic_cast<const DropTableStmt*>(&statement)) {
        auto node = Resolve(drop->Table);
        if (!node) {
            return qdb::core::ResponseBuilder::Error().Message("No storage node for table").Build();
        }
        auto response = SendToStorage(*node, statement);
        DropNode(drop->Table);
        return response;
    }

    auto table = Analyzer::TableFromStatement(statement);
    if (!table.has_value()) {
        return qdb::core::ResponseBuilder::Error().Message("Cannot determine table from statement").Build();
    }

    auto node = Resolve(table.value());
    if (!node) {
        return qdb::core::ResponseBuilder::Error().Message("No storage node for table").Build();
    }

    return SendToStorage(*node, statement);
}

auto Router::CreateNode(const TableRef& table) -> StorageNode& {
    const auto key = TableKey(table);
    if (auto existing = Resolve(table)) {
        return *existing;
    }

    StorageNode node(next_node_id_++, key, "127.0.0.1", next_port_++);
    node.client = qdb::core::TcpClient::New(node.host, node.port);

    table_to_node_[key] = nodes_.size();
    nodes_.push_back(std::move(node));
    return nodes_.back();
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

auto Router::Resolve(const TableRef& table) -> StorageNode* {
    const auto it = table_to_node_.find(TableKey(table));
    if (it == table_to_node_.end()) {
        return nullptr;
    }

    auto& node = nodes_[it->second];
    if (!node.alive) {
        return nullptr;
    }

    return &node;
}

auto Router::SendToStorage(StorageNode& node, const Statement& statement) -> qdb::core::Response {
    if (!node.client) {
        node.client = qdb::core::TcpClient::New(node.host, node.port);
    }

    if (!node.client->IsConnected() && !node.client->Connect()) {
        node.alive = false;
        return qdb::core::ResponseBuilder::Error().Message("Failed to connect to storage node").Build();
    }

    nlohmann::json request = {
        {"action", "execute_ast"},
        {"data", {
            {"ast_root", SerializeAst(statement)}
        }}
    };

    if (!node.client->Send(request)) {
        node.alive = false;
        return qdb::core::ResponseBuilder::Error().Message("Failed to send request to storage").Build();
    }

    auto response = node.client->Receive();

    if (!response.has_value()) {
        node.alive = false;
        return qdb::core::ResponseBuilder::Error().Message("Failed to receive response from storage").Build();
    }

    return qdb::core::Response::FromJsonObject(response.value());
}

auto Router::TableKey(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

}  // namespace qdb::server
