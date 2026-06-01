#ifndef QUASARDB_INTERNER_H
#define QUASARDB_INTERNER_H

#include <qdb/storage/value.h>

#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace qdb::storage {

class Interner {
public:
    Interner();

public:
    Interner(const Interner& other) = delete;

    Interner(Interner&& other) noexcept;

public:
    auto UseStorage(const std::filesystem::path& path) -> void;

    auto Close() noexcept -> void;

    auto DeleteStorage() -> void;

    auto View(const StringId& id) -> std::string_view;

    auto Intern(std::string_view str_view) -> Value;

    auto Get(const StringId& id) -> Value;

public:
    auto operator=(const Interner& other) -> Interner& = delete;

    auto operator=(Interner&& other) noexcept -> Interner& = default;

private:
    auto LoadStorage() -> void;

    auto AddLoadedString(const StringId& id, std::string value) -> std::string_view;

    auto AppendToDisk(const StringId& id, std::string_view string) -> void;

private:
    std::deque<std::string> string_storage_;
    std::unordered_map<std::string, StringId> string_to_id_;
    std::unordered_map<uint32_t, std::string_view> id_to_view_;
    std::filesystem::path path_;
    std::fstream storage_;
    uint32_t next_id_{1};
};

}  // namespace qdb::storage

#endif  // QUASARDB_INTERNER_H
