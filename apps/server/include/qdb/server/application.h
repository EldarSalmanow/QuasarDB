#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <qdb/core/request.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_server.h>
#include <qdb/server/catalog.h>
#include <qdb/server/config.h>
#include <qdb/server/logger.h>
#include <qdb/server/monitor.h>
#include <qdb/server/rbac.h>
#include <qdb/server/router.h>
#include <qdb/server/security.h>
#include <qdb/server/task_processor.h>
#include <qdb/server/telemetry.h>

#include <memory>
#include <optional>
#include <string>

namespace qdb::server {

struct Session {
    std::optional<std::string> database;
};

class Application {
public:
    explicit Application(Config config);

public:
    static auto New(Config config) -> std::unique_ptr<Application>;

public:
    auto Run() -> std::int32_t;

    auto Process(const qdb::core::Request& request) -> qdb::core::Response;
    auto Process(const qdb::core::Request& request, Session& session) -> qdb::core::Response;

private:
    auto HandleHandshake() const -> qdb::core::Response;

    auto HandleLogin(const qdb::core::Request& request) -> qdb::core::Response;

    auto HandleExecute(const qdb::core::Request& request, Session& session) -> qdb::core::Response;

    auto HandleCheckTask(const qdb::core::Request& request) -> qdb::core::Response;

    auto HandleTelemetry(const qdb::core::Request& request) const -> qdb::core::Response;

    auto Authenticate(const qdb::core::Request& request) const -> std::optional<std::string>;

    auto CheckAccess(const std::string& user, const Statement& statement) const -> bool;

    auto HandleSecurityStatement(const std::string& user, const Statement& statement)
        -> std::optional<qdb::core::Response>;

private:
    Config config_;

    std::unique_ptr<qdb::core::TcpServer> server_;

    AccountStore accounts_;

    JwtHandler jwt_;

    RBACManager rbac_;

    Logger logger_;

    Telemetry telemetry_;

    Catalog catalog_;

    std::shared_ptr<Registry> registry_;

    Router router_;

    Monitor monitor_;

    TaskProcessor tasks_;
};

}  // namespace qdb::server

#endif  // QUASARDB_APPLICATION_H
