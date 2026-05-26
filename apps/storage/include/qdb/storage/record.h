#ifndef QUASARDB_RECORD_H
#define QUASARDB_RECORD_H

#include <qdb/storage/schema.h>
#include <qdb/storage/serializer.h>
#include <qdb/storage/value.h>

#include <optional>

namespace qdb::storage {

struct RecordAddress {
    uint32_t page_idx;
    uint32_t slot_idx;

    auto operator==(const RecordAddress& other) const -> bool;

    auto operator!=(const RecordAddress& other) const -> bool;

    friend std::ostream& operator<<(std::ostream& os, const RecordAddress& record_addr);
};

class Record final {
    uint32_t _id;
    std::vector<Value> _fields;
    std::optional<RecordAddress> _address;

public:
    Record(uint32_t id, uint32_t n);

    Record(uint32_t id, std::vector<Value> values);

    Record(uint32_t id, std::vector<Value> values, RecordAddress addr);

    auto operator==(const Record& other) const -> bool;

    auto Size() const -> uint32_t;

    auto operator[](size_t index) -> Value&;

    auto operator[](size_t index) const -> const Value&;

    auto Id() const -> uint32_t;

    auto HasAddress() const -> bool;

    auto Address() const -> RecordAddress;

    auto SetAddress(RecordAddress address) -> void;

    auto SerializedSize() const -> uint32_t;

    auto Serialize() const -> std::vector<uint8_t>;

    static auto FromBinary(
        const uint8_t* data,
        uint32_t size,
        uint32_t record_id,
        const Schema& schema,
        Interner& interner
    ) -> Record;

    friend std::ostream& operator<<(std::ostream& os, const Record& record);
};

}  // namespace qdb::storage

#endif  // QUASARDB_RECORD_H
