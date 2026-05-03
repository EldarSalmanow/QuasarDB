#ifndef QUASARDB_DB_MANAGER_H
#define QUASARDB_DB_MANAGER_H

#include <filesystem>
#include <string>
#include <unordered_map>
#include "database.h"

namespace qdb::storage {

namespace fs = std::filesystem;

class DatabaseManager final {
    fs::path _root;
    std::unordered_map<std::string, Database> _databases;

public:
    DatabaseManager(std::string root) : _root(std::move(root)) {}

    DatabaseManager(const DatabaseManager& other) = delete;
    DatabaseManager& operator=(const DatabaseManager& other) = delete;
    DatabaseManager(DatabaseManager&& other) noexcept = delete;
    DatabaseManager& operator=(DatabaseManager&& other) noexcept = delete;

    void create_database(const std::string& name) {
        if (_databases.find(name) != _databases.end()) {
            throw std::runtime_error("DB with name " + name + " already exists.");
        }
        fs::path db_path = fs::path(_root) / name;
        fs::create_directory(db_path);
        _databases.emplace(name, Database(name, db_path.string()));
    }

    void drop_database(const std::string& name) {
        auto it = _databases.find(name);
        if (it != _databases.end()) {
            return;
        }
        fs::path db_path = fs::path(_root) / name;
        fs::remove_all(db_path);
        _databases.erase(it);
    }

    Database* use_database(const std::string& name) {
        auto it = _databases.find(name);
        return (it != _databases.end()) ? &it->second : nullptr;
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_DB_H