#include <qdb/storage/serializer.h>

#include <cstring>

namespace qdb::storage {

enum class StoredType : std::uint8_t {
    Null = 0,
    Int = 1,
    String = 2,
};

auto SerializedSize(const Value& value) -> std::uint32_t {
    if (value.IsNull()) {
        return 1;
    }

    if (value.IsInt()) {
        return sizeof(StoredType) + sizeof(value.AsInt());
    }

    if (value.IsString()) {
        return sizeof(StoredType) + sizeof(StringId);
    }

    throw std::runtime_error("[ERROR in qdb::storage::SerializedSize]: Unexpected type!");
}

auto AppendValueToBuffer(const Value& value, std::vector<uint8_t>& buffer) -> void {
    size_t offset = buffer.size();

    if (value.IsNull()) {
        StoredType type = StoredType::Null;

        buffer.resize(offset + sizeof(type));

        std::memcpy(buffer.data() + offset, &type, sizeof(type));

        return;
    }

    if (value.IsInt()) {
        StoredType type = StoredType::Int;

        auto value_as_int = value.AsInt();
        auto required_size = offset + sizeof(type) + sizeof(value_as_int);

        if (buffer.size() < required_size) {
            buffer.resize(required_size);
        }

        std::memcpy(buffer.data() + offset, &type, sizeof(type));

        offset += sizeof(type);

        std::memcpy(buffer.data() + offset, &value_as_int, sizeof(value_as_int));
    } else if (value.IsString()) {
        StoredType type = StoredType::String;

        auto id = value.AsString();
        auto required_size = offset + sizeof(type) + sizeof(id);

        if (buffer.size() < required_size) {
            buffer.resize(required_size);
        }

        std::memcpy(buffer.data() + offset, &type, sizeof(type));

        offset += sizeof(type);

        std::memcpy(buffer.data() + offset, &id, sizeof(id));
    } else {
        throw std::runtime_error("[ERROR in qdb::storage::AppendValueToBuffer]: Unexpected type!");
    }
}

auto ReadValue(const uint8_t*& data, Interner& interner) -> Value {
    StoredType type;
    std::memcpy(&type, data, sizeof(type));
    data += sizeof(type);

    if (type == StoredType::Null) {
        return {};
    }

    if (type == StoredType::Int) {
        int32_t value;
        std::memcpy(&value, data, sizeof(value));
        data += sizeof(value);

        return Value(value);
    }

    if (type == StoredType::String) {
        StringId id;
        std::memcpy(&id, data, sizeof(id));
        data += sizeof(id);

        return interner.Get(id);
    }

    throw std::runtime_error("[ERROR in qdb::storage::ReadValue]: Unknown serialized value type!");
}

}  // namespace qdb::storage
