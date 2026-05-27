#include <qdb/server/catalog.h>

#include <qdb/core/json_file.h>

namespace qdb::server {

auto TableName(const TableRef& table) -> const std::string& {
    return table.Table;
}

Catalog::Catalog(std::filesystem::path path) : path_(std::move(path)) {
    Load();
}

auto Catalog::CreateDatabase(const std::string& name) -> bool {
    auto& databases = data_["databases"];
    if (databases.contains(name)) {
        return false;
    }
    databases[name] = {{"tables", nlohmann::json::object()}};
    Save();
    return true;
}

auto Catalog::DropDatabase(const std::string& name) -> std::vector<TableRef> {
    std::vector<TableRef> tables;
    auto& databases = data_["databases"];
    if (!databases.contains(name)) {
        return tables;
    }

    for (const auto& [table, _] : databases[name]["tables"].items()) {
        tables.emplace_back(name, table);
    }
    databases.erase(name);
    Save();
    return tables;
}

auto Catalog::HasDatabase(const std::string& name) const -> bool {
    return data_.contains("databases") && data_["databases"].contains(name);
}

auto Catalog::CreateTable(const CreateTableStmt& statement) -> bool {
    const auto db = DatabaseName(statement.Table);
    if (!HasDatabase(db)) {
        return false;
    }

    auto& tables = data_["databases"][db]["tables"];
    const auto& table = TableName(statement.Table);
    if (tables.contains(table)) {
        return false;
    }

    tables[table] = SerializeAst(statement);
    Save();
    return true;
}

auto Catalog::DropTable(const TableRef& table) -> bool {
    const auto resolved = ResolveTable(table);
    if (!resolved.has_value()) {
        return false;
    }

    auto& tables = data_["databases"][resolved->Database]["tables"];
    tables.erase(resolved->Table);
    Save();
    return true;
}

auto Catalog::HasTable(const TableRef& table) const -> bool {
    return ResolveTable(table).has_value();
}

auto Catalog::ResolveTable(const TableRef& table) const -> std::optional<TableRef> {
    const auto db = DatabaseName(table);
    if (!data_.contains("databases") || !data_["databases"].contains(db)) {
        return std::nullopt;
    }
    if (!data_["databases"][db]["tables"].contains(table.Table)) {
        return std::nullopt;
    }
    return TableRef(db, table.Table);
}

auto Catalog::DatabaseName(const TableRef& table) -> std::string {
    return table.Database;
}

auto Catalog::Load() -> void {
    data_ = qdb::core::JsonFile(path_).Load({{"databases", nlohmann::json::object()}});
    if (!data_.is_object() || !data_.contains("databases")) {
        data_ = {{"databases", nlohmann::json::object()}};
    }
}

auto Catalog::Save() const -> void {
    qdb::core::JsonFile(path_).Save(data_);
}

}  // namespace qdb::server
