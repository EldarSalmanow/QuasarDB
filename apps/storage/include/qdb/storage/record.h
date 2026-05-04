#ifndef QUASARDB_RECORD_H
#define QUASARDB_RECORD_H

#include <string>
#include <vector>
#include "schema.h"
#include "value.h"

namespace qdb::storage {

struct RecordAddress {
    uint32_t page_idx;
    uint32_t slot_idx;

    bool operator==(const RecordAddress& other) const {
        return page_idx == other.page_idx && slot_idx == other.slot_idx;
    }

    bool operator!=(const RecordAddress& other) const { return !(*this == other); }

    friend std::ostream& operator<<(std::ostream& os, const RecordAddress& record_addr) {
        os << "(page_idx=" << record_addr.page_idx << ", slot_idx=" << record_addr.slot_idx << ")";
        return os;
    }
};

class Record final {
    uint32_t _id;
    std::vector<Value> _fields;
    RecordAddress _addr;
    bool _has_addr = false;

public:
    Record(uint32_t id, uint32_t n) : _id(id), _fields(std::vector<Value>(n)) {}

    Record(uint32_t id, std::vector<Value> values) : _id(id), _fields(std::move(values)){};

    Record(uint32_t id, std::vector<Value> values, RecordAddress addr)
        : _id(id), _fields(std::move(values)), _addr(addr), _has_addr(true){};

    bool operator==(const Record& other) const {
        if (size() != other.size()) {
            return false;
        }
        for (uint32_t i = 0; i < size(); ++i) {
            if (!_fields[i].StrictEq(other._fields[i])) {
                return false;
            }
        }
        return _id == other._id && _has_addr == other._has_addr && (!_has_addr || (address() == other.address()));
    }

    uint32_t size() const { return _fields.size(); }

    Value& operator[](int idx) { return _fields[idx]; }

    const Value& operator[](int idx) const { return _fields[idx]; }

    auto id() const { return _id; }

    bool has_addr() const { return _has_addr; }

    RecordAddress address() const {
        if (!has_addr()) {
            throw std::runtime_error("Record has not address.");
        }
        return _addr;
    }

    void set_address(RecordAddress addr) {
        _addr = addr;
        _has_addr = true;
    }

    uint32_t serialized_values_size(const Schema& schema) const {
        uint32_t result = schema.null_bitmap_size();
        for (const auto& value : _fields) {
            if (value.is_null()) {
                continue;
            }
            result += value.serialized_size();
        }
        return result;
    }

    std::vector<uint8_t> serialized(const Schema& schema) const {
        std::vector<uint8_t> data_v;
        auto result_size = serialized_values_size(schema);
        data_v.reserve(result_size);
        data_v.resize(schema.null_bitmap_size());
        for (size_t i = 0; i < _fields.size(); ++i) {
            const auto& value = _fields[i];
            if (value.is_null()) {
                auto bitmap_idx = schema.get_bitmap_idx(static_cast<uint32_t>(i));
                data_v[bitmap_idx / 8] |= (1 << (bitmap_idx % 8));
            } else {
                value.append_to_buffer(data_v);
            }
        }
        assert(data_v.size() == result_size);
        return data_v;
    }

    static Record from_binary(uint8_t* data, uint32_t size, uint32_t record_id, const Schema& schema) {
        const uint8_t* null_bitmap = data;
        data += schema.null_bitmap_size();
        std::vector<Value> values(schema.size());
        for (size_t i = 0; i < schema.size(); ++i) {
            if (!schema[i].not_null()) {
                auto bitmap_idx = schema.get_bitmap_idx(static_cast<uint32_t>(i));
                bool value_is_null = null_bitmap[bitmap_idx / 8] & (1 << (bitmap_idx % 8));
                if (value_is_null) {
                    continue;
                }
            }
            if (schema[i].is_int()) {
                values[i] = Value::from_binary(data, Value::Type::INT);
            } else if (schema[i].is_string()) {
                values[i] = Value::from_binary(data, Value::Type::STRING);
            }
        }
        if (data - null_bitmap != size) {
            throw std::runtime_error("Read data and size are mismatch.");
        }
        return Record(record_id, values);
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_RECORD_H