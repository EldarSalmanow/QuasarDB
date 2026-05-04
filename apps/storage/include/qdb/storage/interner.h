//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_INTERNER_H
#define QUASARDB_INTERNER_H

#include <deque>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include "value.h"

namespace qdb::storage {

class Interner {
    std::unordered_set<std::string_view> _str_view_set;
    std::deque<std::string> _str_storage;

public:
    std::string_view intern(std::string_view str) {
        auto it = _str_view_set.find(str);
        if (it != _str_view_set.end()) {
            return *it;
        }
        _str_storage.emplace_back(str);
        std::string_view new_view(_str_storage.back());
        _str_view_set.insert(new_view);
        return new_view;
    }

    Value str_to_value(std::string_view str_view, std::optional<ExternalString> ext_addr = std::nullopt) {
        auto view = intern(str_view);
        return Value(InternedString{
            .ext_addr = ext_addr.value_or(ExternalString{0, 0}),
            .intern_view = view,
            .has_ext_addr = ext_addr.has_value()});
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_INTERNER_H