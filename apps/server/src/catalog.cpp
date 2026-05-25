#include <qdb/server/catalog.h>

#include <fstream>

namespace qdb::server {

namespace {

auto TableName(const TableRef& table) -> const std::string& {
    return table.Table;
}

}  // namespace

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
    if (!std::filesystem::exists(path_)) {
        data_ = {{"databases", nlohmann::json::object()}};
        return;
    }

    std::ifstream input(path_);
    data_ = nlohmann::json::parse(input, nullptr, false);
    if (!data_.is_object() || !data_.contains("databases")) {
        data_ = {{"databases", nlohmann::json::object()}};
    }
}

auto Catalog::Save() const -> void {
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path());
    }
    const auto tmp = path_.string() + ".tmp";
    {
        std::ofstream output(tmp);
        output << data_.dump(2);
    }
    std::filesystem::rename(tmp, path_);
}

}  // namespace qdb::server
