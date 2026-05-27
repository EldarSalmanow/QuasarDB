#ifndef QUASARDB_PAGE_H
#define QUASARDB_PAGE_H

#include <qdb/storage/pager.h>
#include <qdb/storage/record.h>
#include <qdb/storage/schema.h>

namespace qdb::storage {

class TablePage final {
public:
    static constexpr uint32_t PAGE_SIZE = 4096;

    struct PageHeader {
        uint32_t page_id = 0;
        uint32_t record_count = 0;
        uint32_t next_free_offset = sizeof(PageHeader);
    };

    struct Cell {
        uint32_t record_id;
        uint32_t record_size;
        bool deleted;
    };

    union DataPage {
        PageHeader header;
        uint8_t raw[PAGE_SIZE];

        DataPage() : header(){};
    };

    static_assert(sizeof(DataPage) == PAGE_SIZE);

    static constexpr uint32_t MAX_RECORD_SIZE = PAGE_SIZE - sizeof(PageHeader) - sizeof(Cell);

public:
    TablePage(uint32_t id, Pager* pager, Interner* interner);

    static TablePage create(Pager* pager, Interner* interner);

    uint8_t* raw();

    uint32_t id() const;

    void save_to_disk();

    uint32_t record_count() const;

    uint32_t get_full_free_space_size() const;

    uint32_t is_record_fit(uint32_t record_size);

    int32_t insert_record(int32_t record_id, const uint8_t* data, uint32_t size);

    std::optional<Record> read_record(uint32_t slot_idx, const Schema& schema) const;

    void delete_record(uint32_t slot_idx);

private:
    Pager* _pager;

    DataPage _data;

    Interner* _interner;
};

}  // namespace qdb::storage

#endif  // QUASARDB_PAGE_H
