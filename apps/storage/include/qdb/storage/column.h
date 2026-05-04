#ifndef QUASARDB_COLUMN_H
#define QUASARDB_COLUMN_H

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

namespace qdb::storage {

class Column final {
    static constexpr bool DEBUG = false;

public:
    enum class ColumnType : uint8_t {
        INT,
        STRING,
    };

    static constexpr bool REQUIRE_NOTNULL_FOR_INDEXED = true;
    static constexpr uint8_t NOT_NULL_FLAG = (1 << 0);
    static constexpr uint8_t INDEXED_FLAG = (1 << 1);

private:
    std::string _name;
    ColumnType _type;
    uint8_t _flags;

public:
    Column(std::string name, ColumnType type, uint8_t flags = 0) : _name(std::move(name)), _type(type), _flags(flags) {
        if (DEBUG) {
            std::cout << "Column::Column" << std::endl;
        }
        if (REQUIRE_NOTNULL_FOR_INDEXED && (_flags & INDEXED_FLAG)) {
            _flags |= NOT_NULL_FLAG;
        }
    }

    bool operator==(const Column& other) const = default;

    bool to_binary(std::ostream& os) const {
        if (DEBUG) {
            std::cout << "Column::to_binary" << std::endl;
        }
        uint32_t name_len = _name.size();
        os.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        os.write(_name.c_str(), name_len);
        os.write(reinterpret_cast<const char*>(&_type), sizeof(_type));
        os.write(reinterpret_cast<const char*>(&_flags), sizeof(_flags));
        return !os.fail();
    }

    static std::optional<Column> from_binary(std::istream& is) {
        if (DEBUG) {
            std::cout << "Column::from_binary" << std::endl;
        }
        uint32_t name_len;
        if (!is.read(reinterpret_cast<char*>(&name_len), sizeof(name_len)) || name_len > 1024 * 1024) {
            return std::nullopt;
        }
        std::string name;
        name.resize(name_len);
        if (!is.read(&name[0], name_len)) {
            return std::nullopt;
        }
        ColumnType type;
        if (!is.read(reinterpret_cast<char*>(&type), sizeof(type))) {
            return std::nullopt;
        }
        uint8_t flags;
        if (!is.read(reinterpret_cast<char*>(&flags), sizeof(flags))) {
            return std::nullopt;
        }
        try {
            return Column(std::move(name), type, flags);
        } catch (const std::invalid_argument&) {
            return std::nullopt;
        }
    }

    bool not_null() const {
        assert(!(_flags & INDEXED_FLAG) || (_flags & NOT_NULL_FLAG));
        return _flags & NOT_NULL_FLAG;
    }

    bool indexed() const { return _flags & INDEXED_FLAG; }

    std::string name() const { return _name; }

    bool is_int() const { return _type == ColumnType::INT; }

    bool is_string() const { return _type == ColumnType::STRING; }
};

}  // namespace qdb::storage

#endif  // QUASARDB_COLUMN_H