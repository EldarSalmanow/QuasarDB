#include "../include/qdb/storage/database.h"

namespace qdb::storage {

Database::Database(std::string name, fs::path root) : name_(std::move(name)), root_(std::move(root)) {
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".schema") {
            // TODO: full path database name is ok?
            auto table = Table(entry.path().string(), root_, &interner_);

            tables_.emplace(table.name(), std::move(table));
        }
    }
}

void Database::CreateTable(const std::string& name, const Schema& schema) {
    if (tables_.find(name) != tables_.end()) {
        throw std::runtime_error("[ERROR in qdb::storage::Database]: Table with name '" + name + "' already exists!");
    }

    tables_.emplace(name, Table(name, root_, schema, &interner_));
}

void Database::DropTable(const std::string& name) {
    auto table_iterator = tables_.find(name);

    if (table_iterator == tables_.end()) {
        throw std::runtime_error("[ERROR in qdb::storage::Database]: Table with name '" + name + "' does not exist!");
    }

    table_iterator->second.drop();

    tables_.erase(name);
}

}  // namespace qdb::storage
