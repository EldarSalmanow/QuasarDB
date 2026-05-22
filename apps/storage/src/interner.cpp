#include "../include/qdb/storage/interner.h"

namespace qdb::storage {

std::string_view Interner::intern(std::string_view str) {
    auto it = _str_view_set.find(str);

    if (it != _str_view_set.end()) {
        return *it;
    }

    _str_storage.emplace_back(str);

    std::string_view new_view(_str_storage.back());

    _str_view_set.insert(new_view);

    return new_view;
}

Value Interner::str_to_value(std::string_view str_view, std::optional<ExternalString> ext_addr) {
    auto view = intern(str_view);

    return Value(InternedString{
        .ext_addr = ext_addr.value_or(ExternalString{0, 0}),
        .intern_view = view,
        .has_ext_addr = ext_addr.has_value()});
}

}  // namespace qdb::storage
