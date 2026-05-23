#include "../include/qdb/storage/value.h"

#include <iostream>

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

Value::Value() : _data(nullptr) {}

Value::Value(int32_t val) : _data(val) {}

Value::Value(InternedString val) : _data(val) {}

bool Value::is_null() const { return std::holds_alternative<std::nullptr_t>(_data); }

bool Value::is_int() const { return std::holds_alternative<int32_t>(_data); }

bool Value::is_string() const { return std::holds_alternative<InternedString>(_data); }

Value::Type Value::get_type() const {
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

std::string Value::type_name() const {
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

int32_t Value::as_int() const {
    if (!is_int()) throw std::runtime_error("Value is not an int");
    return std::get<int32_t>(_data);
}

InternedString Value::as_string() const {
    if (!is_string()) throw std::runtime_error("Value is not a string");
    return std::get<InternedString>(_data);
}

std::string Value::to_string() const {
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

bool Value::StrictEq(const Value& other) const {
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

SqlBool Value::operator==(const Value& other) const {
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

SqlBool Value::operator<(const Value& other) const {
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

SqlBool Value::operator<=(const Value& other) const { return (*this < other) || (*this == other); }

SqlBool Value::operator>(const Value& other) const { return !(*this <= other); }

SqlBool Value::operator>=(const Value& other) const { return !(*this < other); }

SqlBool Value::operator!=(const Value& other) const { return !(*this == other); }

std::ostream& operator<<(std::ostream& os, const Value& value) {
    os << "V(" << value.type_name();
    if (value.is_int()) {
        os << "|" << value.as_int();
    } else if (value.is_string()) {
        os << "|has_ext_addr=" << value.as_string().has_ext_addr;
        if (value.as_string().has_ext_addr) {
            os << "|ext_addr=(offset=" << value.as_string().ext_addr.offset
               << "|size=" << value.as_string().ext_addr.size;
        }
        os << "|string=" << value.as_string().intern_view;
    }
    os << ")";
    return os;
}
}  // namespace qdb::storage