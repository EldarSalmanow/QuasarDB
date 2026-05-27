#include <qdb/storage/schema.h>

#include <unordered_set>

namespace qdb::storage {

Schema::Schema() = default;

Schema::Schema(std::vector<Column> columns, std::uint32_t record_id_count)
        : columns_(std::move(columns)), record_id_count_(record_id_count) {
    std::unordered_set<std::string> column_names;

    for (const auto &column : columns_) {
        if (!column_names.insert(column.Name()).second) {
            throw std::runtime_error("[ERROR in qdb::storage::Schema]: Columns can`t have same names!");
        }
    }
}

auto Schema::FromBinary(std::istream& stream) -> std::optional<Schema> {
    std::string header;
    header.resize(HEADER.size());
    if (!stream.read(reinterpret_cast<char*>(header.data()), HEADER.size()) || header != HEADER) {
        return std::nullopt;
    }

    uint32_t record_id_count;
    if (!stream.read(reinterpret_cast<char*>(&record_id_count), sizeof(record_id_count))) {
        return std::nullopt;
    }

    uint32_t columns_len;
    if (!stream.read(reinterpret_cast<char*>(&columns_len), sizeof(columns_len)) || columns_len > 1024 * 1024) {
        return std::nullopt;
    }

    std::vector<Column> columns;
    columns.reserve(columns_len);
    for (uint32_t i = 0; i < columns_len; ++i) {
        if (auto column = Column::FromBinary(stream)) {
            columns.push_back(std::move(*column));
        } else {
            return std::nullopt;
        }
    }

    return Schema(columns, record_id_count);
}

auto Schema::ToBinary(std::ostream& stream) const -> bool {
    stream.write(HEADER.data(), HEADER.size());
    stream.write(reinterpret_cast<const char*>(&record_id_count_), sizeof(record_id_count_));

    uint32_t columns_len = columns_.size();
    stream.write(reinterpret_cast<char*>(&columns_len), sizeof(columns_len));

    for (const auto& column : columns_) {
        if (!column.ToBinary(stream)) {
            return false;
        }
    }

    return !stream.fail();
}

auto Schema::IncrementRecordIdCount() -> void {
    ++record_id_count_;
}

auto Schema::DecrementRecordIdCount() -> void {
    --record_id_count_;
}

auto Schema::ColumnIndex(const std::string& column_name) const -> int32_t {
    for (size_t index = 0; index < columns_.size(); ++index) {
        if (columns_[index].Name() == column_name) {
            return static_cast<int32_t>(index);
        }
    }

    return -1;
}

auto Schema::Size() const -> std::size_t {
    return columns_.size();
}

auto Schema::RecordIdCount() const -> std::uint32_t {
    return record_id_count_;
}

auto Schema::operator==(const Schema& other) const -> bool {
    return columns_ == other.columns_ && record_id_count_ == other.record_id_count_;
}

auto Schema::operator[](const std::size_t &index) const -> const Column& {
    return columns_[index];
}

}  // namespace qdb::storage
