#ifndef QUASARDB_TABLE_H
#define QUASARDB_TABLE_H

#include <inttypes.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include "b_star_plus_tree.h"
#include "interner.h"
#include "journal.h"
#include "page.h"
#include "pager.h"
#include "record.h"
#include "schema.h"
#include "string_storage.h"

namespace qdb::storage {
class Table final {
    static constexpr bool DEBUG = false;
    static constexpr std::string_view HEADER = "TABLE";
    static constexpr uint32_t PAGE_SIZE = 4096;  //
    static constexpr uint32_t MAX_SMALL_STR_LENGTH = PAGE_SIZE / 2;
    static constexpr uint32_t METADATA_PAGE_ID = 0;
    static constexpr uint32_t DATA_PAGE_WITH = METADATA_PAGE_ID + 1;
    static constexpr std::string_view DATA_EXT = ".data";
    static constexpr std::string_view SCHEMA_EXT = ".schema";
    static constexpr std::string_view SCHEMA_TMP_EXT = ".schema.tmp";
    static constexpr std::string_view INDEX_EXT = ".idx";
    static constexpr std::string_view STR_STORAGE_EXT = ".bin";

    static constexpr bool USE_NULL_BITMAP = true;

    struct FastStr {
        static constexpr int PREFIX_SIZE = 8;
        char _prefix[PREFIX_SIZE];
        int32_t _record_id;

        FastStr(const std::string& value, int32_t record_id) : _record_id(record_id) {
            std::memset(_prefix, 0, PREFIX_SIZE);
            std::memcpy(_prefix, value.data(), PREFIX_SIZE <= value.size() ? PREFIX_SIZE : value.size());
        }

        FastStr(std::string_view value, int32_t record_id) : _record_id(record_id) {
            std::memset(_prefix, 0, PREFIX_SIZE);
            std::memcpy(_prefix, value.data(), PREFIX_SIZE <= value.size() ? PREFIX_SIZE : value.size());
        }

        FastStr() = default;

        bool operator<(FastStr other) const {
            int prefix_comp = std::memcmp(_prefix, other._prefix, PREFIX_SIZE);
            if (prefix_comp != 0) {
                return prefix_comp < 0;
            }
            return _record_id < other._record_id;
        }

        bool operator==(const FastStr& other) const {
            return _record_id == other._record_id && std::memcmp(_prefix, other._prefix, PREFIX_SIZE) == 0;
        }

        int SearchCmp(FastStr other) const { return std::memcmp(_prefix, other._prefix, PREFIX_SIZE); }

        friend std::ostream& operator<<(std::ostream& os, const FastStr& fast_str) {
            for (int i = 0; i < PREFIX_SIZE && fast_str._prefix[i]; ++i) {
                os << fast_str._prefix[i];
            }
            os << "(record_id=" << fast_str._record_id << ")";
            return os;
        }
    };

    std::string _name;
    fs::path _root;
    Pager _pager;
    StringStorage _str_storage;
    Interner* _interner;
    Serializer _serializer;
    Schema _schema;
    Journal _journal;
    BStarPlusTree<uint32_t, RecordAddress> _id_to_addr;
    std::unordered_map<
        std::string,
        std::variant<BStarPlusTree<int32_t, RecordAddress>, BStarPlusTree<FastStr, RecordAddress>>>
        _indexes;

    struct MetadataStruct {
        char header[HEADER.size() + 1];
    };

    union MetadataPage {
        MetadataStruct metadata;
        uint8_t raw[PAGE_SIZE];

        MetadataPage() : metadata() {}
    };

    static_assert(sizeof(MetadataPage) == PAGE_SIZE);

public:
    Table(std::string name, fs::path root, Interner* interner)
        : _name(std::move(name)),
          _root(std::move(root)),
          _pager(_root / (_name + std::string(DATA_EXT)), PAGE_SIZE),
          _str_storage(_root / (_name + std::string(STR_STORAGE_EXT))),
          _interner(interner),
          _serializer(_interner, MAX_SMALL_STR_LENGTH),
          _journal(_root, _name),
          _id_to_addr(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT))) {
        // read table
        if (DEBUG) {
            std::cout << "Table::Table(read)" << std::endl;
        }
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
        auto schema = Schema::from_binary(ifs);
        if (schema) {
            _schema = std::move(*schema);
        } else {
            throw std::runtime_error("Can not read schema for table with name " + _name + ".");
        }
    }

    Table(std::string name, fs::path root, Schema schema, Interner* interner)
        : _name(std::move(name)),
          _root(std::move(root)),
          _pager(_root / (_name + std::string(DATA_EXT)), PAGE_SIZE),
          _str_storage(_root / (_name + std::string(STR_STORAGE_EXT))),
          _interner(interner),
          _serializer(_interner, MAX_SMALL_STR_LENGTH),
          _schema(std::move(schema)),
          _journal(_root, _name),
          _id_to_addr(_root / (_name + "_id_to_addr" + std::string(INDEX_EXT))) {
        // create table
        if (DEBUG) {
            std::cout << "Table::Table(create)" << std::endl;
        }
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

        for (size_t i = 0; i < _schema.size(); ++i) {
            if (_schema[i].indexed()) {
                const std::string& column_name = _schema[i].name();
                auto column_index_path = index_path(column_name);
                if (_schema[i].is_int()) {
                    _indexes.emplace(column_name, BStarPlusTree<int, RecordAddress>(column_index_path));
                } else if (_schema[i].is_string()) {
                    _indexes.emplace(column_name, BStarPlusTree<FastStr, RecordAddress>(column_index_path));
                }
            }
        }
    }

    Table(Table&&) noexcept = default;
    Table& operator=(Table&&) noexcept = default;

    ~Table() noexcept { close(); }

private:
    void save_schema() {
        if (DEBUG) {
            std::cout << "Table::save_schema" << std::endl;
        }
        fs::path temp_path = _root / (_name + std::string(SCHEMA_TMP_EXT));
        std::ofstream ofs(temp_path, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Cannot create schema file.");
        }
        if (!_schema.to_binary(ofs)) {
            throw std::runtime_error("Cannot write schema to file.");
        }
        ofs.close();
        fs::path path_to_schema = _root / (_name + std::string(SCHEMA_EXT));
        fs::rename(temp_path, path_to_schema);
    }

    fs::path index_path(const std::string& column_name) {
        return _root / (_name + "_" + column_name + std::string(INDEX_EXT));
    }

public:
    void drop() {
        if (DEBUG) {
            std::cout << "Table::drop" << std::endl;
        }
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

    void close() noexcept {
        _pager.close();
        _str_storage.close();
        for (auto& [column_name, index] : _indexes) {
            std::visit([](auto& tree) { return tree.close(); }, index);
        }
        _id_to_addr.close();
    }

    Record insert_record(std::vector<Value> values, const std::vector<std::string>& column_names = {}) {
        if (DEBUG) {
            std::cout << "Table::insert_record" << std::endl;
        }
        uint32_t record_id = _schema.record_id_count();
        Record record = make_record(record_id, std::move(values), column_names);
        validate_record(record);
        _schema.increment_record_id_count();
        save_schema();
        write_record_to_disk(record);
        update_indexes_after_insert(record);
        _id_to_addr.insert(record.id(), record.address());
        _journal.save_insertion(record);
        return record;
    }

    std::vector<Record> insert_multiple(
        std::vector<std::vector<Value>> rows,
        const std::vector<std::string>& column_names = {}
    ) {
        if (DEBUG) {
            std::cout << "Table::insert_multiple" << std::endl;
        }
        std::vector<Record> result;
        for (auto& values : rows) {
            result.emplace_back(insert_record(std::move(values), column_names));
        }
        return result;
    }

    // std::vector<Record> find_records();

    std::optional<Record> read_record(RecordAddress record_address, void* mem_ptr = nullptr) {
        if (DEBUG) {
            std::cout << "Table::read_record" << std::endl;
        }
        if (!_pager.page_exists(record_address.page_idx)) {
            return std::nullopt;
        }
        auto table_page = TablePage(record_address.page_idx, &_pager, &_str_storage, &_serializer, mem_ptr);
        auto record = table_page.read_record(record_address.slot_idx, _schema);
        if (record == std::nullopt) {
            return std::nullopt;
        }
        record->set_address(record_address);
        return record;
    }

    RecordAddress update_record(Record& record) {
        // Note: this method update record.address in-place
        if (DEBUG) {
            std::cout << "Table::update_record" << std::endl;
        }
        validate_record(record);
        auto old_record_addr = record.address();
        auto old_record = *read_record(old_record_addr);
        delete_record(record);
        auto new_record_addr = write_record_to_disk(record, old_record_addr.page_idx);
        if (old_record_addr != new_record_addr) {
            update_indexes_after_update(record);
            _id_to_addr.update(record.id(), record.address());
        }
        _journal.save_updation(old_record, _schema, &_serializer);
        return new_record_addr;
    }

    std::vector<std::pair<int, std::string>> update_multiple(std::vector<Record>& records) {
        if (DEBUG) {
            std::cout << "Table::update_multiple" << std::endl;
        }
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

    void delete_record(const Record& record) {
        if (DEBUG) {
            std::cout << "Table::delete_record" << std::endl;
        }
        auto table_page = TablePage(record.address().page_idx, &_pager, &_str_storage, &_serializer);
        table_page.delete_record(record.address().slot_idx);
        update_indexes_after_delete(record);
        _id_to_addr.remove(record.id());
        _journal.save_deletion(record, _schema, &_serializer);
    }

    void delete_multiple(const std::vector<Record>& records) {
        if (DEBUG) {
            std::cout << "Table::delete_multiple" << std::endl;
        }
        for (const auto& record : records) {
            delete_record(record);
        }
    }

    void revert(const std::string& time) {
        auto revert_data = _journal.revert_last(time, _schema, &_str_storage, &_serializer);
        while (!revert_data.time.empty()) {
            auto& record = revert_data.record;
            if (revert_data.type == Journal::Track::Type::INSERT) {
                write_record_to_disk(record);
                update_indexes_after_insert(record);
                _id_to_addr.insert(record.id(), record.address());
            } else if (revert_data.type == Journal::Track::Type::UPDATE) {
                assert(_id_to_addr.search(record.id()).size() == 1);
                auto old_record_addr = _id_to_addr.search(record.id()).back();
                record.set_address(old_record_addr);
                delete_record(record);
                auto new_record_addr = write_record_to_disk(record, record.address().page_idx);
                if (old_record_addr != new_record_addr) {
                    update_indexes_after_update(record);
                    _id_to_addr.update(record.id(), record.address());
                }
            } else if (revert_data.type == Journal::Track::Type::DELETE) {
                assert(_id_to_addr.search(record.id()).size() == 1);
                record.set_address(_id_to_addr.search(record.id()).back());
                auto table_page = TablePage(record.address().page_idx, &_pager, &_str_storage, &_serializer);
                table_page.delete_record(record.address().slot_idx);
                update_indexes_after_delete(record);
                _id_to_addr.remove(record.id());
                assert(record.id() + 1 == _schema.record_id_count());
                _schema.decrement_record_id_count();
            }
            revert_data = _journal.revert_last(time, _schema, &_str_storage, &_serializer);
        }
        save_schema();
    }

private:
    Record make_record(uint32_t record_id, std::vector<Value> values, const std::vector<std::string>& column_names) {
        if (DEBUG) {
            std::cout << "Table::make_record" << std::endl;
        }
        if (column_names.empty()) {
            // INSERT INTO table VALUES (val1, val2, ...)
            if (values.size() != _schema.size()) {
                throw std::runtime_error(
                    "Column count mismatch: expected " + std::to_string(_schema.size()) + ", got " +
                    std::to_string(values.size())
                );
            }
            return Record(record_id, std::move(values));
        }
        // INSERT INTO table (col1, col2) VALUES (val1, val2)
        if (values.size() != column_names.size()) {
            throw std::runtime_error(
                "Values size and columns size mismatch: " + std::to_string(values.size()) +
                " != " + std::to_string(column_names.size()) + "."
            );
        }
        auto column_indices = resolve_column_indices(column_names);

        Record record(record_id, _schema.size());
        apply_defaults(record, column_indices);
        update_columns(record, values, column_indices);
        return record;
    }

    std::vector<size_t> resolve_column_indices(const std::vector<std::string>& column_names) const {
        std::vector<size_t> column_indices;
        column_indices.reserve(column_names.size());
        for (const auto& column_name : column_names) {
            auto col_index = _schema.get_column_idx(column_name);
            if (col_index == -1) {
                throw std::runtime_error("Unknown column: " + column_name);
            }
            column_indices.push_back(static_cast<size_t>(col_index));
        }
        return column_indices;
    }

    void apply_defaults(Record& record, const std::vector<size_t>& provided_column_indices) {
        if (DEBUG) {
            std::cout << "Table::apply_defaults" << std::endl;
        }
        for (size_t i = 0; i < _schema.size(); ++i) {
            if (_schema[i].has_default() &&
                std::find(provided_column_indices.begin(), provided_column_indices.end(), i) ==
                    provided_column_indices.end())
            {
                record[i] = _schema[i].default_value(*_interner);
            }
        }
    }

    void update_some_columns(Record& record, std::vector<Value> values, const std::vector<std::string>& column_names)
        const {
        if (DEBUG) {
            std::cout << "Table::update_some_columns" << std::endl;
        }
        for (size_t i = 0; i < column_names.size(); ++i) {
            auto col_index = _schema.get_column_idx(column_names[i]);
            if (col_index == -1) {
                throw std::runtime_error("Unknown column: " + column_names[i]);
            }
            record[col_index] = std::move(values[i]);
        }
    }

    void update_columns(Record& record, std::vector<Value> values, const std::vector<size_t>& column_indices) {
        if (DEBUG) {
            std::cout << "Table::update_columns" << std::endl;
        }
        for (size_t i = 0; i < column_indices.size(); ++i) {
            record[column_indices[i]] = std::move(values[i]);
        }
    }

    void validate_record(const Record& record) {
        if (DEBUG) {
            std::cout << "Table::validate_record" << std::endl;
        }
        for (size_t i = 0; i < _schema.size(); ++i) {
            const auto& column = _schema[i];
            const auto& value = record[i];
            if (column.not_null() && value.is_null()) {
                throw std::runtime_error("Column '" + column.name() + "' cannot be NULL (NOT_NULL constraint)");
            }
            if (!value.is_null()) {
                if (column.is_int() && !value.is_int()) {
                    throw std::runtime_error("Column '" + column.name() + "' expects INT, got " + value.type_name());
                }
                if (column.is_string() && !value.is_string()) {
                    throw std::runtime_error("Column '" + column.name() + "' expects STRING, got " + value.type_name());
                }
            }
            if (column.indexed()) {
                auto it = _indexes.find(column.name());
                if (it == _indexes.end()) {
                    throw std::runtime_error("Index not found for indexed column");
                }
                bool dublicate_found = std::visit(
                    [&](auto& tree) -> bool {
                        using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                        if constexpr (std::is_same_v<KeyType, int32_t>) {
                            auto results = tree.search(value.as_int());
                            for (auto addr : results) {
                                if (!record.has_addr() || addr != record.address()) {
                                    return true;
                                }
                            }
                        } else if constexpr (std::is_same_v<KeyType, FastStr>) {
                            auto results = tree.search(FastStr(value.as_string().intern_view, 0));
                            if (results.empty()) {
                                return false;
                            }
                            auto page_buf = std::make_unique<uint8_t[]>(PAGE_SIZE);
                            for (auto addr : results) {
                                auto prob_record = *read_record(addr, page_buf.get());
                                if (prob_record[i].StrictEq(value) && (!record.has_addr() || addr != record.address()))
                                {
                                    return true;
                                }
                            }
                        }
                        return false;
                    },
                    it->second
                );
                if (dublicate_found) {
                    throw std::runtime_error(
                        "Duplicate value for indexed column '" + column.name() + "': " + value.to_string()
                    );
                }
            }
        }
    }

    RecordAddress write_record_to_disk(Record& record, uint32_t prefer_page_id = 0) {
        // Note: set new record_address to record
        if (DEBUG) {
            std::cout << "Table::write_record_to_disk" << std::endl;
        }
        for (uint32_t i = 0; i < record.size(); ++i) {
            if (record[i].is_string() && !record[i].as_string().has_ext_addr &&
                record[i].as_string().intern_view.size() > MAX_SMALL_STR_LENGTH)
            {
                auto ext_addr = _str_storage.append(record[i].as_string().intern_view);
                record[i] = _interner->str_to_value(record[i].as_string().intern_view, ext_addr);
            }
        }
        auto serialized_record = record.serialized(_schema, &_serializer);
        uint32_t serialized_size = serialized_record.size();
        auto table_page = find_enough_free_page(serialized_size, prefer_page_id);
        uint32_t slot_idx = table_page.insert_record(record.id(), serialized_record.data(), serialized_size);
        RecordAddress record_address = {table_page.id(), slot_idx};
        record.set_address(record_address);
        return record_address;
    }

    TablePage find_enough_free_page(uint32_t free_space, uint32_t prefer_page_id = 0) {
        if (DEBUG) {
            std::cout << "Table::find_enough_free_page" << std::endl;
        }
        if (free_space > TablePage::MAX_RECORD_SIZE) {
            throw std::runtime_error("free_space > TablePage::MAX_RECORD_SIZE.");
        }
        std::vector<uint8_t> page_mem;
        page_mem.resize(PAGE_SIZE);
        auto total_pages = _pager.get_total_pages();
        if (prefer_page_id > 0 && prefer_page_id < total_pages) {
            auto table_page = TablePage(prefer_page_id, &_pager, &_str_storage, &_serializer, page_mem.data());
            if (table_page.is_record_fit(free_space)) {
                return TablePage(table_page.id(), &_pager, &_str_storage, &_serializer);
            }
        }
        for (auto i = DATA_PAGE_WITH; i < total_pages; ++i) {
            auto table_page = TablePage(i, &_pager, &_str_storage, &_serializer, page_mem.data());
            if (table_page.is_record_fit(free_space)) {
                return TablePage(table_page.id(), &_pager, &_str_storage, &_serializer);
            }
        }
        return TablePage::create(&_pager, &_str_storage, &_serializer);
    }

    void update_indexes_after_insert(const Record& record) {
        if (DEBUG) {
            std::cout << "Table::update_indexes_after_insert" << std::endl;
        }
        for (size_t i = 0; i < _schema.size(); ++i) {
            const auto& column = _schema[i];
            if (column.indexed()) {
                const auto& value = record[i];
                auto it = _indexes.find(column.name());
                if (it == _indexes.end()) {
                    throw std::runtime_error("Index not found for column " + column.name() + ".");
                }
                std::visit(
                    [&](auto& tree) {
                        using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                        if constexpr (std::is_same_v<KeyType, int32_t>) {
                            tree.insert(value.as_int(), record.address());
                        } else if constexpr (std::is_same_v<KeyType, FastStr>) {
                            tree.insert(FastStr(value.as_string().intern_view, record.id()), record.address());
                        }
                    },
                    it->second
                );
            }
        }
    }

    void update_indexes_after_delete(const Record& record) {
        if (DEBUG) {
            std::cout << "Table::update_indexes_after_delete" << std::endl;
        }
        for (size_t i = 0; i < _schema.size(); ++i) {
            const auto& column = _schema[i];
            if (column.indexed()) {
                const auto& value = record[i];
                auto it = _indexes.find(column.name());
                if (it == _indexes.end()) {
                    throw std::runtime_error("Index not found for column " + column.name() + ".");
                }
                std::visit(
                    [&](auto& tree) {
                        using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                        if constexpr (std::is_same_v<KeyType, int32_t>) {
                            tree.remove(value.as_int());
                        } else if constexpr (std::is_same_v<KeyType, FastStr>) {
                            tree.remove(FastStr(value.as_string().intern_view, record.id()));
                        }
                    },
                    it->second
                );
            }
        }
    }

    void update_indexes_after_update(const Record& record) {
        if (DEBUG) {
            std::cout << "Table::update_indexes_after_update" << std::endl;
        }
        for (size_t i = 0; i < _schema.size(); ++i) {
            const auto& column = _schema[i];
            if (column.indexed()) {
                const auto& value = record[i];
                auto it = _indexes.find(column.name());
                if (it == _indexes.end()) {
                    throw std::runtime_error("Index not found for column " + column.name() + ".");
                }
                std::visit(
                    [&](auto& tree) {
                        using KeyType = typename std::decay_t<decltype(tree)>::key_type;
                        if constexpr (std::is_same_v<KeyType, int32_t>) {
                            tree.update(value.as_int(), record.address());
                        } else if constexpr (std::is_same_v<KeyType, FastStr>) {
                            tree.update(FastStr(value.as_string().intern_view, record.id()), record.address());
                        }
                    },
                    it->second
                );
            }
        }
    }

public:
    std::string name() const { return _name; }

    auto schema() const { return _schema; }
};

}  // namespace qdb::storage

#endif  // QUASARDB_TABLE_H
