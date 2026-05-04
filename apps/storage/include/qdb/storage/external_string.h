#ifndef QUASARDB_EXTERNAL_STRING_H
#define QUASARDB_EXTERNAL_STRING_H

#include <cinttypes>

namespace qdb::storage {

struct ExternalString {
    uint64_t offset;
    uint64_t size;
};

}  // namespace qdb::storage

#endif  // QUASARDB_EXTERNAL_STRING_H