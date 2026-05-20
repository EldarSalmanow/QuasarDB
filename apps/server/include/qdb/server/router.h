#ifndef QUASARDB_ROUTER_H
#define QUASARDB_ROUTER_H

#include <qdb/server/ast.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_client.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace qdb::server {

struct StorageNode {
    std::uint64_t id{};
    std::string table;
    std::string host;
    std::uint32_t port;
    bool alive{true};
    std::unique_ptr<qdb::core::TcpClient> client;

    StorageNode();

    StorageNode(std::uint64_t id, std::string table, std::string host, std::uint32_t port, bool alive = true);
};

class Router {
public:
    Router() = default;

    explicit Router(std::vector<StorageNode> nodes);

public:
    auto Route(const Statement& statement) -> qdb::core::Response;

private:
    static auto TableKey(const TableRef& table) -> std::string;

private:
    auto CreateNode(const TableRef& table) -> StorageNode&;

    auto DropNode(const TableRef& table) -> bool;

    auto Resolve(const TableRef& table) -> StorageNode*;

    auto SendToStorage(StorageNode& node, const Statement& statement) -> qdb::core::Response;

private:
    std::vector<StorageNode> nodes_;

    std::unordered_map<std::string, std::size_t> table_to_node_;

    std::uint64_t next_node_id_{1};

    std::uint32_t next_port_{9001};
};

}  // namespace qdb::server

#endif  // QUASARDB_ROUTER_H
