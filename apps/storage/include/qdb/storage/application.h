#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <memory>
#include <optional>
#include <qdb/core/tcp_server.h>
#include <qdb/core/tcp_client.h>
#include <qdb/storage/config.h>
#include <qdb/storage/db_manager.h>
#include <nlohmann/json.hpp>

namespace qdb::storage {

class Application final {
public:
    explicit Application(Config config);

public:
    static auto New(Config config) -> std::unique_ptr<Application>;

public:
    auto Run() -> std::int32_t;

private:
    auto AcceptConnection() -> std::unique_ptr<qdb::core::TcpClient>;

    auto ProcessRequest(const nlohmann::json& request) -> std::optional<nlohmann::json>;

    auto ExecuteAst(const nlohmann::json& ast) -> std::optional<nlohmann::json>;

private:
    Config config_;

    std::unique_ptr<qdb::core::TcpServer> server_;

    std::unique_ptr<qdb::core::TcpClient> connection_;

    DatabaseManager db_manager_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_APPLICATION_H