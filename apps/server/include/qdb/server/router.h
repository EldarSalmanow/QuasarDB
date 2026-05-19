//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_ROUTER_H
#define QUASARDB_ROUTER_H

#include <qdb/server/ast.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace qdb::server {

struct StorageNode {
    std::uint64_t id{};
    std::string table;
    std::string address;
    bool alive{true};

    StorageNode() = default;

    StorageNode(std::uint64_t id, std::string table, std::string address, bool alive = true);
};

class Router {
public:
    Router() = default;

    explicit Router(std::vector<StorageNode> nodes);

public:
    auto AddNode(StorageNode node) -> void;

    auto CreateNode(const TableRef& table) -> StorageNode;

    auto DropNode(const TableRef& table) -> bool;

    auto Resolve(const TableRef& table) const -> std::optional<StorageNode>;

    auto Size() const -> std::size_t;

private:
    static auto TableKey(const TableRef& table) -> std::string;

private:
    std::vector<StorageNode> nodes_;

    std::unordered_map<std::string, std::size_t> table_to_node_;

    std::uint64_t next_node_id_{1};
};

}  // namespace qdb::server

#endif  // QUASARDB_ROUTER_H
