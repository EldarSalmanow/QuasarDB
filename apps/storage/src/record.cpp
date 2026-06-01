#include <qdb/storage/record.h>

#include <cassert>

namespace qdb::storage {

auto RecordAddress::operator==(const RecordAddress& other) const -> bool {
    return page_index == other.page_index && slot_idx == other.slot_idx;
}

auto RecordAddress::operator!=(const RecordAddress& other) const -> bool { return !(*this == other); }

auto operator<<(std::ostream& ostream, const RecordAddress& address) -> std::ostream& {
    ostream << "(page_index=" << address.page_index << ", slot_index=" << address.slot_idx << ")";

    return ostream;
}

Record::Record(std::uint32_t id, std::uint32_t values_count) : _id(id), _fields(std::vector<Value>(values_count)) {
    if (values_count == 0) {
        throw std::runtime_error("[ERROR in qdb::storage::Record]: Record cannot have 0 columns!");
    }
}

Record::Record(std::uint32_t id, std::vector<Value> values) : _id(id), _fields(std::move(values)){};

Record::Record(std::uint32_t id, std::vector<Value> values, RecordAddress address)
    : _id(id), _fields(std::move(values)), _address(address){};

auto Record::FromBinary(
    const std::uint8_t* data,
    std::uint32_t size,
    std::uint32_t record_id,
    const Schema& schema,
    Interner& interner
) -> Record {
    const std::uint8_t* start = data;

    std::vector<Value> values(schema.Size());
    for (std::size_t i = 0; i < schema.Size(); ++i) {
        values[i] = ReadValue(data, interner);
    }

    if (data - start != size) {
        throw std::runtime_error("[ERROR in qdb::storage::Record]: Read data and size are mismatch!");
    }

    return Record(record_id, values);
}

auto Record::Id() const -> std::uint32_t { return _id; }

auto Record::Size() const -> std::uint32_t { return static_cast<std::uint32_t>(_fields.size()); }

auto Record::Address() const -> RecordAddress {
    if (!_address.has_value()) {
        throw std::runtime_error("[ERROR in qdb::storage::Record]: Record has not address!");
    }

    return _address.value();
}

auto Record::SetAddress(RecordAddress address) -> void { _address = address; }

auto Record::HasAddress() const -> bool { return _address.has_value(); }

auto Record::SerializedSize() const -> std::uint32_t {
    uint32_t result = 0;
    for (const auto& value : _fields) {
        result += qdb::storage::SerializedSize(value);
    }

    return result;
}

auto Record::Serialize() const -> std::vector<std::uint8_t> {
    auto result_size = SerializedSize();

    std::vector<std::uint8_t> data_v;
    data_v.reserve(result_size);
    for (const auto& value : _fields) {
        AppendValueToBuffer(value, data_v);
    }

    assert(data_v.size() == result_size);

    return data_v;
}

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

auto Record::operator[](const std::size_t& index) -> Value& { return _fields[index]; }

auto Record::operator[](const std::size_t& index) const -> const Value& { return _fields[index]; }

}  // namespace qdb::storage
