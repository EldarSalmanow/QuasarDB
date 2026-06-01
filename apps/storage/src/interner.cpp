#include <qdb/storage/interner.h>

#include <stdexcept>

namespace qdb::storage {

Interner::Interner() = default;

Interner::Interner(Interner&& other) noexcept = default;

auto Interner::UseStorage(const std::filesystem::path& path) -> void {
    Close();

    path_ = path;
    storage_.open(path_, std::ios::in | std::ios::out | std::ios::binary);

    if (!storage_.is_open()) {
        storage_.open(path_, std::ios::out | std::ios::binary | std::ios::trunc);

        if (!storage_.is_open()) {
            throw std::runtime_error(
                "[ERROR in qdb::storage::Interner]: "
                "Cannot create file '" +
                path_.string() + "'!"
            );
        }

        storage_.close();
        storage_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!storage_.is_open()) {
        throw std::runtime_error(
            "[ERROR in qdb::storage::Interner]: "
            "Cannot open file '" +
            path_.string() + "'!"
        );
    }

    LoadStorage();
}

auto Interner::View(const StringId& id) -> std::string_view {
    if (auto iterator = id_to_view_.find(id.value); iterator != id_to_view_.end()) {
        return iterator->second;
    }

    throw std::runtime_error(
        "[ERROR in qdb::storage::Interner]: "
        "Unknown string id '" +
        std::to_string(id.value) + "'!"
    );
}

auto Interner::Intern(std::string_view str_view) -> Value {
    auto key = std::string(str_view);

    if (auto iterator = string_to_id_.find(key); iterator != string_to_id_.end()) {
        return Value(iterator->second);
    }

    StringId id{next_id_++};

    if (storage_.is_open()) {
        AppendToDisk(id, str_view);
    }

    string_storage_.emplace_back(str_view);

    auto view = std::string_view(string_storage_.back());

    string_to_id_.emplace(std::move(key), id);
    id_to_view_.emplace(id.value, view);

    return Value{id};
}

auto Interner::Get(const StringId& id) -> Value {
    (void)View(id);

    return Value(id);
}

auto Interner::Close() noexcept -> void {
    if (storage_.is_open()) {
        storage_.close();
    }
}

auto Interner::DeleteStorage() -> void {
    Close();

    if (!path_.empty()) {
        std::filesystem::remove(path_);
    }
}

auto Interner::LoadStorage() -> void {
    string_storage_.clear();
    string_to_id_.clear();
    id_to_view_.clear();
    next_id_ = 1;

    storage_.clear();
    storage_.seekg(0, std::ios::beg);

    while (true) {
        uint32_t stored_id = 0;
        uint64_t size = 0;

        if (!storage_.read(reinterpret_cast<char*>(&stored_id), sizeof(stored_id))) {
            storage_.clear();

            return;
        }

        if (!storage_.read(reinterpret_cast<char*>(&size), sizeof(size))) {
            throw std::runtime_error("[ERROR in qdb::storage::Interner]: Corrupted string storage!");
        }

        std::string value(size, '\0');

        if (!storage_.read(value.data(), size)) {
            throw std::runtime_error("[ERROR in qdb::storage::Interner]: Corrupted string storage!");
        }

        AddLoadedString(StringId{stored_id}, std::move(value));
    }
}

auto Interner::AddLoadedString(const StringId& id, std::string value) -> std::string_view {
    next_id_ = std::max(next_id_, id.value + 1);
    string_storage_.push_back(std::move(value));

    auto view = std::string_view(string_storage_.back());

    id_to_view_.emplace(id.value, view);
    string_to_id_.emplace(std::string(view), id);

    return view;
}

auto Interner::AppendToDisk(const StringId& id, std::string_view str) -> void {
    storage_.clear();
    storage_.seekp(0, std::ios::end);

    uint64_t size = str.size();
    if (!storage_.write(reinterpret_cast<const char*>(&id.value), sizeof(id.value)) ||
        !storage_.write(reinterpret_cast<const char*>(&size), sizeof(size)) ||
        !storage_.write(str.data(), static_cast<std::streamsize>(str.size())))
    {
        throw std::runtime_error("[ERROR in qdb::storage::Interner]: Failed to write string data!");
    }

    storage_.flush();

    if (storage_.fail()) {
        throw std::runtime_error("[ERROR in qdb::storage::Interner]: Cannot append string to file!");
    }
}

}  // namespace qdb::storage
