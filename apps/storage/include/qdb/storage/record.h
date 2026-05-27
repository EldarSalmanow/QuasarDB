#ifndef QUASARDB_RECORD_H
#define QUASARDB_RECORD_H

#include <qdb/storage/schema.h>
#include <qdb/storage/serializer.h>
#include <qdb/storage/value.h>

#include <optional>

namespace qdb::storage {

struct RecordAddress {
    std::uint32_t page_index;
    std::uint32_t slot_idx;

    auto operator==(const RecordAddress& other) const -> bool;

    auto operator!=(const RecordAddress& other) const -> bool;

    friend auto operator<<(std::ostream& ostream, const RecordAddress& address) -> std::ostream&;
};

class Record final {
public:
    Record(std::uint32_t id, std::uint32_t values_count);

    Record(std::uint32_t id, std::vector<Value> values);

    Record(std::uint32_t id, std::vector<Value> values, RecordAddress address);

public:
    static auto FromBinary(const std::uint8_t* data, std::uint32_t size, std::uint32_t record_id,
                           const Schema& schema, Interner& interner) -> Record;

public:
    auto Id() const -> std::uint32_t;

    auto Size() const -> std::uint32_t;

    auto Address() const -> RecordAddress;

    auto SetAddress(RecordAddress address) -> void;

    auto HasAddress() const -> bool;

    auto SerializedSize() const -> std::uint32_t;

    auto Serialize() const -> std::vector<std::uint8_t>;

public:
    auto operator==(const Record& other) const -> bool;

    auto operator[](const std::size_t &index) -> Value&;

    auto operator[](const std::size_t &index) const -> const Value&;

private:
    std::uint32_t _id;

    std::vector<Value> _fields;

    std::optional<RecordAddress> _address;
};

}  // namespace qdb::storage

#endif  // QUASARDB_RECORD_H
