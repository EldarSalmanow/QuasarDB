#ifndef QUASARDB_RECORD_H
#define QUASARDB_RECORD_H

#include <fstream>
#include <string>
#include <vector>
#include "schema.h"
#include "serializer.h"
#include "value.h"

namespace qdb::storage {

struct RecordAddress {
    uint32_t page_idx;
    uint32_t slot_idx;

    bool operator==(const RecordAddress& other) const;

    bool operator!=(const RecordAddress& other) const;

    friend std::ostream& operator<<(std::ostream& os, const RecordAddress& record_addr);
};

class Record final {
    uint32_t _id;
    std::vector<Value> _fields;
    RecordAddress _addr;
    bool _has_addr = false;

public:
    Record(uint32_t id, uint32_t n);

    Record(uint32_t id, std::vector<Value> values);

    Record(uint32_t id, std::vector<Value> values, RecordAddress addr);

    bool operator==(const Record& other) const;

    uint32_t size() const;

    Value& operator[](int idx);

    const Value& operator[](int idx) const;

    uint32_t id() const;

    bool has_addr() const;

    RecordAddress address() const;

    void set_address(RecordAddress addr);

    uint32_t serialized_values_size(const Schema& schema, Serializer* serializer) const;

    std::vector<uint8_t> serialized(const Schema& schema, Serializer* serializer) const;

    static Record from_binary(
        uint8_t* data,
        uint32_t size,
        uint32_t record_id,
        const Schema& schema,
        StringStorage* string_storage,
        Serializer* serializer
    );

    friend std::ostream& operator<<(std::ostream& os, const Record& record);
};

}  // namespace qdb::storage

#endif  // QUASARDB_RECORD_H