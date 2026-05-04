#ifndef QUASARDB_COLUMN_H
#define QUASARDB_COLUMN_H

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

namespace qdb::storage {

class Column final {
public:
    enum class ColumnType : uint8_t {
        INT,
        STRING,
    };

public:
    static constexpr bool REQUIRE_NOTNULL_FOR_INDEXED = true;
    static constexpr uint8_t NOT_NULL_FLAG = (1 << 0);
    static constexpr uint8_t INDEXED_FLAG = (1 << 1);

public:
    Column(std::string name, ColumnType type, uint8_t flags = 0) : name_(std::move(name)), type_(type), flags_(flags) {
        if (DEBUG) {
            std::cout << "Column::Column" << std::endl;
        }

        if (REQUIRE_NOTNULL_FOR_INDEXED && ((flags_ & INDEXED_FLAG) != 0)) {
            flags_ |= NOT_NULL_FLAG;
        }
    }

public:
    static std::optional<Column> from_binary(std::istream& stream) {
        if (DEBUG) {
            std::cout << "Column::from_binary" << std::endl;
        }

        uint32_t name_len;
        if (!stream.read(reinterpret_cast<char*>(&name_len), sizeof(name_len)) || name_len > 1024 * 1024) {
            return std::nullopt;
        }

        std::string name;
        name.resize(name_len);
        if (!stream.read(name.data(), name_len)) {
            return std::nullopt;
        }

        ColumnType type;
        if (!stream.read(reinterpret_cast<char*>(&type), sizeof(type))) {
            return std::nullopt;
        }

        uint8_t flags;
        if (!stream.read(reinterpret_cast<char*>(&flags), sizeof(flags))) {
            return std::nullopt;
        }

        try {
            return Column(std::move(name), type, flags);
        } catch (const std::invalid_argument&) {
            return std::nullopt;
        }
    }

public:
    bool to_binary(std::ostream& stream) const {
        if (DEBUG) {
            std::cout << "Column::to_binary" << std::endl;
        }

        uint32_t name_len = name_.size();

        stream.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        stream.write(name_.c_str(), name_len);
        stream.write(reinterpret_cast<const char*>(&type_), sizeof(type_));
        stream.write(reinterpret_cast<const char*>(&flags_), sizeof(flags_));

        return !stream.fail();
    }

    std::string name() const { return name_; }

    bool is_int() const { return type_ == ColumnType::INT; }

    bool is_string() const { return type_ == ColumnType::STRING; }

    bool not_null() const {
        assert(!(flags_ & INDEXED_FLAG) || (flags_ & NOT_NULL_FLAG));

        return (flags_ & NOT_NULL_FLAG) != 0;
    }

    bool indexed() const { return (flags_ & INDEXED_FLAG) != 0; }

public:
    bool operator==(const Column& other) const = default;

private:
    std::string name_;

    ColumnType type_;

    uint8_t flags_;

    static constexpr bool DEBUG = false;
};

}  // namespace qdb::storage

#endif  // QUASARDB_COLUMN_H