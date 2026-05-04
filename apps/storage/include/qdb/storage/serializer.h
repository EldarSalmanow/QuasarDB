#ifndef QUASARDB_SERIALIZER_H
#define QUASARDB_SERIALIZER_H

#include <cassert>
#include <string_view>
#include <utility>
#include "interner.h"
#include "string_storage.h"
#include "value.h"

namespace qdb::storage {

class Serializer final {
    Interner* _interner;
    uint32_t _max_small_str_size;

public:
    Serializer(Interner* interner, uint32_t max_small_str_size)
        : _interner(interner), _max_small_str_size(max_small_str_size) {}

    uint32_t serialized_size(const Value& value) const {
        if (value.is_null()) {
            return 1;
        }
        if (value.is_int()) {
            return sizeof(value.as_int());
        }
        if (value.is_string()) {
            auto interned_str = value.as_string();
            if (interned_str.has_ext_addr) {
                return 1 + sizeof(ExternalString);
            }
            uint32_t len = static_cast<uint32_t>(interned_str.intern_view.size());
            return 1 + sizeof(len) + len;
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }

    void append_value_to_buffer(const Value& value, std::vector<uint8_t>& buffer) const {
        size_t offset = buffer.size();
        if (value.is_null()) {
            throw std::runtime_error("Cannot write null as value.");
        }
        if (value.is_int()) {
            auto value_as_int = value.as_int();
            auto required_size = offset + sizeof(value_as_int);
            if (buffer.size() < required_size) {
                buffer.resize(required_size);
            }
            std::memcpy(buffer.data() + offset, &value_as_int, sizeof(value_as_int));
        } else if (value.is_string()) {
            auto value_as_string = value.as_string();
            if (value_as_string.has_ext_addr) {
                bool flag_has_ext_addr = true;
                auto required_size = offset + sizeof(flag_has_ext_addr) + sizeof(value_as_string.ext_addr);
                if (buffer.size() < required_size) {
                    buffer.resize(required_size);
                }
                std::memcpy(buffer.data() + offset, &flag_has_ext_addr, sizeof(flag_has_ext_addr));
                offset += sizeof(flag_has_ext_addr);
                std::memcpy(buffer.data() + offset, &value_as_string.ext_addr, sizeof(value_as_string.ext_addr));
            } else {
                bool flag_has_ext_addr = false;
                const auto& string = value_as_string.intern_view;
                uint32_t len = static_cast<uint32_t>(string.size());
                auto required_size = offset + sizeof(flag_has_ext_addr) + sizeof(len) + len;
                if (buffer.size() < required_size) {
                    buffer.resize(required_size);
                }
                std::memcpy(buffer.data() + offset, &flag_has_ext_addr, sizeof(flag_has_ext_addr));
                offset += sizeof(flag_has_ext_addr);
                std::memcpy(buffer.data() + offset, &len, sizeof(len));
                offset += sizeof(len);
                std::memcpy(buffer.data() + offset, string.data(), len);
            }
        } else {
            assert(false && "Unexpected type.");
        }
    }

    Value read_value(uint8_t*& data, Value::Type type, StringStorage* string_storage) {
        if (type == Value::Type::NULL_TYPE) {
            throw std::runtime_error("Cannot read null type of value.");
        }
        if (type == Value::Type::INT) {
            int32_t value;
            std::memcpy(&value, data, sizeof(value));
            data += sizeof(value);
            return Value(value);
        }
        if (type == Value::Type::STRING) {
            ExternalString str_ext_addr;
            bool flag_has_ext_addr;
            std::memcpy(&flag_has_ext_addr, data, sizeof(flag_has_ext_addr));
            data += sizeof(flag_has_ext_addr);
            if (flag_has_ext_addr) {
                std::memcpy(&str_ext_addr, data, sizeof(str_ext_addr));
                data += sizeof(str_ext_addr);
                if (string_storage == nullptr) {
                    throw std::runtime_error("StringStorage* is needed for read large string.");
                }
                return _interner->str_to_value(string_storage->read(str_ext_addr), str_ext_addr);
            } else {
                uint32_t len;
                std::memcpy(&len, data, sizeof(len));
                data += sizeof(len);
                auto view = std::string_view(reinterpret_cast<const char*>(data), len);
                data += len;
                return _interner->str_to_value(view);
            }
        }
        assert(false && "Unexpected type.");
        __builtin_unreachable();
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_SERIALIZER_H