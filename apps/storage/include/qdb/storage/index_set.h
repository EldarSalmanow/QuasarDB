#ifndef QUASARDB_INDEX_SET_H
#define QUASARDB_INDEX_SET_H

#include <qdb/storage/b_star_plus_tree.h>
#include <qdb/storage/record.h>
#include <qdb/storage/schema.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace qdb::storage {

class IndexSet final {
public:
    using Reader = std::function<std::optional<Record>(RecordAddress)>;

    IndexSet(std::filesystem::path root, std::string table_name);

    void Open(const Schema& schema);
    void Close() noexcept;
    void Drop();

    auto Find(const Schema& schema, const std::string& column_name, const Value& value)
        -> std::optional<std::vector<RecordAddress>>;
    auto HasDuplicate(
        const Schema& schema,
        const Record& record,
        std::size_t column_index,
        const Value& value,
        const Reader& reader
    ) -> bool;

    void Insert(const Schema& schema, const Record& record);
    void Remove(const Schema& schema, const Record& record);
    void Update(const Schema& schema, const Record& old_record, const Record& new_record);

private:
    using Tree = std::variant<BStarPlusTree<int32_t, RecordAddress>, BStarPlusTree<StringId, RecordAddress>>;
    using Visitor = std::function<void(std::size_t, const Column&, const Value&, Tree&)>;

    auto PathFor(const std::string& column_name) const -> std::filesystem::path;
    void ForEach(const Schema& schema, const Record& record, const Visitor& visitor);

    std::filesystem::path root_;
    std::string table_name_;
    std::unordered_map<std::string, Tree> indexes_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_INDEX_SET_H
