#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <qdb/client/config.h>

#include <memory>

namespace qdb::client {

class Application {
public:
    static auto New(const std::unique_ptr<Config>& config) -> std::unique_ptr<Application>;

public:
    auto Run() -> int;
};

}  // namespace qdb::client

#endif  // QUASARDB_APPLICATION_H
