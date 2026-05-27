#include <qdb/storage/value.h>

#include <cassert>

namespace qdb::storage {

auto operator==(SqlBool first, SqlBool second) -> bool {
    return static_cast<int>(first) == static_cast<int>(second);
}

auto operator&&(SqlBool first, SqlBool second) -> SqlBool {
    if (first == SqlBool::FALSE || second == SqlBool::FALSE) {
        return SqlBool::FALSE;
    }

    if (first == SqlBool::UNKNOWN || second == SqlBool::UNKNOWN) {
        return SqlBool::UNKNOWN;
    }

    return SqlBool::TRUE;
}

auto operator||(SqlBool first, SqlBool second) -> SqlBool {
    if (first == SqlBool::TRUE || second == SqlBool::TRUE) {
        return SqlBool::TRUE;
    }

    if (first == SqlBool::UNKNOWN || second == SqlBool::UNKNOWN) {
        return SqlBool::UNKNOWN;
    }

    return SqlBool::FALSE;
}

auto operator!(SqlBool first) -> SqlBool {
    switch (first) {
        case SqlBool::TRUE:
            return SqlBool::FALSE;
        case SqlBool::FALSE:
            return SqlBool::TRUE;
        default:
            return SqlBool::UNKNOWN;
    }
}

Value::Value()
        : data_(nullptr) {}

Value::Value(std::int32_t value)
        : data_(value) {}

Value::Value(StringId value)
        : data_(value) {}

auto Value::IsNull() const -> bool {
    return std::holds_alternative<std::nullptr_t>(data_);
}

auto Value::IsInt() const -> bool {
    return std::holds_alternative<int32_t>(data_);
}

auto Value::IsString() const -> bool {
    return std::holds_alternative<StringId>(data_);
}

auto Value::GetType() const -> Type {
    if (IsNull()) {
        return Type::NULL_TYPE;
    }

    if (IsInt()) {
        return Type::INT;
    }

    if (IsString()) {
        return Type::STRING;
    }

    throw std::runtime_error("[FATAL in qdb::storage::Value]: Unexpected type!");
}

auto Value::GetTypeName() const -> std::string {
    if (IsNull()) {
        return "NULL";
    }

    if (IsInt()) {
        return "INT";
    }

    if (IsString()) {
        return "STRING";
    }

    throw std::runtime_error("[FATAL in qdb::storage::Value]: Unexpected type!");
}

auto Value::AsInt() const -> std::int32_t {
    if (!IsInt()) {
        throw std::runtime_error("[FATAL in qdb::storage::Value]: Value is not an int!");
    }

    return std::get<int32_t>(data_);
}

auto Value::AsString() const -> StringId {
    if (!IsString()) {
        throw std::runtime_error("[FATAL in qdb::storage::Value]: Value is not a string!");
    }

    return std::get<StringId>(data_);
}

auto Value::ToString() const -> std::string {
    if (IsNull()) {
        return "NULL";
    }

    if (IsInt()) {
        return std::to_string(AsInt());
    }

    if (IsString()) {
        return "#" + std::to_string(AsString().value);
    }

    throw std::runtime_error("[FATAL in qdb::storage::Value]: Unexpected type!");
}

auto Value::StrictEq(const Value& other) const -> bool {
    if (IsNull() && other.IsNull()) {
        return true;
    }

    if (GetType() != other.GetType()) {
        return false;
    }

    if (IsInt()) {
        return AsInt() == other.AsInt();
    }

    if (IsString()) {
        return AsString() == other.AsString();
    }

    throw std::runtime_error("[FATAL in qdb::storage::Value]: Unexpected type!");
}

auto Value::operator==(const Value& other) const -> SqlBool {
    if (IsNull() || other.IsNull()) {
        return SqlBool::UNKNOWN;
    }

    if (GetType() != other.GetType()) {
        throw std::runtime_error("Compare different type");
    }

    if (IsInt()) {
        return AsInt() == other.AsInt() ? SqlBool::TRUE : SqlBool::FALSE;
    }

    if (IsString()) {
        return AsString() == other.AsString() ? SqlBool::TRUE : SqlBool::FALSE;
    }

    throw std::runtime_error("[FATAL in qdb::storage::Value]: Unexpected type!");
}

auto Value::operator<(const Value& other) const -> SqlBool {
    if (IsNull() || other.IsNull()) {
        return SqlBool::UNKNOWN;
    }

    if (GetType() != other.GetType()) {
        throw std::runtime_error("[FATAL in qdb::storage::Value]: Compare different type!");
    }

    if (IsInt()) {
        return AsInt() < other.AsInt() ? SqlBool::TRUE : SqlBool::FALSE;
    }

    if (IsString()) {
        return AsString() < other.AsString() ? SqlBool::TRUE : SqlBool::FALSE;
    }

    throw std::runtime_error("[FATAL in qdb::storage::Value]: Unexpected type!");
}

auto Value::operator<=(const Value& other) const -> SqlBool {
    return (*this < other) || (*this == other);
}

auto Value::operator>(const Value& other) const -> SqlBool {
    return !(*this <= other);
}

auto Value::operator>=(const Value& other) const -> SqlBool {
    return !(*this < other);
}

auto Value::operator!=(const Value& other) const -> SqlBool {
    return !(*this == other);
}

}  // namespace qdb::storage
