#ifndef QUASARDB_SQL_BOOL_H
#define QUASARDB_SQL_BOOL_H

enum class SqlBool {
    FALSE,
    TRUE,
    UNKNOWN,
};

inline SqlBool operator&&(SqlBool a, SqlBool b) {
    if (a == SqlBool::FALSE || b == SqlBool::FALSE)
        return SqlBool::FALSE;
    if (a == SqlBool::UNKNOWN || b == SqlBool::UNKNOWN)
        return SqlBool::UNKNOWN;
    return SqlBool::TRUE;
}

inline SqlBool operator||(SqlBool a, SqlBool b) {
    if (a == SqlBool::TRUE || b == SqlBool::TRUE)
        return SqlBool::TRUE;
    if (a == SqlBool::UNKNOWN || b == SqlBool::UNKNOWN)
        return SqlBool::UNKNOWN;
    return SqlBool::FALSE;
}

inline SqlBool operator!(SqlBool a) {
    if (a == SqlBool::TRUE)
        return SqlBool::FALSE;
    if (a == SqlBool::FALSE)
        return SqlBool::TRUE;
    return SqlBool::UNKNOWN;
}

inline bool operator==(SqlBool a, SqlBool b) {
    return static_cast<int>(a) == static_cast<int>(b);
}

#endif  // QUASARDB_SQL_BOOL_H