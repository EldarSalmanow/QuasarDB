#ifndef QUASARDB_SQL_BOOL_H
#define QUASARDB_SQL_BOOL_H

namespace qdb::storage {

enum class SqlBool {
    FALSE,
    TRUE,
    UNKNOWN,
};

SqlBool operator&&(SqlBool a, SqlBool b);

SqlBool operator||(SqlBool a, SqlBool b);

SqlBool operator!(SqlBool a);

bool operator==(SqlBool a, SqlBool b);

}  // namespace qdb::storage

#endif  // QUASARDB_SQL_BOOL_H