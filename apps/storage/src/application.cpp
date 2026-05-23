#include <qdb/storage/application.h>

#include <iostream>
#include <qdb/core/response.h>
#include <qdb/storage/executor.h>
#include <qdb/server/ast.h>

namespace qdb::storage {

Application::Application(Config config)
    : config_(std::move(config)),
      server_(qdb::core::TcpServer::New(config_.Host(), config_.Port())),
      db_manager_(config_.Root()) {}

auto Application::New(Config config) -> std::unique_ptr<Application> {
    return std::make_unique<Application>(std::move(config));
}

auto Application::Run() -> std::int32_t {
    if (!server_ || !server_->Start()) {
        std::cerr << "Failed to start storage on " << config_.Host() << ":" << config_.Port() << std::endl;
        return 1;
    }

    std::cout << "Storage listening on " << config_.Host() << ":" << config_.Port() << std::endl;

    while (server_->IsRunning()) {
        auto client = AcceptConnection();
        if (!client) {
            continue;
        }

        connection_ = std::move(client);
        while (true) {
            auto request = connection_->Receive();
            if (!request.has_value()) {
                break;
            }

            auto response = ProcessRequest(request.value());
            if (!response.has_value()) {
                break;
            }

            if (!connection_->Send(response.value())) {
                break;
            }
        }
    }

    return 0;
}

auto Application::AcceptConnection() -> std::unique_ptr<qdb::core::TcpClient> {
    if (!server_) {
        return nullptr;
    }

    return server_->Accept();
}

auto Application::ProcessRequest(const nlohmann::json& request) -> std::optional<nlohmann::json> {
    if (!request.is_object()) {
        return qdb::core::ResponseBuilder::Error()
            .Message("Request must be a JSON object")
            .Build()
            .ToJsonObject();
    }

    const std::string action = request.value("action", "");
    if (action.empty()) {
        return qdb::core::ResponseBuilder::Error().Message("Missing action").Build().ToJsonObject();
    }

    if (action == "execute_ast") {
        if (!request.contains("data") || !request["data"].contains("ast_root")) {
            return qdb::core::ResponseBuilder::Error().Message("Missing ast_root").Build().ToJsonObject();
        }

        return ExecuteAst(request["data"]["ast_root"]);
    }

    if (action == "ping") {
        return qdb::core::ResponseBuilder::Success().Message("pong").Build().ToJsonObject();
    }

    return qdb::core::ResponseBuilder::Error().Message("Unknown action").Build().ToJsonObject();
}

auto Application::ExecuteAst(const nlohmann::json& ast) -> std::optional<nlohmann::json> {
    try {
        auto stmt = qdb::server::DeserializeAst(ast);
        if (!stmt) {
            return qdb::core::ResponseBuilder::Error()
                .Message("Invalid or unsupported AST")
                .Build()
                .ToJsonObject();
        }

        Executor exec(db_manager_);
        stmt->Accept(exec);
        return exec.Result();
    } catch (const std::exception& ex) {
        return qdb::core::ResponseBuilder::Error().Message(ex.what()).Build().ToJsonObject();
    }
}

}  // namespace qdb::storage
