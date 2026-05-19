#include <qdb/server/application.h>

#include <qdb/server/ast.h>
#include <qdb/server/analyzer.h>
#include <qdb/server/lexer.h>
#include <qdb/server/parser.h>

#include <algorithm>
#include <exception>
#include <optional>
#include <thread>
#include <utility>

namespace qdb::server {

namespace {

auto Ok(std::string message, nlohmann::json data = nlohmann::json::object()) -> qdb::core::Response {
    return qdb::core::ResponseBuilder::Success()
        .Message(std::move(message))
        .Data(std::move(data))
        .Build();
}

auto Error(std::string message, nlohmann::json data = nlohmann::json::object()) -> qdb::core::Response {
    return qdb::core::ResponseBuilder::Error()
        .Message(std::move(message))
        .Data(data.is_null() ? nlohmann::json::object() : std::move(data))
        .Build();
}

}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      server_(qdb::core::TcpServer::New(config_.Host(), config_.Port())),
      accounts_(config_.AccountPath()),
      jwt_(config_.JwtSecret()) {}

auto Application::New(Config config) -> std::unique_ptr<Application> {
    return std::make_unique<Application>(std::move(config));
}

auto Application::Run() -> std::int32_t {
    if (!server_ || !server_->Start()) {
        return 1;
    }

    while (server_->IsRunning()) {
        auto client = server_->Accept();
        if (!client) {
            continue;
        }

        std::thread([this, client = std::move(client)]() mutable {
            while (client->IsConnected()) {
                auto request = client->ReceiveRequest();

                if (!request.has_value()) {
                    break;
                }

                client->SendResponse(Process(request.value()));
            }
        }).detach();
    }

    return 0;
}

auto Application::Process(const qdb::core::Request& request) -> qdb::core::Response {
    if (request.Action().empty()) {
        return Error("Request must contain string field 'action'");
    }

    try {
        if (request.Action() == "login") {
            return HandleLogin(request);
        }

        if (request.Action() == "query") {
            return HandleExecute(request);
        }

        if (request.Action() == "check_task") {
            return HandleCheckTask(request);
        }

        return Error("Unsupported action: " + request.Action());
    } catch (const std::exception& exception) {
        return Error(exception.what());
    }
}

auto Application::HandleLogin(const qdb::core::Request& request) -> qdb::core::Response {
    const auto data = request.Data();
    const auto username = data.value("username", "");
    const auto password = data.value("password", "");

    if (username.empty() || password.empty()) {
        return Error("Login requires username and password");
    }

    if (!accounts_.Authenticate(username, password)) {
        return Error("Invalid username or password");
    }

    return Ok("Login successful", {{"token", jwt_.GenerateToken(username)}});
}

auto Application::HandleExecute(const qdb::core::Request& request) -> qdb::core::Response {
    if (request.Query().empty()) {
        return Error("query requires data.query");
    }

    if (config_.AuthRequired() && !Authenticate(request).has_value()) {
        return Error("Valid token is required");
    }

    return RouteSql(request.Query());
}

auto Application::HandleCheckTask(const qdb::core::Request& request) -> qdb::core::Response {
    if (config_.AuthRequired() && !Authenticate(request).has_value()) {
        return Error("Valid token is required");
    }

    if (!request.TaskId().has_value()) {
        return Error("check_task requires data.task_id");
    }

    return Error("Task not found", {{"task_id", request.TaskId().value()}});
}

auto Application::Authenticate(const qdb::core::Request& request) const -> std::optional<std::string> {
    if (request.Token().empty()) {
        return std::nullopt;
    }

    return jwt_.ValidateToken(request.Token());
}

auto Application::RouteSql(const std::string& sql) -> qdb::core::Response {
    Lexer lexer(sql);
    auto tokens = lexer.Tokenize();

    if (std::any_of(tokens.begin(), tokens.end(), [](const Token& token) { return token.type == TokenType::Invalid; })) {
        return Error("Invalid token in SQL query");
    }

    Parser parser(std::move(tokens));
    auto statement = parser.ParseStatement();

    if (const auto* create = dynamic_cast<const CreateTableStmt*>(statement.get())) {
        auto node = router_.CreateNode(create->Table);
        return Ok("Storage node created", {
            {"storage_node", node.address},
            {"table", node.table},
            {"internal_request", {
                {"action", "execute_ast"},
                {"data", {
                    {"database", create->Table.Database},
                    {"ast_root", SerializeAst(*statement)}
                }}
            }},
            {"rows", nlohmann::json::array()},
            {"rows_affected", 0}
        });
    }

    if (const auto* drop = dynamic_cast<const DropTableStmt*>(statement.get())) {
        const auto removed = router_.DropNode(drop->Table);
        return Ok(removed ? "Storage node removed" : "Table had no storage node",
                 {{"removed", removed}, {"internal_request", {
                      {"action", "execute_ast"},
                      {"data", {
                          {"database", drop->Table.Database},
                          {"ast_root", SerializeAst(*statement)}
                      }}
                  }}});
    }

    auto table = Analyzer::TableFromStatement(*statement);
    if (!table.has_value()) {
        return Ok("Query parsed", {{"executed", false}});
    }

    auto node = router_.Resolve(table.value());
    if (!node.has_value()) {
        return Error("No storage node for table");
    }

    return Ok("Query routed", {
        {"storage_node", node->address},
        {"table", node->table},
        {"internal_request", {
            {"action", "execute_ast"},
            {"data", {
                {"database", table->Database},
                {"ast_root", SerializeAst(*statement)}
            }}
        }},
        {"rows", nlohmann::json::array()},
        {"rows_affected", 0}
    });
}

}  // namespace qdb::server
