#include "../include/qdb/storage/page.h"

#include <cstring>

namespace qdb::storage {

auto CellAt(TablePage::DataPage* page, uint32_t idx) -> std::pair<TablePage::Cell*, uint8_t*> {
    uint32_t offset = sizeof(TablePage::PageHeader);
    for (uint32_t i = 0; i < idx; ++i) {
        auto* cell = reinterpret_cast<TablePage::Cell*>(page->raw + offset);
        offset += sizeof(TablePage::Cell) + cell->record_size;
    }
    auto* cell = reinterpret_cast<TablePage::Cell*>(page->raw + offset);
    return {cell, page->raw + offset + sizeof(TablePage::Cell)};
}

auto CellAt(const TablePage::DataPage* page, uint32_t idx) -> std::pair<const TablePage::Cell*, const uint8_t*> {
    uint32_t offset = sizeof(TablePage::PageHeader);
    for (uint32_t i = 0; i < idx; ++i) {
        auto* cell = reinterpret_cast<const TablePage::Cell*>(page->raw + offset);
        offset += sizeof(TablePage::Cell) + cell->record_size;
    }
    auto* cell = reinterpret_cast<const TablePage::Cell*>(page->raw + offset);
    return {cell, page->raw + offset + sizeof(TablePage::Cell)};
}

TablePage::TablePage(uint32_t id, Pager* pager, Interner* interner) : _pager(pager), _interner(interner) {
    if (pager) {
        pager->read_page(id, _data.raw);
        if (_data.header.page_id != id) {
            throw std::runtime_error("Readed page id is not match with argument id.");
        }
    } else {
        _data.header.page_id = id;
    }
}

TablePage TablePage::create(Pager* pager, Interner* interner) {
    uint32_t page_id = pager->append_new_page();
    std::unique_ptr<DataPage> data = std::make_unique<DataPage>();
    data->header.page_id = page_id;
    pager->write_page(page_id, data->raw);
    return TablePage(page_id, pager, interner);
}

uint8_t* TablePage::raw() { return _data.raw; }

uint32_t TablePage::id() const { return _data.header.page_id; }

void TablePage::save_to_disk() {
    if (_pager) {
        _pager->write_page(id(), raw());
    }
}

uint32_t TablePage::record_count() const { return _data.header.record_count; }

uint32_t TablePage::get_full_free_space_size() const { return PAGE_SIZE - _data.header.next_free_offset; }

uint32_t TablePage::is_record_fit(uint32_t record_size) {
    return record_size + sizeof(Cell) <= get_full_free_space_size();
}

int32_t TablePage::insert_record(int32_t record_id, const uint8_t* data, uint32_t size) {
    uint32_t needed_size = sizeof(Cell) + size;
    if (get_full_free_space_size() < needed_size) {
        return -1;
    }

    uint32_t offset = _data.header.next_free_offset;
    auto* cell = reinterpret_cast<Cell*>(_data.raw + offset);
    cell->record_id = record_id;
    cell->record_size = size;
    cell->deleted = false;

    std::memcpy(_data.raw + offset + sizeof(Cell), data, size);

    auto slot_idx = _data.header.record_count++;
    _data.header.next_free_offset += needed_size;

    save_to_disk();
    return slot_idx;
}

std::optional<Record> TablePage::read_record(uint32_t slot_idx, const Schema& schema) const {
    if (slot_idx >= record_count()) {
        return std::nullopt;
    }
    auto [cell, payload] = CellAt(&_data, slot_idx);
    if (cell->deleted) {
        return std::nullopt;
    }
    auto record = Record::FromBinary(payload, cell->record_size, cell->record_id, schema, *_interner);
    return record;
}

void TablePage::delete_record(uint32_t slot_idx) {
    if (slot_idx >= record_count()) {
        throw std::out_of_range(
            "Slot idx out of range: slot_idx=" + std::to_string(slot_idx) +
            ", slot_count=" + std::to_string(record_count()) + "."
        );
    }
    auto [cell, payload] = CellAt(&_data, slot_idx);
    if (!cell->deleted) {
        cell->deleted = true;
        save_to_disk();
    }
}

}  // namespace qdb::storage
