#ifndef QUASARDB_VALUE_H
#define QUASARDB_VALUE_H

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>

namespace qdb::storage {

enum class SqlBool {
    FALSE,
    TRUE,
    UNKNOWN,
};

auto operator==(SqlBool first, SqlBool second) -> bool;

auto operator&&(SqlBool first, SqlBool second) -> SqlBool;

auto operator||(SqlBool first, SqlBool second) -> SqlBool;

auto operator!(SqlBool first) -> SqlBool;

struct StringId {
    bool operator==(const StringId& other) const { return value == other.value; }

    bool operator<(const StringId& other) const { return value < other.value; }

    friend std::ostream& operator<<(std::ostream& ostream, const StringId& id) { return ostream << id.value; }

    uint32_t value = 0;
};

class Value final {
public:
    enum class Type {
        NULL_TYPE,
        INT,
        STRING,
    };

public:
    friend class Interner;

public:
    Value();

    explicit Value(std::int32_t value);

private:
    explicit Value(StringId value);

public:
    auto IsNull() const -> bool;

    auto IsInt() const -> bool;

    auto IsString() const -> bool;

    auto GetType() const -> Type;

    auto GetTypeName() const -> std::string;

    auto AsInt() const -> std::int32_t;

    auto AsString() const -> StringId;

    auto ToString() const -> std::string;

    auto StrictEq(const Value& other) const -> bool;

public:
    auto operator==(const Value& other) const -> SqlBool;

    auto operator<(const Value& other) const -> SqlBool;

    auto operator<=(const Value& other) const -> SqlBool;

    auto operator>(const Value& other) const -> SqlBool;

    auto operator>=(const Value& other) const -> SqlBool;

    auto operator!=(const Value& other) const -> SqlBool;

private:
    std::variant<std::nullptr_t, std::int32_t, StringId> data_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_VALUE_H
