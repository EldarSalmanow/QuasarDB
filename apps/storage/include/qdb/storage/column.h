#ifndef QUASARDB_COLUMN_H
#define QUASARDB_COLUMN_H

#include <qdb/storage/interner.h>

#include <optional>
#include <string>

namespace qdb::storage {

class Column final {
public:
    enum ColumnType : uint8_t {
        INT,
        STRING,
    };

    enum class DefaultType : uint8_t {
        NONE,
        NULL_VALUE,
        INT,
        STRING,
    };

public:
    static constexpr bool REQUIRE_NOTNULL_FOR_INDEXED = true;
    static constexpr uint8_t NOT_NULL_FLAG = 1 << 0;
    static constexpr uint8_t INDEXED_FLAG = 1 << 1;

private:
    static constexpr uint32_t MAX_SERIALIZED_STRING_SIZE = 1024 * 1024;

public:
    Column(std::string name, ColumnType type, uint8_t flags = 0);

    Column(std::string name, ColumnType type, uint8_t flags,
           DefaultType default_type, int32_t default_int = 0, std::string default_string = "");

public:
    static auto FromBinary(std::istream& stream) -> std::optional<Column>;

public:
    auto ToBinary(std::ostream& stream) const -> bool;

public:
    auto Name() const -> std::string;

    auto IsInt() const -> bool;

    auto IsString() const -> bool;

    auto IsNotNull() const -> bool;

    auto IsIndexed() const -> bool;

    auto HasDefault() const -> bool;

    auto GetDefaultType() const -> DefaultType;

    auto DefaultValue(Interner& interner) const -> Value;

public:
    auto operator==(const Column& other) const -> bool;

private:
    auto ValidateDefault() const -> void;

private:
    std::string name_;

    ColumnType type_;

    uint8_t flags_;

    DefaultType default_type_;

    int32_t default_int_;

    std::string default_string_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_COLUMN_H
