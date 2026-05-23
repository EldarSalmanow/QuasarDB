#ifndef QUASARDB_COLUMN_H
#define QUASARDB_COLUMN_H

#include "interner.h"

#include <cstdint>
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
    static constexpr uint8_t NOT_NULL_FLAG = (1 << 0);
    static constexpr uint8_t INDEXED_FLAG = (1 << 1);

public:
    Column(std::string name, ColumnType type, uint8_t flags = 0);

    Column(
        std::string name,
        ColumnType type,
        uint8_t flags,
        DefaultType default_type,
        int32_t default_int = 0,
        std::string default_string = ""
    );

public:
    static std::optional<Column> from_binary(std::istream& stream);

    static std::optional<Column> from_binary(std::istream& stream, bool read_default_metadata);

public:
    bool to_binary(std::ostream& stream) const;

    std::string name() const;

    bool is_int() const;

    bool is_string() const;

    bool not_null() const;

    bool indexed() const;

    bool has_default() const;

    DefaultType default_type() const;

    Value default_value(Interner& interner) const;

public:
    bool operator==(const Column& other) const = default;

private:
    std::string name_;

    ColumnType type_;

    uint8_t flags_;

    DefaultType default_type_;

    int32_t default_int_;

    std::string default_string_;

    void validate_default() const;

    static constexpr bool DEBUG = false;
    static constexpr uint32_t MAX_SERIALIZED_STRING_SIZE = 1024 * 1024;
};

}  // namespace qdb::storage

#endif  // QUASARDB_COLUMN_H
