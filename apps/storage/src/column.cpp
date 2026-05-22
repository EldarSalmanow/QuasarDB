#include "../include/qdb/storage/column.h"

namespace qdb::storage {

Column::Column(std::string name, ColumnType type, uint8_t flags)
    : Column(std::move(name), type, flags, DefaultType::NONE, 0, "") {}

Column::Column(
    std::string name,
    ColumnType type,
    uint8_t flags,
    DefaultType default_type,
    int32_t default_int,
    std::string default_string
)
    : name_(std::move(name)),
      type_(type),
      flags_(flags),
      default_type_(default_type),
      default_int_(default_int),
      default_string_(std::move(default_string)) {
    if (DEBUG) {
        std::cout << "Column::Column" << std::endl;
    }

    if (REQUIRE_NOTNULL_FOR_INDEXED && ((flags_ & INDEXED_FLAG) != 0)) {
        flags_ |= NOT_NULL_FLAG;
    }

    validate_default();
}

std::optional<Column> Column::from_binary(std::istream& stream) { return from_binary(stream, true); }

std::optional<Column> Column::from_binary(std::istream& stream, bool read_default_metadata) {
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

    DefaultType default_type = DefaultType::NONE;
    int32_t default_int = 0;
    std::string default_string;

    if (read_default_metadata) {
        if (!stream.read(reinterpret_cast<char*>(&default_type), sizeof(default_type))) {
            return std::nullopt;
        }

        if (default_type == DefaultType::INT) {
            if (!stream.read(reinterpret_cast<char*>(&default_int), sizeof(default_int))) {
                return std::nullopt;
            }
        } else if (default_type == DefaultType::STRING) {
            uint32_t default_len;
            if (!stream.read(reinterpret_cast<char*>(&default_len), sizeof(default_len)) ||
                default_len > MAX_SERIALIZED_STRING_SIZE)
            {
                return std::nullopt;
            }
            default_string.resize(default_len);
            if (!stream.read(default_string.data(), default_len)) {
                return std::nullopt;
            }
        } else if (default_type != DefaultType::NONE && default_type != DefaultType::NULL_VALUE) {
            return std::nullopt;
        }
    }

    try {
        return Column(std::move(name), type, flags, default_type, default_int, std::move(default_string));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

bool Column::to_binary(std::ostream& stream) const {
    if (DEBUG) {
        std::cout << "Column::to_binary" << std::endl;
    }

    uint32_t name_len = name_.size();

    stream.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
    stream.write(name_.c_str(), name_len);
    stream.write(reinterpret_cast<const char*>(&type_), sizeof(type_));
    stream.write(reinterpret_cast<const char*>(&flags_), sizeof(flags_));
    stream.write(reinterpret_cast<const char*>(&default_type_), sizeof(default_type_));

    if (default_type_ == DefaultType::INT) {
        stream.write(reinterpret_cast<const char*>(&default_int_), sizeof(default_int_));
    } else if (default_type_ == DefaultType::STRING) {
        if (default_string_.size() > MAX_SERIALIZED_STRING_SIZE) {
            return false;
        }
        uint32_t default_len = default_string_.size();
        stream.write(reinterpret_cast<const char*>(&default_len), sizeof(default_len));
        stream.write(default_string_.c_str(), default_len);
    }

    return !stream.fail();
}

std::string Column::name() const { return name_; }

bool Column::is_int() const { return type_ == ColumnType::INT; }

bool Column::is_string() const { return type_ == ColumnType::STRING; }

bool Column::not_null() const {
    assert(!(flags_ & INDEXED_FLAG) || (flags_ & NOT_NULL_FLAG));

    return (flags_ & NOT_NULL_FLAG) != 0;
}

bool Column::indexed() const { return (flags_ & INDEXED_FLAG) != 0; }

bool Column::has_default() const { return default_type_ != DefaultType::NONE; }

Column::DefaultType Column::default_type() const { return default_type_; }

Value Column::default_value(Interner& interner) const {
    switch (default_type_) {
        case DefaultType::NULL_VALUE:
            return Value();
        case DefaultType::INT:
            return Value(default_int_);
        case DefaultType::STRING:
            return interner.str_to_value(default_string_);
        case DefaultType::NONE:
            break;
    }
    throw std::runtime_error("Column '" + name_ + "' does not have DEFAULT value");
}

void Column::validate_default() const {
    if (default_type_ == DefaultType::NONE) {
        return;
    }
    if (default_type_ == DefaultType::NULL_VALUE) {
        if (not_null()) {
            throw std::invalid_argument("Column '" + name_ + "' cannot have DEFAULT NULL and NOT_NULL");
        }
        return;
    }
    if (default_type_ == DefaultType::INT && !is_int()) {
        throw std::invalid_argument("Column '" + name_ + "' expects STRING default");
    }
    if (default_type_ == DefaultType::STRING && !is_string()) {
        throw std::invalid_argument("Column '" + name_ + "' expects INT default");
    }
}
}  // namespace qdb::storage
