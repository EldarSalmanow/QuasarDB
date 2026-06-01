#include <qdb/storage/index_set.h>

namespace qdb::storage {

IndexSet::IndexSet(std::filesystem::path root, std::string table_name)
    : root_(std::move(root)), table_name_(std::move(table_name)) {}

void IndexSet::Open(const Schema& schema) {
    indexes_.clear();
    for (std::size_t i = 0; i < schema.Size(); ++i) {
        const auto& column = schema[i];
        if (!column.IsIndexed()) {
            continue;
        }
        if (column.IsInt()) {
            indexes_.emplace(column.Name(), BStarPlusTree<int32_t, RecordAddress>(PathFor(column.Name())));
        } else if (column.IsString()) {
            indexes_.emplace(column.Name(), BStarPlusTree<StringId, RecordAddress>(PathFor(column.Name())));
        }
    }
}

void IndexSet::Close() noexcept {
    for (auto& [_, index] : indexes_) {
        std::visit([](auto& tree) { tree.close(); }, index);
    }
}

void IndexSet::Drop() {
    std::vector<std::filesystem::path> paths;
    paths.reserve(indexes_.size());
    for (const auto& [_, index] : indexes_) {
        paths.push_back(std::visit([](const auto& tree) { return tree.path(); }, index));
    }
    indexes_.clear();
    for (const auto& path : paths) {
        std::filesystem::remove(path);
    }
}

auto IndexSet::Find(const Schema& schema, const std::string& column_name, const Value& value)
    -> std::optional<std::vector<RecordAddress>> {
    const auto column_index = schema.ColumnIndex(column_name);
    if (column_index < 0) {
        throw std::runtime_error("Unknown column: " + column_name);
    }

    const auto& column = schema[column_index];
    if (!column.IsIndexed() || value.IsNull()) {
        return std::nullopt;
    }
    if ((column.IsInt() && !value.IsInt()) || (column.IsString() && !value.IsString())) {
        return std::nullopt;
    }

    auto it = indexes_.find(column_name);
    if (it == indexes_.end()) {
        throw std::runtime_error("Index not found for column " + column_name + ".");
    }

    return std::visit(
        [&](auto& tree) -> std::vector<RecordAddress> {
            using Key = typename std::decay_t<decltype(tree)>::key_type;
            if constexpr (std::is_same_v<Key, int32_t>) {
                return tree.search(value.AsInt());
            } else {
                return tree.search(value.AsString());
            }
        },
        it->second
    );
}

auto IndexSet::HasDuplicate(
    const Schema& schema,
    const Record& record,
    std::size_t column_index,
    const Value& value,
    const Reader& reader
) -> bool {
    bool found = false;
    ForEach(schema, record, [&](std::size_t i, const Column&, const Value& indexed_value, Tree& index) {
        if (i != column_index || indexed_value.IsNull()) {
            return;
        }
        found = std::visit(
            [&](auto& tree) {
                using Key = typename std::decay_t<decltype(tree)>::key_type;
                auto addresses = [&] {
                    if constexpr (std::is_same_v<Key, int32_t>) {
                        return tree.search(indexed_value.AsInt());
                    } else {
                        return tree.search(indexed_value.AsString());
                    }
                }();

                for (auto address : addresses) {
                    if (record.HasAddress() && address == record.Address()) {
                        continue;
                    }
                    auto existing = reader(address);
                    if (existing && (*existing)[column_index].StrictEq(value)) {
                        return true;
                    }
                }
                return false;
            },
            index
        );
    });
    return found;
}

void IndexSet::Insert(const Schema& schema, const Record& record) {
    ForEach(schema, record, [&](std::size_t, const Column&, const Value& value, Tree& index) {
        std::visit(
            [&](auto& tree) {
                using Key = typename std::decay_t<decltype(tree)>::key_type;
                if constexpr (std::is_same_v<Key, int32_t>) {
                    tree.insert(value.AsInt(), record.Address());
                } else {
                    tree.insert(value.AsString(), record.Address());
                }
            },
            index
        );
    });
}

void IndexSet::Remove(const Schema& schema, const Record& record) {
    ForEach(schema, record, [&](std::size_t, const Column&, const Value& value, Tree& index) {
        std::visit(
            [&](auto& tree) {
                using Key = typename std::decay_t<decltype(tree)>::key_type;
                if constexpr (std::is_same_v<Key, int32_t>) {
                    tree.remove(value.AsInt());
                } else {
                    tree.remove(value.AsString());
                }
            },
            index
        );
    });
}

void IndexSet::Update(const Schema& schema, const Record& old_record, const Record& new_record) {
    for (std::size_t i = 0; i < schema.Size(); ++i) {
        const auto& column = schema[i];
        if (!column.IsIndexed()) {
            continue;
        }
        auto it = indexes_.find(column.Name());
        if (it == indexes_.end()) {
            throw std::runtime_error("Index not found for column " + column.Name() + ".");
        }

        const auto& old_value = old_record[i];
        const auto& new_value = new_record[i];
        if (old_value.StrictEq(new_value) && old_record.Address() == new_record.Address()) {
            continue;
        }
        auto& index = it->second;
        std::visit(
            [&](auto& tree) {
                using Key = typename std::decay_t<decltype(tree)>::key_type;
                if constexpr (std::is_same_v<Key, int32_t>) {
                    if (!old_value.IsNull()) tree.remove(old_value.AsInt());
                    if (!new_value.IsNull()) tree.insert(new_value.AsInt(), new_record.Address());
                } else {
                    if (!old_value.IsNull()) tree.remove(old_value.AsString());
                    if (!new_value.IsNull()) tree.insert(new_value.AsString(), new_record.Address());
                }
            },
            index
        );
    }
}

auto IndexSet::PathFor(const std::string& column_name) const -> std::filesystem::path {
    return root_ / (table_name_ + "_" + column_name + ".idx");
}

void IndexSet::ForEach(const Schema& schema, const Record& record, const Visitor& visitor) {
    for (std::size_t i = 0; i < schema.Size(); ++i) {
        const auto& column = schema[i];
        if (!column.IsIndexed() || record[i].IsNull()) {
            continue;
        }
        auto it = indexes_.find(column.Name());
        if (it == indexes_.end()) {
            throw std::runtime_error("Index not found for column " + column.Name() + ".");
        }
        visitor(i, column, record[i], it->second);
    }
}

}  // namespace qdb::storage
