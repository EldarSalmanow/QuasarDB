#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <nlohmann/json.hpp>

#include <qdb/core/tcp_server.h>
#include <qdb/core/tcp_client.h>
#include <qdb/server/ast.h>
#include <qdb/storage/config.h>
#include <qdb/storage/interner.h>
#include <qdb/storage/table.h>

#include <memory>
#include <optional>


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

    auto CreateTable(const qdb::server::CreateTableStmt& statement) -> nlohmann::json;

    auto DropTable(const qdb::server::DropTableStmt& statement) -> nlohmann::json;

    auto OpenExistingTable() -> void;

private:
    Config config_;

    std::unique_ptr<qdb::core::TcpServer> server_;

    std::unique_ptr<qdb::core::TcpClient> connection_;

    Interner interner_;

    std::unique_ptr<Table> table_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_APPLICATION_H
