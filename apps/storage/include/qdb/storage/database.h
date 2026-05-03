//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_DATABASE_H
#define QUASARDB_DATABASE_H

#include <filesystem>
#include <string>
#include <unordered_map>
#include "interner.h"
#include "table.h"

namespace qdb::storage {

namespace fs = std::filesystem;

class Database final {
    std::string _name;
    fs::path _root;
    std::unordered_map<std::string, Table> _tables;
    Interner _interner;

public:
    Database(std::string name, fs::path root) : _name(std::move(name)), _root(std::move(root)) {
        for (const auto& entry : std::filesystem::directory_iterator(_root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".schema") {
                auto table = Table(entry.path().string(), _root, &_interner);
                _tables.emplace(table.name(), std::move(table));
            }
        }
    }

    Database(const Database& other) = delete;
    Database& operator=(const Database& other) = delete;
    Database(Database&& other) noexcept = delete;
    Database& operator=(Database&& other) noexcept = delete;

    void create_table(const std::string& name, const Schema& schema) {
        if (_tables.find(name) != _tables.end()) {
            throw std::runtime_error("Table with name " + name + " already exists.");
        }
        _tables.emplace(name, Table(name, _root, schema, &_interner));
    }

    void drop_table(const std::string& name) {
        auto it = _tables.find(name);
        if (it == _tables.end()) {
            throw std::runtime_error("Table with name " + name + " is not exists.");
        }
        it->second.drop();
        _tables.erase(name);
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_DATABASE_H