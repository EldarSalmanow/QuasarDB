#ifndef QUASARDB_DATABASE_H
#define QUASARDB_DATABASE_H

#include "interner.h"
#include "table.h"

#include <filesystem>
#include <string>
#include <unordered_map>


namespace fs = std::filesystem;

namespace qdb::storage {

class Database final {
public:
    Database(std::string name, fs::path root);

    Database(const Database& other) = delete;

    Database(Database&& other) noexcept = delete;

public:
    void CreateTable(const std::string& name, const Schema& schema);

    void DropTable(const std::string& name);

public:
    Database& operator=(const Database& other) = delete;

    Database& operator=(Database&& other) noexcept = delete;

public:
    std::string name_;

    fs::path root_;

    std::unordered_map<std::string, Table> tables_;

    Interner interner_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_DATABASE_H