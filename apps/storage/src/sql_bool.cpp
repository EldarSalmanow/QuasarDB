#include "../include/qdb/storage/sql_bool.h"

namespace qdb::storage {

SqlBool operator&&(SqlBool a, SqlBool b) {
    if (a == SqlBool::FALSE || b == SqlBool::FALSE) return SqlBool::FALSE;
    if (a == SqlBool::UNKNOWN || b == SqlBool::UNKNOWN) return SqlBool::UNKNOWN;
    return SqlBool::TRUE;
}

SqlBool operator||(SqlBool a, SqlBool b) {
    if (a == SqlBool::TRUE || b == SqlBool::TRUE) return SqlBool::TRUE;
    if (a == SqlBool::UNKNOWN || b == SqlBool::UNKNOWN) return SqlBool::UNKNOWN;
    return SqlBool::FALSE;
}

SqlBool operator!(SqlBool a) {
    if (a == SqlBool::TRUE) return SqlBool::FALSE;
    if (a == SqlBool::FALSE) return SqlBool::TRUE;
    return SqlBool::UNKNOWN;
}

bool operator==(SqlBool a, SqlBool b) { return static_cast<int>(a) == static_cast<int>(b); }

}  // namespace qdb::storage
