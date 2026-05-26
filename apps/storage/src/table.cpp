#include "../include/qdb/storage/table.h"

namespace qdb::storage {

Table::Table(std::string name, fs::path root, Interner* interner)
    : _name(std::move(name)),
      _root(std::move(root)),
      _pager(_root / (_name + std::string(DATA_EXT)), PAGE_SIZE),
      _interner(interner),
      _journal(_root, _name),
    _id_to_addr(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT))) {
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
    open_indexes();
}

Table::Table(std::string name, fs::path root, Schema schema, Interner* interner)
    : _name(std::move(name)),
      _root(std::move(root)),
      _pager(_root / (_name + std::string(DATA_EXT)), PAGE_SIZE),
      _interner(interner),
      _schema(std::move(schema)),
      _journal(_root, _name),
      _id_to_addr(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT))) {
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

    open_indexes();
}

void Table::open_indexes() {
    _indexes.clear();
    for (size_t i = 0; i < _schema.Size(); ++i) {
        if (_schema[i].IsIndexed()) {
            const std::string& column_name = _schema[i].Name();
            auto column_index_path = index_path(column_name);
            if (_schema[i].IsInt()) {
                _indexes.emplace(column_name, BStarPlusTree<int, RecordAddress>(column_index_path));
            } else if (_schema[i].IsString()) {
                _indexes.emplace(column_name, BStarPlusTree<StringId, RecordAddress>(column_index_path));
            }
        }
    }
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

fs::path Table::index_path(const std::string& column_name) {
    return _root / (_name + "_" + column_name + std::string(INDEX_EXT));
}

void Table::drop() {
    close();
    std::vector<fs::path> index_paths;
    for (const auto& [column_name, index] : _indexes) {
        index_paths.push_back(std::visit([](auto& tree) { return tree.path(); }, index));
    }
    _indexes.clear();
    for (const auto& p : index_paths) {
        fs::remove(p);
    }
    fs::remove(_root / (_name + std::string(DATA_EXT)));
    fs::remove(_root / (_name + std::string(SCHEMA_EXT)));
    fs::remove(_root / (_name + std::string(STR_STORAGE_EXT)));
    _journal.drop();
    fs::remove(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT)));
}

void Table::close() noexcept {
    _pager.close();
    for (auto& [column_name, index] : _indexes) {
        std::visit([](auto& tree) { return tree.close(); }, index);
    }
    _id_to_addr.close();
}

Record Table::insert_record(std::vector<Value> values, const std::vector<std::string>& column_names) {
    uint32_t record_id = _schema.RecordIdCount();
    Record record = make_record(record_id, std::move(values), column_names);
    validate_record(record);
    _schema.IncrementRecordIdCount();
    save_schema();
    write_record_to_disk(record);
    update_indexes_after_insert(record);
    _id_to_addr.insert(record.Id(), record.Address());
    _journal.save_insertion(record);
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
    if (!_pager.page_exists(record_address.page_idx)) {
        return std::nullopt;
    }
    auto table_page = TablePage(record_address.page_idx, &_pager, _interner);
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
    if (column_idx < 0) {
        throw std::runtime_error("Unknown column: " + column_name);
    }

    const auto& column = _schema[column_idx];
    if (!column.IsIndexed() || value.IsNull()) {
        return std::nullopt;
    }
    if ((column.IsInt() && !value.IsInt()) || (column.IsString() && !value.IsString())) {
        return std::nullopt;
    }

    auto index = _indexes.find(column_name);
    if (index == _indexes.end()) {
        throw std::runtime_error("Index not found for column " + column_name + ".");
    }

    std::vector<Record> result;
    auto add_records = [&](const std::vector<RecordAddress>& addresses) {
        for (auto address : addresses) {
            auto record = read_record(address);
            if (record && (*record)[column_idx].StrictEq(value)) {
                result.push_back(std::move(*record));
            }
        }
    };

    std::visit([&](auto& tree) {
        using KeyType = typename std::decay_t<decltype(tree)>::key_type;
        if constexpr (std::is_same_v<KeyType, int32_t>) {
            add_records(tree.search(value.AsInt()));
        } else if constexpr (std::is_same_v<KeyType, StringId>) {
            add_records(tree.search(value.AsString()));
        }
    }, index->second);
    return result;
}

RecordAddress Table::update_record(Record& record) {
    validate_record(record);
    auto old_record_addr = record.Address();
    auto old_record = *read_record(old_record_addr);
    auto table_page = TablePage(record.Address().page_idx, &_pager, _interner);
    table_page.delete_record(record.Address().slot_idx);
    auto new_record_addr = write_record_to_disk(record, old_record_addr.page_idx);
    update_indexes_after_update(old_record, record);
    _id_to_addr.update(record.Id(), record.Address());
    _journal.save_updation(old_record);
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
    auto table_page = TablePage(record.Address().page_idx, &_pager, _interner);
    table_page.delete_record(record.Address().slot_idx);
    update_indexes_after_delete(record);
    _id_to_addr.remove(record.Id());
    _journal.save_deletion(record);
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
            update_indexes_after_insert(record);
            _id_to_addr.insert(record.Id(), record.Address());
        } else if (revert_data.type == Journal::Track::Type::UPDATE) {
            assert(_id_to_addr.search(record.Id()).size() == 1);
            auto current_record_addr = _id_to_addr.search(record.Id()).back();
            auto current_record = *read_record(current_record_addr);
            record.SetAddress(current_record_addr);
            auto table_page = TablePage(record.Address().page_idx, &_pager, _interner);
            table_page.delete_record(record.Address().slot_idx);
            write_record_to_disk(record, record.Address().page_idx);
            update_indexes_after_update(current_record, record);
            _id_to_addr.update(record.Id(), record.Address());
        } else if (revert_data.type == Journal::Track::Type::DELETE) {
            assert(_id_to_addr.search(record.Id()).size() == 1);
            auto address = _id_to_addr.search(record.Id()).back();
            auto table_page = TablePage(address.page_idx, &_pager, _interner);
            auto real_record = *table_page.read_record(address.slot_idx, _schema);
            table_page.delete_record(address.slot_idx);
            update_indexes_after_delete(real_record);
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
        if (column.IsIndexed()) {
            auto duplicate_found = false;
            for_each_indexed_value(record, [&](size_t indexed_column, const Column&, const Value& indexed_value, IndexTree& index) {
                if (indexed_column != i) {
                    return;
                }
                duplicate_found = std::visit(
                [&](auto& tree) -> bool {
                    using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                    if constexpr (std::is_same_v<KeyType, int32_t>) {
                        auto results = tree.search(indexed_value.AsInt());
                        for (auto addr : results) {
                            if (!record.HasAddress() || addr != record.Address()) {
                                return true;
                            }
                        }
                    } else if constexpr (std::is_same_v<KeyType, StringId>) {
                        auto results = tree.search(indexed_value.AsString());
                        for (auto addr : results) {
                            auto prob_record = *read_record(addr);
                            if (prob_record[i].StrictEq(indexed_value) && (!record.HasAddress() || addr != record.Address())) {
                                return true;
                            }
                        }
                    }
                    return false;
                },
                index
                );
            });
            if (duplicate_found) {
                auto text = value.IsString() ? std::string(_interner->View(value.AsString())) : value.ToString();
                throw std::
                    runtime_error("Duplicate value for indexed column '" + column.Name() + "': " + text);
            }
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

void Table::for_each_indexed_value(
    const Record& record,
    const std::function<void(size_t, const Column&, const Value&, IndexTree&)>& action
) {
    for (size_t i = 0; i < _schema.Size(); ++i) {
        const auto& column = _schema[i];
        if (!column.IsIndexed()) {
            continue;
        }
        if (record[i].IsNull()) {
            continue;
        }
        auto it = _indexes.find(column.Name());
        if (it == _indexes.end()) {
            throw std::runtime_error("Index not found for column " + column.Name() + ".");
        }
        action(i, column, record[i], it->second);
    }
}

void Table::update_indexes_after_insert(const Record& record) {
    for_each_indexed_value(record, [&](size_t, const Column&, const Value& value, IndexTree& index) {
        std::visit(
            [&](auto& tree) {
                using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                if constexpr (std::is_same_v<KeyType, int32_t>) {
                    tree.insert(value.AsInt(), record.Address());
                } else if constexpr (std::is_same_v<KeyType, StringId>) {
                    tree.insert(value.AsString(), record.Address());
                }
            },
            index
        );
    });
}

void Table::update_indexes_after_delete(const Record& record) {
    for_each_indexed_value(record, [&](size_t, const Column&, const Value& value, IndexTree& index) {
        std::visit(
            [&](auto& tree) {
                using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                if constexpr (std::is_same_v<KeyType, int32_t>) {
                    tree.remove(value.AsInt());
                } else if constexpr (std::is_same_v<KeyType, StringId>) {
                    tree.remove(value.AsString());
                }
            },
            index
        );
    });
}

void Table::update_indexes_after_update(const Record& old_record, const Record& new_record) {
    for_each_indexed_value(new_record, [&](size_t i, const Column&, const Value&, IndexTree& index) {
        if (old_record[i].StrictEq(new_record[i]) && old_record.Address() == new_record.Address()) {
            return;
        }
            const auto& old_value = old_record[i];
            const auto& new_value = new_record[i];
            std::visit(
                [&](auto& tree) {
                    using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                    if constexpr (std::is_same_v<KeyType, int32_t>) {
                        tree.remove(old_value.AsInt());
                        tree.insert(new_value.AsInt(), new_record.Address());
                    } else if constexpr (std::is_same_v<KeyType, StringId>) {
                        tree.remove(old_value.AsString());
                        tree.insert(new_value.AsString(), new_record.Address());
                    }
                },
                index
            );
    });
}

std::string Table::name() const { return _name; }

const Schema& Table::schema() const { return _schema; }

}  // namespace qdb::storage
