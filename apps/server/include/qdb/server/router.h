#ifndef QUASARDB_ROUTER_H
#define QUASARDB_ROUTER_H

#include <qdb/core/response.h>
#include <qdb/server/ast.h>
#include <qdb/server/catalog.h>
#include <qdb/server/registry.h>

#include <memory>
#include <string>

namespace qdb::server {

class Router {
public:
    Router(std::shared_ptr<Registry> registry, Catalog& catalog);

public:
    static auto New(std::shared_ptr<Registry> registry, Catalog& catalog) -> std::unique_ptr<Router>;

public:
    auto Route(const Statement& statement) -> qdb::core::Response;

private:
    auto SendToStorage(const StorageId& id, const Statement& statement) -> qdb::core::Response;

    static auto TableKey(const TableRef& table) -> std::string;

private:
    std::shared_ptr<Registry> registry_;

    Catalog& catalog_;
};

}  // namespace qdb::server

#endif  // QUASARDB_ROUTER_H
