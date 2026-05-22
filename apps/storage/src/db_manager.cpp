#include "../include/qdb/storage/db_manager.h"

namespace qdb::storage {

DatabaseManager::DatabaseManager(std::string root) : root_(std::move(root)) {}

void DatabaseManager::CreateDatabase(std::string name) {
    if (databases_.find(name) != databases_.end()) {
        throw std::
            runtime_error("[ERROR in qdb::storage::DatabaseManager]: DB with name '" + name + "' already exists!");
    }

    fs::path db_path = fs::path(root_) / name;

    fs::create_directory(db_path);

    databases_.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(name, db_path));
}

void DatabaseManager::DropDatabase(const std::string& name) {
    auto db_iterator = databases_.find(name);

    if (db_iterator == databases_.end()) {
        return;
    }

    fs::path db_path = fs::path(root_) / name;

    fs::remove_all(db_path);

    databases_.erase(db_iterator);
}

Database* DatabaseManager::UseDatabase(const std::string& name) {
    auto db_iterator = databases_.find(name);

    if (db_iterator == databases_.end()) {
        return nullptr;
    }

    return &db_iterator->second;
}

}  // namespace qdb::storage
