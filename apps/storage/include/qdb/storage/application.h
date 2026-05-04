#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <memory>
#include "../../../../../libs/core/include/qdb/core/tcp_client.h"
#include "db_manager.h"

namespace qdb::storage {

class Application final {
public:
    Application() = default;

public:
    auto Run() -> std::int32_t {
        connection_ = AcceptConnection();

        if (!connection_) {
            return 1;
        }

        while (true) {
            auto request = connection_->Receive();

            auto response = ProcessRequest(request);

            if (!request) {
                break;
            }
        }

        return 0;
    }

private:
    auto AcceptConnection() -> std::unique_ptr<qdb::core::TcpClient>;

    auto ProcessRequest(const nlohmann::json& request) -> std::optional<nlohmann::json>;

private:
    std::unique_ptr<qdb::core::TcpClient> connection_;

    DatabaseManager db_manager_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_APPLICATION_H