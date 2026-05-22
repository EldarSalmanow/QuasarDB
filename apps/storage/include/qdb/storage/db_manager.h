#ifndef QUASARDB_DB_MANAGER_H
#define QUASARDB_DB_MANAGER_H

// #include <qdb/storage/database.h>
#include "database.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace qdb::storage {

class DatabaseManager final {
public:
    DatabaseManager(std::string root);

    DatabaseManager(const DatabaseManager& other) = delete;

    DatabaseManager(DatabaseManager&& other) noexcept = delete;

public:
    void CreateDatabase(std::string name);

    void DropDatabase(const std::string& name);

    Database* UseDatabase(const std::string& name);

public:
    DatabaseManager& operator=(const DatabaseManager& other) = delete;

    DatabaseManager& operator=(DatabaseManager&& other) noexcept = delete;

private:
    fs::path root_;

    std::unordered_map<std::string, Database> databases_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_DB_MANAGER_H