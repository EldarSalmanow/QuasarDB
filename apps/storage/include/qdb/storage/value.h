#ifndef QUASARDB_VALUE_H
#define QUASARDB_VALUE_H

#include "external_string.h"
#include "sql_bool.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace qdb::storage {

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

    Value() : _data(nullptr) {}
    Value(int32_t val) : _data(val) {}

private:
    Value(InternedString val) : _data(val) {}

public:
    bool is_null() const { return std::holds_alternative<std::nullptr_t>(_data); }

    bool is_int() const { return std::holds_alternative<int32_t>(_data); }

    bool is_string() const { return std::holds_alternative<InternedString>(_data); }

    Type get_type() const {
        if (is_null()) {
            return Type::NULL_TYPE;
        }
        if (is_int()) {
            return Type::INT;
        }
        if (is_string()) {
            return Type::STRING;
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    std::string type_name() const {
        if (is_null()) {
            return "NULL";
        }
        if (is_int()) {
            return "INT";
        }
        if (is_string()) {
            return "STRING";
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    int32_t as_int() const {
        if (!is_int()) throw std::runtime_error("Value is not an int");
        return std::get<int32_t>(_data);
    }

    InternedString as_string() const {
        if (!is_string()) throw std::runtime_error("Value is not a string");
        return std::get<InternedString>(_data);
    }

    std::string to_string() const {
        if (is_null()) {
            return "NULL";
        }
        if (is_int()) {
            return std::to_string(as_int());
        }
        if (is_string()) {
            return std::string(as_string().intern_view);
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    bool StrictEq(const Value& other) const {
        if (is_null() && other.is_null()) {
            return true;
        }
        if (get_type() != other.get_type()) {
            return false;
        }
        if (is_int()) {
            return as_int() == other.as_int();
        }
        if (is_string()) {
            return as_string().intern_view == other.as_string().intern_view;
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    SqlBool operator==(const Value& other) const {
        if (is_null() || other.is_null()) {
            return SqlBool::UNKNOWN;
        }
        if (get_type() != other.get_type()) {
            throw std::runtime_error("Compare different type");
        }
        if (is_int()) {
            return as_int() == other.as_int() ? SqlBool::TRUE : SqlBool::FALSE;
        }
        if (is_string()) {
            return as_string().intern_view == other.as_string().intern_view ? SqlBool::TRUE : SqlBool::FALSE;
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    SqlBool operator<(const Value& other) const {
        if (is_null() || other.is_null()) {
            return SqlBool::UNKNOWN;
        }
        if (get_type() != other.get_type()) {
            throw std::runtime_error("Compare different type");
        }
        if (is_int()) {
            return as_int() < other.as_int() ? SqlBool::TRUE : SqlBool::FALSE;
        }
        if (is_string()) {
            return as_string().intern_view < other.as_string().intern_view ? SqlBool::TRUE : SqlBool::FALSE;
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    SqlBool operator<=(const Value& other) const { return (*this < other) || (*this == other); }

    SqlBool operator>(const Value& other) const { return !(*this <= other); }

    SqlBool operator>=(const Value& other) const { return !(*this < other); }

    SqlBool operator!=(const Value& other) const { return !(*this == other); }
};

}  // namespace qdb::storage

#endif  // QUASARDB_VALUE_H