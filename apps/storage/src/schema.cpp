#include "../include/qdb/storage/schema.h"

#include <iostream>

namespace qdb::storage {

Schema::Schema(std::vector<Column> columns, uint32_t record_id_count)
    : _columns(std::move(columns)), _record_id_count(record_id_count) {
    if (DEBUG) {
        std::cout << "Schema::Schema(columns, record_id)" << std::endl;
    }

    std::unordered_set<std::string> column_names;
    _bitmap_size = 0;

    for (size_t i = 0; i < _columns.size(); ++i) {
        if (!column_names.insert(_columns[i].name()).second) {
            throw std::runtime_error("[ERROR in qdb::storage::Schema]: Columns can`t have same names!");
        }

        if (!_columns[i].not_null()) {
            column_id_to_bitmap_id[i] = _bitmap_size;

            ++_bitmap_size;
        }
    }

    _bitmap_size = (_bitmap_size + 7) / 8;
}

std::optional<Schema> Schema::from_binary(std::istream& stream) {
    if (DEBUG) {
        std::cout << "Schema::from_binary" << std::endl;
    }

    std::string header;
    header.resize(HEADER.size());
    if (!stream.read(reinterpret_cast<char*>(header.data()), HEADER.size()) || header != HEADER) {
        return std::nullopt;
    }

    uint32_t first_field;
    if (!stream.read(reinterpret_cast<char*>(&first_field), sizeof(first_field))) {
        return std::nullopt;
    }

    bool read_default_metadata = false;
    uint32_t record_id_count = first_field;
    if (first_field == FORMAT_MAGIC) {
        uint32_t format_version;
        if (!stream.read(reinterpret_cast<char*>(&format_version), sizeof(format_version)) ||
            format_version != FORMAT_VERSION) {
            return std::nullopt;
        }
        if (!stream.read(reinterpret_cast<char*>(&record_id_count), sizeof(record_id_count))) {
            return std::nullopt;
        }
        read_default_metadata = true;
    }

    uint32_t columns_len;
    if (!stream.read(reinterpret_cast<char*>(&columns_len), sizeof(columns_len)) || columns_len > 1024 * 1024) {
        return std::nullopt;
    }

    std::vector<Column> _columns;
    _columns.reserve(columns_len);
    for (uint32_t i = 0; i < columns_len; ++i) {
        auto column = Column::from_binary(stream, read_default_metadata);

        if (column) {
            _columns.push_back(std::move(*column));
        } else {
            return std::nullopt;
        }
    }

    try {
        return Schema(_columns, record_id_count);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

bool Schema::to_binary(std::ostream& stream) {
    if (DEBUG) {
        std::cout << "Schema::to_binary" << std::endl;
    }

    stream.write(HEADER.data(), HEADER.size());
    uint32_t format_magic = FORMAT_MAGIC;
    uint32_t format_version = FORMAT_VERSION;
    stream.write(reinterpret_cast<char*>(&format_magic), sizeof(format_magic));
    stream.write(reinterpret_cast<char*>(&format_version), sizeof(format_version));
    stream.write(reinterpret_cast<char*>(&_record_id_count), sizeof(_record_id_count));

    uint32_t columns_len = _columns.size();
    stream.write(reinterpret_cast<char*>(&columns_len), sizeof(columns_len));

    for (const auto& column : _columns) {
        if (!column.to_binary(stream)) {
            return false;
        }
    }

    return !stream.fail();
}

void Schema::increment_record_id_count() {
    if (DEBUG) {
        std::cout << "Schema::increment_record_id_count" << std::endl;
    }

    ++_record_id_count;
}

void Schema::decrement_record_id_count() {
    if (DEBUG) {
        std::cout << "Schema::decrement_record_id_count" << std::endl;
    }

    --_record_id_count;
}

int32_t Schema::get_column_idx(const std::string& column_name) const {
    for (size_t index = 0; index < _columns.size(); ++index) {
        if (_columns[index].name() == column_name) {
            return static_cast<int32_t>(index);
        }
    }

    return -1;
}

uint32_t Schema::get_bitmap_idx(uint32_t column_idx) const {
    if (DEBUG) {
        std::cout << "Schema::get_bitmap_idx" << std::endl;
    }

    if (column_idx >= _columns.size()) {
        throw std::out_of_range(
            "[ERROR in qdb::storage::Schema]: Column index " + std::to_string(column_idx) + " is out of range [0, " +
            std::to_string(_columns.size()) + ")!"
        );
    }

    if (_columns[column_idx].not_null()) {
        throw std::runtime_error(
            "[ERROR in qdb::storage::Schema]: Column with column index " + std::to_string(column_idx) +
            " is not null and has not bitmap index!"
        );
    }

    return column_id_to_bitmap_id.at(column_idx);
}

size_t Schema::size() const { return _columns.size(); }

uint32_t Schema::record_id_count() const { return _record_id_count; }

uint32_t Schema::null_bitmap_size() const { return _bitmap_size; }

bool Schema::operator==(const Schema& other) const {
    return _columns == other._columns && _record_id_count == other._record_id_count;
}

const Column& Schema::operator[](const size_t& index) const { return _columns[index]; }

}  // namespace qdb::storage
