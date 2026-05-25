#ifndef QUASARDB_APPLICATION_H
#define QUASARDB_APPLICATION_H

#include <qdb/client/config.h>
#include <qdb/client/renderer.h>

#include <qdb/core/request.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_client.h>

#include <memory>
#include <optional>
#include <string>


namespace qdb::client {

class Application {
public:
    explicit Application(Config config);

public:
    static auto New(Config config) -> std::unique_ptr<Application>;

public:
    auto Run() -> std::int32_t;

private:
    auto HandleAsyncResponse(const qdb::core::Response& response) -> void;

    auto PollTask(const std::string& task_id) -> std::optional<qdb::core::Response>;

    auto BuildRequest(const std::string& command) -> qdb::core::Request;

    auto HandleLoginResponse(const qdb::core::Response& response) -> void;

    auto CreateSuperuserInteractively() -> bool;

private:
    Config config_;

    std::unique_ptr<qdb::core::TcpClient> client_;

    Renderer renderer_;

    std::string token_;
};

}  // namespace qdb::client

#endif  // QUASARDB_APPLICATION_H
