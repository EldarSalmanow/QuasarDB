#include "../include/qdb/storage/page.h"

#include <cstring>
#include <iostream>

namespace qdb::storage {

void TablePage::PageDeleter::operator()(DataPage* p) const {
    if (owned && p != nullptr) {
        delete p;
    }
}

TablePage::TablePage(uint32_t id, Pager* pager, StringStorage* str_storage, Serializer* serializer, void* mem_ptr)
    : _pager(pager),
      _data(mem_ptr ? static_cast<DataPage*>(mem_ptr) : new DataPage(), PageDeleter{mem_ptr == nullptr}),
      _str_storage(str_storage),
      _serializer(serializer) {
    // read exists page or create in-memory
    if (DEBUG) {
        std::cout << "TablePage::TablePage(read)" << std::endl;
    }
    if (pager) {
        pager->read_page(id, _data->raw);
        if (_data->header.page_id != id) {
            throw std::runtime_error("Readed page id is not match with argument id.");
        }
    } else {
        _data->header.page_id = id;
    }
}

TablePage TablePage::create(Pager* pager, StringStorage* str_storage, Serializer* serializer) {
    // create page in file
    if (DEBUG) {
        std::cout << "TablePage::TablePage(create)" << std::endl;
    }
    uint32_t page_id = pager->append_new_page();
    std::unique_ptr<DataPage> data = std::make_unique<DataPage>();
    data->header.page_id = page_id;
    pager->write_page(page_id, data->raw);
    return TablePage(page_id, pager, str_storage, serializer);
}

uint8_t* TablePage::raw() { return _data->raw; }

uint32_t TablePage::id() const { return _data->header.page_id; }

void TablePage::save_to_disk() {
    if (_pager) {
        _pager->write_page(id(), raw());
    }
}

TablePage::Slot* TablePage::get_slots() const { return reinterpret_cast<Slot*>(_data->raw + sizeof(PageHeader)); }

uint32_t TablePage::slot_count() const { return _data->header.slot_count; }

uint32_t TablePage::free_space_upper() const { return _data->header.free_space_upper; }

uint32_t TablePage::get_continuous_free_space_size() const {
    uint32_t slots_end = sizeof(PageHeader) + (_data->header.slot_count * sizeof(Slot));
    return _data->header.free_space_upper - slots_end;
}

uint32_t TablePage::get_full_free_space_size() const { return _data->header.total_free_bytes; }

uint32_t TablePage::is_record_fit(uint32_t record_size) {
    return record_size + sizeof(Slot) <= get_full_free_space_size();
}

uint32_t TablePage::get_free_or_new_slot_idx() {
    auto* slots = get_slots();
    for (uint32_t i = 0; i < slot_count(); ++i) {
        if (slots[i].deleted) {
            return i;
        }
    }
    return slot_count();
}

int32_t TablePage::insert_record(int32_t record_id, const uint8_t* data, uint32_t size) {
    if (DEBUG) {
        std::cout << "TablePage::insert_record" << std::endl;
    }
    uint32_t needed_size = sizeof(Slot) + size;
    if (get_full_free_space_size() < needed_size) {
        return -1;
    }
    if (get_continuous_free_space_size() < needed_size) {
        compact();
    }

    uint32_t offset = _data->header.free_space_upper - size;
    std::memcpy(_data->raw + offset, data, size);

    auto* slots = get_slots();
    auto slot_idx = get_free_or_new_slot_idx();
    slots[slot_idx].record_offset = offset;
    slots[slot_idx].record_size = size;
    slots[slot_idx].record_id = record_id;
    slots[slot_idx].deleted = false;

    _data->header.free_space_upper = offset;
    _data->header.total_free_bytes -= needed_size;
    if (slot_idx == slot_count()) {
        ++_data->header.slot_count;
    }

    save_to_disk();
    return slot_idx;
}

std::optional<Record> TablePage::read_record(uint32_t slot_idx, const Schema& schema) const {
    if (DEBUG) {
        std::cout << "TablePage::read_record" << std::endl;
    }
    if (slot_idx >= slot_count()) {
        return std::nullopt;
    }
    auto slots = get_slots();
    if (slots[slot_idx].deleted) {
        return std::nullopt;
    }
    auto record = Record::from_binary(
        _data->raw + slots[slot_idx].record_offset,
        slots[slot_idx].record_size,
        slots[slot_idx].record_id,
        schema,
        _str_storage,
        _serializer
    );
    return record;
}

void TablePage::delete_record(uint32_t slot_idx) {
    if (DEBUG) {
        std::cout << "TablePage::delete_record" << std::endl;
    }
    if (slot_idx >= slot_count()) {
        throw std::out_of_range(
            "Slot idx out of range: slot_idx=" + std::to_string(slot_idx) +
            ", slot_count=" + std::to_string(slot_count()) + "."
        );
    }
    auto slots = get_slots();
    if (slots[slot_idx].deleted) {
        return;
    }
    slots[slot_idx].deleted = true;
    for (auto i = slot_count() - 1; slot_count() > 0 && slots[i].deleted; --i) {
        --_data->header.slot_count;
        _data->header.total_free_bytes += sizeof(Slot);
    }
    _data->header.total_free_bytes += slots[slot_idx].record_size;
    save_to_disk();
}

void TablePage::compact() {
    // Note: this method does not save data to disk.
    if (DEBUG) {
        std::cout << "TablePage::compact" << std::endl;
    }
    TablePage temp(0, nullptr, _str_storage, _serializer);
    uint32_t slots_end = sizeof(PageHeader) + (slot_count() * sizeof(Slot));
    std::memcpy(temp.raw(), raw(), slots_end);
    std::memset(temp.raw() + slots_end, 0, PAGE_SIZE - slots_end);
    temp._data->header.free_space_upper = PAGE_SIZE;
    Slot* slots = get_slots();
    for (uint32_t i = 0; i < slot_count(); ++i) {
        if (!slots[i].deleted) {
            auto record_size = slots[i].record_size;
            temp._data->header.free_space_upper -= record_size;
            temp.get_slots()[i].record_offset = temp.free_space_upper();
            std::memcpy(temp.raw() + temp.free_space_upper(), raw() + slots[i].record_offset, record_size);
        }
    }
    assert(temp._data->header.total_free_bytes == temp._data->header.free_space_upper - slots_end);
    std::memcpy(raw(), temp.raw(), PAGE_SIZE);
}

}  // namespace qdb::storage
