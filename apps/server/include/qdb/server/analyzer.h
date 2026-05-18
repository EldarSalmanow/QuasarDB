#ifndef QUASARDB_ANALYZER_H
#define QUASARDB_ANALYZER_H

#include <qdb/server/ast.h>

#include <optional>

namespace qdb::server {

std::optional<TableRef> ExtractTableRef(const Statement& statement);

}  // namespace qdb::server

#endif  // QUASARDB_ANALYZER_H