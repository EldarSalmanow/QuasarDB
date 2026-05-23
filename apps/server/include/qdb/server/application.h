#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <qdb/core/request.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_server.h>
#include <qdb/server/async_status.h>
#include <qdb/server/config.h>
#include <qdb/server/monitor.h>
#include <qdb/server/router.h>
#include <qdb/server/security.h>

#include <memory>
#include <optional>
#include <string>


namespace qdb::server {

class Application {
public:
    explicit Application(Config config);

public:
    static auto New(Config config) -> std::unique_ptr<Application>;

public:
    auto Run() -> std::int32_t;

    auto Process(const qdb::core::Request& request) -> qdb::core::Response;

private:
    auto HandleLogin(const qdb::core::Request& request) -> qdb::core::Response;

    auto HandleExecute(const qdb::core::Request& request) -> qdb::core::Response;

    auto HandleCheckTask(const qdb::core::Request& request) -> qdb::core::Response;

    auto Authenticate(const qdb::core::Request& request) const -> std::optional<std::string>;

private:
    Config config_;

    std::unique_ptr<qdb::core::TcpServer> server_;

    AccountStore accounts_;

    JwtHandler jwt_;

    std::shared_ptr<Registry> registry_;

    Router router_;

    Monitor monitor_;

    TaskTracker tasks_;
};

}  // namespace qdb::server

#endif  // QUASARDB_APPLICATION_H
