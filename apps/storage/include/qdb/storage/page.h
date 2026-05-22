#ifndef QUASARDB_PAGE_H
#define QUASARDB_PAGE_H

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "pager.h"
#include "record.h"
#include "schema.h"
#include "serializer.h"
#include "string_storage.h"

namespace qdb::storage {

class TablePage final {
    static constexpr bool DEBUG = false;

public:
    static constexpr uint32_t PAGE_SIZE = 4096;

    struct PageHeader {
        uint32_t page_id = 0;
        uint32_t slot_count = 0;
        uint32_t free_space_upper = PAGE_SIZE;
        uint32_t total_free_bytes = PAGE_SIZE - sizeof(struct PageHeader);
    };

    struct Slot {
        uint32_t record_offset;
        uint32_t record_size;
        uint32_t record_id;
        bool deleted;
    };

    union DataPage {
        PageHeader header;
        uint8_t raw[PAGE_SIZE];

        DataPage() : header(){};
    };

    static_assert(sizeof(DataPage) == PAGE_SIZE);

    static constexpr uint32_t MAX_RECORD_SIZE = PAGE_SIZE - sizeof(PageHeader) - sizeof(Slot);

private:
    struct PageDeleter {
        bool owned;
        void operator()(DataPage* p) const;
    };

    Pager* _pager;
    std::unique_ptr<DataPage, PageDeleter> _data;
    StringStorage* _str_storage;
    Serializer* _serializer;

public:
    TablePage(uint32_t id, Pager* pager, StringStorage* str_storage, Serializer* serializer, void* mem_ptr = nullptr);

    static TablePage create(Pager* pager, StringStorage* str_storage, Serializer* serializer);

    uint8_t* raw();

    uint32_t id() const;

    void save_to_disk();

    Slot* get_slots() const;

    uint32_t slot_count() const;

    uint32_t free_space_upper() const;

    uint32_t get_continuous_free_space_size() const;

    uint32_t get_full_free_space_size() const;

    uint32_t is_record_fit(uint32_t record_size);

    uint32_t get_free_or_new_slot_idx();

    int32_t insert_record(int32_t record_id, const uint8_t* data, uint32_t size);

    std::optional<Record> read_record(uint32_t slot_idx, const Schema& schema) const;

    void delete_record(uint32_t slot_idx);

    void compact();
};

}  // namespace qdb::storage

#endif  // QUASARDB_PAGE_H