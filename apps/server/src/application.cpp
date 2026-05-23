#include <qdb/server/application.h>

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

auto HasAggregate(const SelectStmt& select) -> bool {
    return std::any_of(select.SelectItems.begin(), select.SelectItems.end(), [](const SelectItem& item) {
        return dynamic_cast<const AggregateExpr*>(item.Expr.get()) != nullptr;
    });
}

auto IsLongRunning(const Statement& statement) -> bool {
    if (dynamic_cast<const RevertStmt*>(&statement) != nullptr) {
        return true;
    }

    const auto* select = dynamic_cast<const SelectStmt*>(&statement);
    return select != nullptr && HasAggregate(*select);
}

}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      server_(qdb::core::TcpServer::New(config_.Host(), config_.Port())),
      accounts_(config_.AccountPath()),
      jwt_(config_.JwtSecret()),
      registry_(Registry::New()),
      router_(registry_),
      monitor_(registry_),
      tasks_([this](const Statement& statement) { return router_.Route(statement); }) {}

auto Application::New(Config config) -> std::unique_ptr<Application> {
    return std::make_unique<Application>(std::move(config));
}

auto Application::Run() -> std::int32_t {
    if (!server_ || !server_->Start()) {
        return 1;
    }

    monitor_.Start();

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

    monitor_.Stop();

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

    const auto query = request.Query();

    Lexer lexer(query);

    auto tokens = lexer.Tokenize();

    if (std::any_of(tokens.begin(), tokens.end(), [](const Token& token) { return token.type == TokenType::Invalid; })) {
        return Error("Invalid token in SQL query");
    }

    Parser parser(std::move(tokens));

    auto statement = parser.ParseStatement();

    Analyzer::ValidateStatement(*statement);

    if (IsLongRunning(*statement)) {
        auto task_id = tasks_.Submit(std::move(statement));

        return qdb::core::ResponseBuilder::Pending()
            .Message("Operation is running in background")
            .Data({{"task_id", std::move(task_id)}})
            .Build();
    }

    return router_.Route(*statement);
}

auto Application::HandleCheckTask(const qdb::core::Request& request) -> qdb::core::Response {
    if (config_.AuthRequired() && !Authenticate(request).has_value()) {
        return Error("Valid token is required");
    }

    if (!request.TaskId().has_value()) {
        return Error("check_task requires data.task_id");
    }

    auto task = tasks_.Get(request.TaskId().value());

    if (!task.has_value()) {
        return Error("Task not found", {{"task_id", request.TaskId().value()}});
    }

    if (task->result.has_value()) {
        return task->result.value();
    }

    return qdb::core::ResponseBuilder::Pending()
        .Message(task->status == TaskStatus::Running ? "Operation is still running" : "Operation is pending")
        .Data({{"task_id", request.TaskId().value()}, {"status", task->status == TaskStatus::Running ? "running" : "pending"}})
        .Build();
}

auto Application::Authenticate(const qdb::core::Request& request) const -> std::optional<std::string> {
    if (request.Token().empty()) {
        return std::nullopt;
    }

    return jwt_.ValidateToken(request.Token());
}

}  // namespace qdb::server
