#include "../include/qdb/storage/table.h"

namespace qdb::storage {

Table::Table(std::string name, fs::path root, Interner* interner)
    : _name(std::move(name)),
      _root(std::move(root)),
      _pager(_root / (_name + std::string(DATA_EXT)), PAGE_SIZE),
      _interner(interner),
      _journal(_root, _name),
      _id_to_addr(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT))),
      _indexes(_root, _name) {
    _interner->UseStorage(_root / (_name + std::string(STR_STORAGE_EXT)));
    auto metadata = std::make_unique<union MetadataPage>();
    _pager.read_page(METADATA_PAGE_ID, metadata->raw);
    auto header = std::string_view(metadata->metadata.header);
    if (header != HEADER) {
        throw std::runtime_error(
            "Headers don't match. Read: " + std::string(header) + ". Expected: " + std::string(HEADER) + "."
        );
    }

    fs::path path_to_schema = _root / (_name + std::string(SCHEMA_EXT));
    std::ifstream ifs(path_to_schema, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("No schema for table with name " + _name + ".");
    }
    auto schema = Schema::FromBinary(ifs);
    if (schema) {
        _schema = std::move(*schema);
    } else {
        throw std::runtime_error("Can not read schema for table with name " + _name + ".");
    }
    _indexes.Open(_schema);
}

Table::Table(std::string name, fs::path root, Schema schema, Interner* interner)
    : _name(std::move(name)),
      _root(std::move(root)),
      _pager(_root / (_name + std::string(DATA_EXT)), PAGE_SIZE),
      _interner(interner),
      _schema(std::move(schema)),
      _journal(_root, _name),
      _id_to_addr(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT))),
      _indexes(_root, _name) {
    _interner->UseStorage(_root / (_name + std::string(STR_STORAGE_EXT)));
    auto data_path = _root / (_name + std::string(DATA_EXT));
    auto str_storage_path = _root / (_name + std::string(STR_STORAGE_EXT));
    if (!fs::is_empty(data_path) || !fs::is_empty(str_storage_path)) {
        throw std::runtime_error("Table with name " + _name + " already exists.");
    }
    auto page_id = _pager.append_new_page();
    assert(page_id == METADATA_PAGE_ID);
    auto metadata = std::make_unique<union MetadataPage>();
    std::memcpy(metadata->metadata.header, HEADER.data(), HEADER.size());
    metadata->metadata.header[HEADER.size()] = '\0';
    _pager.write_page(METADATA_PAGE_ID, metadata->raw);

    save_schema();

    _indexes.Open(_schema);
}

Table::~Table() noexcept { close(); }

void Table::save_schema() {
    fs::path temp_path = _root / (_name + std::string(SCHEMA_TMP_EXT));
    std::ofstream ofs(temp_path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Cannot create schema file.");
    }
    if (!_schema.ToBinary(ofs)) {
        throw std::runtime_error("Cannot write schema to file.");
    }
    ofs.close();
    fs::path path_to_schema = _root / (_name + std::string(SCHEMA_EXT));
    fs::rename(temp_path, path_to_schema);
}

void Table::drop() {
    close();
    _indexes.Drop();
    fs::remove(_root / (_name + std::string(DATA_EXT)));
    fs::remove(_root / (_name + std::string(SCHEMA_EXT)));
    fs::remove(_root / (_name + std::string(STR_STORAGE_EXT)));
    _journal.drop();
    fs::remove(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT)));
}

void Table::close() noexcept {
    _pager.close();
    _indexes.Close();
    _id_to_addr.close();
}

Record Table::insert_record(std::vector<Value> values, const std::vector<std::string>& column_names) {
    uint32_t record_id = _schema.RecordIdCount();
    Record record = make_record(record_id, std::move(values), column_names);
    validate_record(record);
    _journal.save_insertion(record);
    _schema.IncrementRecordIdCount();
    save_schema();
    write_record_to_disk(record);
    _indexes.Insert(_schema, record);
    _id_to_addr.insert(record.Id(), record.Address());
    return record;
}

std::vector<Record> Table::
    insert_multiple(std::vector<std::vector<Value>> rows, const std::vector<std::string>& column_names) {
    std::vector<Record> result;
    for (auto& values : rows) {
        result.emplace_back(insert_record(std::move(values), column_names));
    }
    return result;
}

std::optional<Record> Table::read_record(RecordAddress record_address) {
    if (!_pager.page_exists(record_address.page_index)) {
        return std::nullopt;
    }
    auto table_page = TablePage(record_address.page_index, &_pager, _interner);
    auto record = table_page.read_record(record_address.slot_idx, _schema);
    if (record == std::nullopt) {
        return std::nullopt;
    }
    record->SetAddress(record_address);
    return record;
}

std::optional<Record> Table::read_by_id(uint32_t record_id) {
    auto addresses = _id_to_addr.search(record_id);
    if (addresses.empty()) {
        return std::nullopt;
    }
    return read_record(addresses.back());
}

std::vector<Record> Table::records() {
    std::vector<Record> result;
    for (uint32_t id = 0; id < _schema.RecordIdCount(); ++id) {
        auto addresses = _id_to_addr.search(id);
        if (!addresses.empty()) {
            result.push_back(*read_record(addresses.back()));
        }
    }
    return result;
}

std::optional<std::vector<Record>> Table::find_by_index(const std::string& column_name, const Value& value) {
    const auto column_idx = _schema.ColumnIndex(column_name);
    auto addresses = _indexes.Find(_schema, column_name, value);
    if (!addresses.has_value()) return std::nullopt;

    std::vector<Record> result;
    for (auto address : addresses.value()) {
        auto record = read_record(address);
        if (record && (*record)[column_idx].StrictEq(value)) {
            result.push_back(std::move(*record));
        }
    }
    return result;
}

RecordAddress Table::update_record(Record& record) {
    validate_record(record);
    auto old_record_addr = record.Address();
    auto old_record = *read_record(old_record_addr);
    _journal.save_updation(old_record);
    auto table_page = TablePage(record.Address().page_index, &_pager, _interner);
    table_page.delete_record(record.Address().slot_idx);
    auto new_record_addr = write_record_to_disk(record, old_record_addr.page_index);
    _indexes.Update(_schema, old_record, record);
    _id_to_addr.update(record.Id(), record.Address());
    return new_record_addr;
}

std::vector<std::pair<int, std::string>> Table::update_multiple(std::vector<Record>& records) {
    std::vector<std::pair<int, std::string>> error_list;
    for (uint32_t i = 0; i < records.size(); ++i) {
        try {
            update_record(records[i]);
        } catch (const std::exception& e) {
            error_list.emplace_back(i, e.what());
        } catch (...) {
            error_list.emplace_back(i, "Unknown error");
        }
    }
    return error_list;
}

void Table::delete_record(const Record& record) {
    _journal.save_deletion(record);
    auto table_page = TablePage(record.Address().page_index, &_pager, _interner);
    table_page.delete_record(record.Address().slot_idx);
    _indexes.Remove(_schema, record);
    _id_to_addr.remove(record.Id());
}

void Table::delete_multiple(const std::vector<Record>& records) {
    for (const auto& record : records) {
        delete_record(record);
    }
}

void Table::revert(const std::string& time) {
    auto revert_data = _journal.revert_last(time, _schema, *_interner);
    while (!revert_data.time.empty()) {
        auto& record = revert_data.record;
        if (revert_data.type == Journal::Track::Type::INSERT) {
            write_record_to_disk(record);
            _indexes.Insert(_schema, record);
            _id_to_addr.insert(record.Id(), record.Address());
        } else if (revert_data.type == Journal::Track::Type::UPDATE) {
            assert(_id_to_addr.search(record.Id()).size() == 1);
            auto current_record_addr = _id_to_addr.search(record.Id()).back();
            auto current_record = *read_record(current_record_addr);
            record.SetAddress(current_record_addr);
            auto table_page = TablePage(record.Address().page_index, &_pager, _interner);
            table_page.delete_record(record.Address().slot_idx);
            write_record_to_disk(record, record.Address().page_index);
            _indexes.Update(_schema, current_record, record);
            _id_to_addr.update(record.Id(), record.Address());
        } else if (revert_data.type == Journal::Track::Type::DELETE) {
            assert(_id_to_addr.search(record.Id()).size() == 1);
            auto address = _id_to_addr.search(record.Id()).back();
            auto table_page = TablePage(address.page_index, &_pager, _interner);
            auto real_record = *table_page.read_record(address.slot_idx, _schema);
            table_page.delete_record(address.slot_idx);
            _indexes.Remove(_schema, real_record);
            _id_to_addr.remove(record.Id());
            assert(record.Id() + 1 == _schema.RecordIdCount());
            _schema.DecrementRecordIdCount();
        }
        revert_data = _journal.revert_last(time, _schema, *_interner);
    }
    save_schema();
}

Record Table::make_record(uint32_t record_id, std::vector<Value> values, const std::vector<std::string>& column_names) {
    if (column_names.empty()) {
        if (values.size() != _schema.Size()) {
            throw std::runtime_error(
                "Column count mismatch: expected " + std::to_string(_schema.Size()) + ", got " +
                std::to_string(values.size())
            );
        }
        return Record(record_id, std::move(values));
    }
    if (values.size() != column_names.size()) {
        throw std::runtime_error(
            "Values size and columns size mismatch: " + std::to_string(values.size()) +
            " != " + std::to_string(column_names.size()) + "."
        );
    }
    auto column_indices = resolve_column_indices(column_names);

    Record record(record_id, _schema.Size());
    apply_defaults(record, column_indices);
    update_columns(record, values, column_indices);
    return record;
}

std::vector<size_t> Table::resolve_column_indices(const std::vector<std::string>& column_names) const {
    std::vector<size_t> column_indices;
    column_indices.reserve(column_names.size());
    for (const auto& column_name : column_names) {
        auto col_index = _schema.ColumnIndex(column_name);
        if (col_index == -1) {
            throw std::runtime_error("Unknown column: " + column_name);
        }
        column_indices.push_back(static_cast<size_t>(col_index));
    }
    return column_indices;
}

void Table::apply_defaults(Record& record, const std::vector<size_t>& provided_column_indices) {
    for (size_t i = 0; i < _schema.Size(); ++i) {
        if (_schema[i].HasDefault() &&
            std::find(provided_column_indices.begin(), provided_column_indices.end(), i) ==
                provided_column_indices.end())
        {
            record[i] = _schema[i].DefaultValue(*_interner);
        }
    }
}

void Table::update_columns(Record& record, std::vector<Value> values, const std::vector<size_t>& column_indices) {
    for (size_t i = 0; i < column_indices.size(); ++i) {
        record[column_indices[i]] = std::move(values[i]);
    }
}

void Table::validate_record(const Record& record) {
    for (size_t i = 0; i < _schema.Size(); ++i) {
        const auto& column = _schema[i];
        const auto& value = record[i];
        if (column.IsNotNull() && value.IsNull()) {
            throw std::runtime_error("Column '" + column.Name() + "' cannot be NULL (NOT_NULL constraint)");
        }
        if (!value.IsNull()) {
            if (column.IsInt() && !value.IsInt()) {
                throw std::runtime_error("Column '" + column.Name() + "' expects INT, got " + value.GetTypeName());
            }
            if (column.IsString() && !value.IsString()) {
                throw std::runtime_error("Column '" + column.Name() + "' expects STRING, got " + value.GetTypeName());
            }
        }
        if (column.IsIndexed() && _indexes.HasDuplicate(_schema, record, i, value, [this](auto address) {
                return read_record(address);
            }))
        {
            auto text = value.IsString() ? std::string(_interner->View(value.AsString())) : value.ToString();
            throw std::runtime_error("Duplicate value for indexed column '" + column.Name() + "': " + text);
        }
    }
}

RecordAddress Table::write_record_to_disk(Record& record, uint32_t prefer_page_id) {
    auto serialized_record = record.Serialize();
    uint32_t serialized_size = serialized_record.size();
    auto table_page = find_enough_free_page(serialized_size, prefer_page_id);
    uint32_t slot_idx = table_page.insert_record(record.Id(), serialized_record.data(), serialized_size);
    RecordAddress record_address = {table_page.id(), slot_idx};
    record.SetAddress(record_address);
    return record_address;
}

TablePage Table::find_enough_free_page(uint32_t free_space, uint32_t prefer_page_id) {
    if (free_space > TablePage::MAX_RECORD_SIZE) {
        throw std::runtime_error("free_space > TablePage::MAX_RECORD_SIZE.");
    }
    auto total_pages = _pager.get_total_pages();
    if (prefer_page_id > 0 && prefer_page_id < total_pages) {
        auto table_page = TablePage(prefer_page_id, &_pager, _interner);
        if (table_page.is_record_fit(free_space)) {
            return TablePage(table_page.id(), &_pager, _interner);
        }
    }
    for (auto i = DATA_PAGE_WITH; i < total_pages; ++i) {
        auto table_page = TablePage(i, &_pager, _interner);
        if (table_page.is_record_fit(free_space)) {
            return TablePage(table_page.id(), &_pager, _interner);
        }
    }
    return TablePage::create(&_pager, _interner);
}

std::string Table::name() const { return _name; }

const Schema& Table::schema() const { return _schema; }

}  // namespace qdb::storage
