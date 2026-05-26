#ifndef QUASARDB_TABLE_H
#define QUASARDB_TABLE_H

#include "b_star_plus_tree.h"
#include "journal.h"
#include "page.h"
#include "pager.h"
#include "record.h"
#include "schema.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

namespace fs = std::filesystem;

namespace qdb::storage {

class Table final {
    static constexpr std::string_view HEADER = "TABLE";
    static constexpr uint32_t PAGE_SIZE = 4096;
    static constexpr uint32_t METADATA_PAGE_ID = 0;
    static constexpr uint32_t DATA_PAGE_WITH = METADATA_PAGE_ID + 1;
    static constexpr std::string_view DATA_EXT = ".data";
    static constexpr std::string_view SCHEMA_EXT = ".schema";
    static constexpr std::string_view SCHEMA_TMP_EXT = ".schema.tmp";
    static constexpr std::string_view INDEX_EXT = ".idx";
    static constexpr std::string_view STR_STORAGE_EXT = ".bin";

private:
    std::string _name;
    fs::path _root;
    Pager _pager;
    Interner* _interner;
    Schema _schema;
    Journal _journal;
    BStarPlusTree<uint32_t, RecordAddress> _id_to_addr;
    using IndexTree = std::variant<BStarPlusTree<int32_t, RecordAddress>, BStarPlusTree<StringId, RecordAddress>>;
    std::unordered_map<std::string, IndexTree> _indexes;

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
    Table(std::string name, fs::path root, Interner* interner);

    Table(std::string name, fs::path root, Schema schema, Interner* interner);

    Table(Table&&) noexcept = default;
    Table& operator=(Table&&) noexcept = default;

    ~Table() noexcept;

private:
    void save_schema();

    fs::path index_path(const std::string& column_name);

    void open_indexes();

public:
    void drop();

    void close() noexcept;

    Record insert_record(std::vector<Value> values, const std::vector<std::string>& column_names = {});

    std::vector<Record> insert_multiple(
        std::vector<std::vector<Value>> rows,
        const std::vector<std::string>& column_names = {}
    );

    std::optional<Record> read_record(RecordAddress record_address);

    std::optional<Record> read_by_id(uint32_t record_id);

    std::vector<Record> records();

    std::optional<std::vector<Record>> find_by_index(const std::string& column_name, const Value& value);

    RecordAddress update_record(Record& record);

    std::vector<std::pair<int, std::string>> update_multiple(std::vector<Record>& records);

    void delete_record(const Record& record);

    void delete_multiple(const std::vector<Record>& records);

    void revert(const std::string& time);

private:
    Record make_record(uint32_t record_id, std::vector<Value> values, const std::vector<std::string>& column_names);

    std::vector<size_t> resolve_column_indices(const std::vector<std::string>& column_names) const;

    void apply_defaults(Record& record, const std::vector<size_t>& provided_column_indices);

    void update_columns(Record& record, std::vector<Value> values, const std::vector<size_t>& column_indices);

    void validate_record(const Record& record);

    RecordAddress write_record_to_disk(Record& record, uint32_t prefer_page_id = 0);

    TablePage find_enough_free_page(uint32_t free_space, uint32_t prefer_page_id = 0);

    void for_each_indexed_value(
        const Record& record,
        const std::function<void(size_t, const Column&, const Value&, IndexTree&)>& action
    );

    void update_indexes_after_insert(const Record& record);

    void update_indexes_after_delete(const Record& record);

    void update_indexes_after_update(const Record& old_record, const Record& new_record);

public:
    std::string name() const;

    const Schema& schema() const;

};

}  // namespace qdb::storage

#endif  // QUASARDB_TABLE_H
