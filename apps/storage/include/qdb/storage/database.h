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
    Database(std::string name, fs::path root) : name_(std::move(name)), root_(std::move(root)) {
        for (const auto& entry : std::filesystem::directory_iterator(root_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".schema") {
                // TODO: full path database name is ok?
                auto table = Table(entry.path().string(), root_, &interner_);

                tables_.emplace(table.name(), std::move(table));
            }
        }
    }

    Database(const Database& other) = delete;

    Database(Database&& other) noexcept = delete;

public:
    void CreateTable(const std::string& name, const Schema& schema) {
        if (tables_.find(name) != tables_.end()) {
            throw std::
                runtime_error("[ERROR in qdb::storage::Database]: Table with name '" + name + "' already exists!");
        }

        tables_.emplace(name, Table(name, root_, schema, &interner_));
    }

    void DropTable(const std::string& name) {
        auto table_iterator = tables_.find(name);

        if (table_iterator == tables_.end()) {
            throw std::
                runtime_error("[ERROR in qdb::storage::Database]: Table with name '" + name + "' does not exist!");
        }

        table_iterator->second.drop();

        tables_.erase(name);
    }

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