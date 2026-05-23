#ifndef QUASARDB_CATALOG_H
#define QUASARDB_CATALOG_H

#include <nlohmann/json.hpp>

#include <qdb/server/ast.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace qdb::server {

class Catalog {
public:
    explicit Catalog(std::filesystem::path path);

    auto CreateDatabase(const std::string& name) -> bool;
    auto DropDatabase(const std::string& name) -> std::vector<TableRef>;
    auto HasDatabase(const std::string& name) const -> bool;

    auto CreateTable(const CreateTableStmt& statement) -> bool;
    auto DropTable(const TableRef& table) -> bool;
    auto HasTable(const TableRef& table) const -> bool;
    auto ResolveTable(const TableRef& table) const -> std::optional<TableRef>;

private:
    static auto DatabaseName(const TableRef& table) -> std::string;

    auto Load() -> void;
    auto Save() const -> void;

private:
    std::filesystem::path path_;
    nlohmann::json data_;
};

}  // namespace qdb::server

#endif  // QUASARDB_CATALOG_H
