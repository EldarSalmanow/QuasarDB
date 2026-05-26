#include "../include/qdb/storage/record.h"

#include <cassert>

namespace qdb::storage {

auto RecordAddress::operator==(const RecordAddress& other) const -> bool {
    return page_idx == other.page_idx && slot_idx == other.slot_idx;
}

auto RecordAddress::operator!=(const RecordAddress& other) const -> bool { return !(*this == other); }

std::ostream& operator<<(std::ostream& os, const RecordAddress& record_addr) {
    os << "(page_idx=" << record_addr.page_idx << ", slot_idx=" << record_addr.slot_idx << ")";
    return os;
}

Record::Record(uint32_t id, uint32_t n) : _id(id), _fields(std::vector<Value>(n)) {
    if (n == 0) {
        throw std::runtime_error("Record cannot have 0 columns.");
    }
}

Record::Record(uint32_t id, std::vector<Value> values) : _id(id), _fields(std::move(values)){};

Record::Record(uint32_t id, std::vector<Value> values, RecordAddress addr)
    : _id(id), _fields(std::move(values)), _address(addr){};

auto Record::operator==(const Record& other) const -> bool {
    if (Size() != other.Size()) {
        return false;
    }
    for (uint32_t i = 0; i < Size(); ++i) {
        if (!_fields[i].StrictEq(other._fields[i])) {
            return false;
        }
    }
    return _id == other._id && _address == other._address;
}

auto Record::Size() const -> uint32_t { return static_cast<uint32_t>(_fields.size()); }

auto Record::operator[](size_t index) -> Value& { return _fields[index]; }

auto Record::operator[](size_t index) const -> const Value& { return _fields[index]; }

auto Record::Id() const -> uint32_t { return _id; }

auto Record::HasAddress() const -> bool { return _address.has_value(); }

auto Record::Address() const -> RecordAddress {
    if (!_address.has_value()) {
        throw std::runtime_error("Record has not address.");
    }
    return _address.value();
}

auto Record::SetAddress(RecordAddress address) -> void {
    _address = address;
}

auto Record::SerializedSize() const -> uint32_t {
    uint32_t result = 0;
    for (const auto& value : _fields) {
        result += qdb::storage::SerializedSize(value);
    }
    return result;
}

auto Record::Serialize() const -> std::vector<uint8_t> {
    std::vector<uint8_t> data_v;
    auto result_size = SerializedSize();
    data_v.reserve(result_size);
    for (const auto& value : _fields) {
        AppendValueToBuffer(value, data_v);
    }
    assert(data_v.size() == result_size);
    return data_v;
}

auto Record::FromBinary(
    const uint8_t* data,
    uint32_t size,
    uint32_t record_id,
    const Schema& schema,
    Interner& interner
) -> Record {
    const uint8_t* start = data;
    std::vector<Value> values(schema.Size());
    for (size_t i = 0; i < schema.Size(); ++i) {
        values[i] = ReadValue(data, interner);
    }
    if (data - start != size) {
        throw std::runtime_error("Read data and size are mismatch.");
    }
    return Record(record_id, values);
}

std::ostream& operator<<(std::ostream& os, const Record& record) {
    os << "Rec(id=" << record._id << "|has_addr=" << record._address.has_value() << "|";
    for (const auto& v : record._fields) {
        os << v << "|";
    }
    os << ")";
    return os;
}

}  // namespace qdb::storage
