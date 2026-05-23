#include <qdb/server/application.h>

#include <qdb/server/analyzer.h>
#include <qdb/server/lexer.h>
#include <qdb/server/parser.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <optional>
#include <sstream>
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

auto DatabaseName(const TableRef& table) -> std::string {
    return table.Database.empty() ? "default" : table.Database;
}

auto PermissionFor(const Statement& statement) -> Permission {
    if (dynamic_cast<const SelectStmt*>(&statement) != nullptr) return Permission::READ;
    if (dynamic_cast<const InsertStmt*>(&statement) != nullptr) return Permission::WRITE;
    if (dynamic_cast<const UpdateStmt*>(&statement) != nullptr) return Permission::WRITE;
    if (dynamic_cast<const DeleteStmt*>(&statement) != nullptr) return Permission::WRITE;
    if (dynamic_cast<const RevertStmt*>(&statement) != nullptr) return Permission::WRITE;
    if (dynamic_cast<const CreateDatabaseStmt*>(&statement) != nullptr) return Permission::CREATE;
    if (dynamic_cast<const CreateTableStmt*>(&statement) != nullptr) return Permission::CREATE;
    if (dynamic_cast<const DropDatabaseStmt*>(&statement) != nullptr) return Permission::DELETE;
    if (dynamic_cast<const DropTableStmt*>(&statement) != nullptr) return Permission::DELETE;
    return Permission::READ;
}

auto HandlerId() -> std::string {
    std::ostringstream output;
    output << std::this_thread::get_id();
    return output.str();
}

}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      server_(qdb::core::TcpServer::New(config_.Host(), config_.Port())),
      accounts_(config_.AccountPath()),
      jwt_(config_.JwtSecret()),
      rbac_(config_.RbacPath()),
      logger_((std::filesystem::path(config_.RbacPath()).parent_path() / "logs").string()),
      catalog_(std::filesystem::path(config_.RbacPath()).parent_path() / "catalog.json"),
      registry_(Registry::New(config_.AutoStartStorage(), config_.StorageBinary(), config_.StorageRoot())),
      router_(registry_, catalog_),
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
    const auto started = std::chrono::steady_clock::now();
    const auto user = Authenticate(request).value_or("anonymous");
    qdb::core::Response response = Error("Unsupported action: " + request.Action());

    if (request.Action().empty()) {
        response = Error("Request must contain string field 'action'");
    } else {
        try {
            if (request.Action() == "login") {
                response = HandleLogin(request);
            } else if (request.Action() == "query") {
                response = HandleExecute(request);
            } else if (request.Action() == "check_task") {
                response = HandleCheckTask(request);
            } else {
                response = Error("Unsupported action: " + request.Action());
            }
        } catch (const std::exception& exception) {
            response = Error(exception.what());
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
    telemetry_.RecordRequest(static_cast<std::uint64_t>(elapsed), response.IsSuccess() || response.IsPending());
    const auto status = response.IsSuccess() ? "success" : (response.IsPending() ? "pending" : "error");
    logger_.LogQuery(user, HandlerId(), request.Query().empty() ? request.Action() : request.Query(),
                     static_cast<std::uint64_t>(elapsed), status);
    return response;
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

    const auto user = Authenticate(request);
    if (config_.AuthRequired() && !user.has_value()) {
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
    if (config_.AuthRequired() && !CheckAccess(user.value(), *statement)) {
        return Error("Permission denied");
    }

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

    auto task = tasks_.GetStatus(request.TaskId().value());

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

auto Application::CheckAccess(const std::string& user, const Statement& statement) const -> bool {
    const auto permission = PermissionFor(statement);
    if (const auto* create_db = dynamic_cast<const CreateDatabaseStmt*>(&statement)) {
        return rbac_.CheckPermission(user, create_db->DatabaseName, "*", permission);
    }
    if (const auto* drop_db = dynamic_cast<const DropDatabaseStmt*>(&statement)) {
        return rbac_.CheckPermission(user, drop_db->DatabaseName, "*", permission);
    }

    auto table = Analyzer::TableFromStatement(statement);
    if (!table.has_value()) {
        return true;
    }
    return rbac_.CheckPermission(user, DatabaseName(table.value()), table->Table, permission);
}

}  // namespace qdb::server
