#ifndef QUASARDB_SCHEMA_H
#define QUASARDB_SCHEMA_H

#include "column.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace qdb::storage {

class Schema final {
public:
    Schema() = default;

    Schema(std::vector<Column> columns, uint32_t record_id_count = 0);

public:
    static std::optional<Schema> from_binary(std::istream& stream);

public:
    bool to_binary(std::ostream& stream);

    void increment_record_id_count();

    void decrement_record_id_count();

    int32_t get_column_idx(const std::string& column_name) const;

    uint32_t get_bitmap_idx(uint32_t column_idx) const;

    size_t size() const;

    uint32_t record_id_count() const;

    uint32_t null_bitmap_size() const;

public:
    bool operator==(const Schema& other) const;

    const Column& operator[](const size_t& index) const;

private:
    static constexpr bool DEBUG = false;
    static constexpr std::string_view HEADER = "SCHEMA";
    static constexpr uint32_t FORMAT_MAGIC = 0x51444232;  // QDB2
    static constexpr uint32_t FORMAT_VERSION = 2;

    std::vector<Column> _columns;
    uint32_t _record_id_count = 0;
    uint32_t _bitmap_size = 0;  // bytes
    std::unordered_map<uint32_t, uint32_t> column_id_to_bitmap_id;
};

}  // namespace qdb::storage

#endif  // QUASARDB_SCHEMA_H
