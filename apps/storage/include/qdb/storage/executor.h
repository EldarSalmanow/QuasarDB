#ifndef QUASARDB_EXECUTOR_H
#define QUASARDB_EXECUTOR_H

#include <qdb/server/ast.h>
#include <qdb/storage/table.h>

#include <nlohmann/json.hpp>

namespace qdb::storage {

class Executor {
public:
    Executor(Table& table, Interner& interner);

public:
    auto Execute(const qdb::server::Statement& statement) -> nlohmann::json;

private:
    auto SelectRows(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json;

    auto AggregateSelect(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json;

private:
    Table& table_;

    Interner& interner_;
};

}

#endif  // QUASARDB_EXECUTOR_H
