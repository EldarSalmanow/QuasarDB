#ifndef QUASARDB_SCHEMA_H
#define QUASARDB_SCHEMA_H

#include <qdb/storage/column.h>

#include <optional>
#include <string>
#include <vector>

namespace qdb::storage {

class Schema final {
public:
    Schema();

    Schema(std::vector<Column> columns, std::uint32_t record_id_count = 0);

public:
    static auto FromBinary(std::istream& stream) -> std::optional<Schema>;

public:
    auto ToBinary(std::ostream& stream) const -> bool;

    auto IncrementRecordIdCount() -> void;

    auto DecrementRecordIdCount() -> void;

    auto ColumnIndex(const std::string& column_name) const -> std::int32_t;

    auto Size() const -> std::size_t;

    auto RecordIdCount() const -> std::uint32_t;

public:
    auto operator==(const Schema& other) const -> bool;

    auto operator[](const std::size_t &index) const -> const Column&;

private:
    static constexpr std::string_view HEADER = "SCHEMA";

    std::vector<Column> columns_;
    uint32_t record_id_count_ = 0;
};

}  // namespace qdb::storage

#endif  // QUASARDB_SCHEMA_H
