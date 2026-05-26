#ifndef QUASARDB_SERIALIZER_H
#define QUASARDB_SERIALIZER_H

#include <qdb/storage/interner.h>

#include <vector>

namespace qdb::storage {

auto SerializedSize(const Value& value) -> std::uint32_t;

auto AppendValueToBuffer(const Value& value, std::vector<uint8_t>& buffer) -> void;

auto ReadValue(const uint8_t*& data, Interner& interner) -> Value;

}  // namespace qdb::storage

#endif  // QUASARDB_SERIALIZER_H
