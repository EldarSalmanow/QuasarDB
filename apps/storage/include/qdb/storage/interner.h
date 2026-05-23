#ifndef QUASARDB_INTERNER_H
#define QUASARDB_INTERNER_H

#include "value.h"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>


namespace qdb::storage {

class Interner {
public:
    Interner() = default;

    Interner(const Interner& other) = delete;

    Interner(Interner&& other) noexcept = default;

public:
    std::string_view intern(std::string_view str);

    Value str_to_value(std::string_view str_view, std::optional<ExternalString> ext_addr = std::nullopt);

public:
    Interner& operator=(const Interner& other) = delete;

    Interner& operator=(Interner&& other) noexcept = default;

private:
    std::unordered_set<std::string_view> _str_view_set;

    std::deque<std::string> _str_storage;
};

}  // namespace qdb::storage

#endif  // QUASARDB_INTERNER_H