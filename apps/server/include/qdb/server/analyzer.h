#ifndef QUASARDB_ANALYZER_H
#define QUASARDB_ANALYZER_H

#include <qdb/server/ast.h>

#include <optional>

namespace qdb::server {

class Analyzer {
public:
    static auto ValidateStatement(const Statement& statement) -> void;

    static auto TableFromStatement(const Statement& statement) -> std::optional<TableRef>;
};

}  // namespace qdb::server

#endif  // QUASARDB_ANALYZER_H
