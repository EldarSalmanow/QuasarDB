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
public:
    Serializer(Interner* interner, uint32_t max_small_str_size);

public:
    uint32_t serialized_size(const Value& value) const;

    void append_value_to_buffer(const Value& value, std::vector<uint8_t>& buffer) const;

    Value read_value(uint8_t*& data, Value::Type type, StringStorage* string_storage);

private:
    Interner* interner_;

    uint32_t max_small_str_size_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_SERIALIZER_H