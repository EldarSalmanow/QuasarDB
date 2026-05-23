#ifndef QUASARDB_ROUTER_H
#define QUASARDB_ROUTER_H

#include <qdb/server/ast.h>
#include <qdb/server/registry.h>

#include <memory>
#include <string>


namespace qdb::server {

class Router {
public:
    explicit Router(std::shared_ptr<Registry> registry);

public:
    static auto New(std::shared_ptr<Registry> registry) -> std::unique_ptr<Router>;

public:
    auto Route(const Statement& statement) -> qdb::core::Response;

private:
    auto Resolve(const Statement &statement) -> std::optional<StorageId>;

    static auto TableKey(const TableRef& table) -> std::string;

private:
    std::shared_ptr<Registry> registry_;
};

}  // namespace qdb::server

#endif  // QUASARDB_ROUTER_H
