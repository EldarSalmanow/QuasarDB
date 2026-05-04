#ifndef QUASARDB_DB_MANAGER_H
#define QUASARDB_DB_MANAGER_H

// #include <qdb/storage/database.h>
#include "database.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace qdb::storage {

class DatabaseManager final {
public:
    DatabaseManager(std::string root) : root_(std::move(root)) {}

public:
    DatabaseManager(const DatabaseManager& other) = delete;

    DatabaseManager(DatabaseManager&& other) noexcept = delete;

public:
    void CreateDatabase(const std::string& name) {
        if (databases_.find(name) != databases_.end()) {
            throw std::runtime_error("[ERROR] DB with name '" + name + "' already exists!");
        }

        fs::path db_path = fs::path(root_) / name;

        fs::create_directory(db_path);

        databases_.emplace(name, Database(name, db_path.string()));
    }

    void DropDatabase(const std::string& name) {
        auto db_iterator = databases_.find(name);

        if (db_iterator == databases_.end()) {
            return;
        }

        fs::path db_path = fs::path(root_) / name;

        fs::remove_all(db_path);

        databases_.erase(db_iterator);
    }

    Database* UseDatabase(const std::string& name) {
        auto db_iterator = databases_.find(name);

        if (db_iterator == databases_.end()) {
            return nullptr;
        }

        return &db_iterator->second;
    }

public:
    DatabaseManager& operator=(const DatabaseManager& other) = delete;

    DatabaseManager& operator=(DatabaseManager&& other) noexcept = delete;

private:
    fs::path root_;

    std::unordered_map<std::string, Database> databases_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_DB_MANAGER_H