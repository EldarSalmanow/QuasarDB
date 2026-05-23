#ifndef QUASARDB_MONITOR_H
#define QUASARDB_MONITOR_H

#include <qdb/storage/registry.h>

namespace qdb::server {

class Monitor {
public:

    Monitor();

public:
    static auto New() -> std::unique_ptr<Monitor>;

private:

};

}

#endif  // QUASARDB_MONITOR_H
