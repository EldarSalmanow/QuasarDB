#ifndef QUASARDB_VALUE_H
#define QUASARDB_VALUE_H

#include "string_storage.h"

#include <cassert>
#include <string>
#include <variant>


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

struct InternedString {
    ExternalString ext_addr;
    std::string_view intern_view;
    bool has_ext_addr;
};

class Value final {
private:
    std::variant<std::nullptr_t, int32_t, InternedString> _data;

public:
    friend class Interner;
    enum class Type {
        NULL_TYPE,
        INT,
        STRING,
    };

    Value();
    Value(int32_t val);

private:
    Value(InternedString val);

public:
    bool is_null() const;

    bool is_int() const;

    bool is_string() const;

    Type get_type() const;

    std::string type_name() const;

    int32_t as_int() const;

    InternedString as_string() const;

    std::string to_string() const;

    bool StrictEq(const Value& other) const;

    SqlBool operator==(const Value& other) const;

    SqlBool operator<(const Value& other) const;

    SqlBool operator<=(const Value& other) const;

    SqlBool operator>(const Value& other) const;

    SqlBool operator>=(const Value& other) const;

    SqlBool operator!=(const Value& other) const;

    friend std::ostream& operator<<(std::ostream& os, const Value& value);
};

}  // namespace qdb::storage

#endif  // QUASARDB_VALUE_H