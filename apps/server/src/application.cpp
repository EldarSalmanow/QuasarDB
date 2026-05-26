#include <qdb/server/application.h>

#include <qdb/server/analyzer.h>
#include <qdb/server/lexer.h>
#include <qdb/server/parser.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

namespace qdb::server {

auto Ok(std::string message, nlohmann::json data = nlohmann::json::object()) -> qdb::core::Response {
    return qdb::core::Success(std::move(message), std::move(data));
}

auto Error(std::string message, nlohmann::json data = nlohmann::json::object()) -> qdb::core::Response {
    return qdb::core::Error(std::move(message), data.is_null() ? nlohmann::json::object() : std::move(data));
}

auto HasAggregate(const SelectStmt& select) -> bool {
    return std::any_of(select.SelectItems.begin(), select.SelectItems.end(), [](const SelectItem& item) {
        return item.Expr->KindOf() == Expression::Kind::Aggregate;
    });
}

auto IsLongRunning(const Statement& statement) -> bool {
    if (statement.KindOf() == Statement::Kind::Revert) return true;
    if (statement.KindOf() != Statement::Kind::Select) return false;
    return HasAggregate(static_cast<const SelectStmt&>(statement));
}

auto DatabaseName(const TableRef& table) -> std::string {
    return table.Database;
}

auto PermissionFor(const Statement& statement) -> Permission {
    switch (statement.KindOf()) {
        case Statement::Kind::Insert:
        case Statement::Kind::Update:
        case Statement::Kind::Delete:
        case Statement::Kind::Revert:
            return Permission::WRITE;
        case Statement::Kind::CreateDatabase:
        case Statement::Kind::CreateTable:
            return Permission::CREATE;
        case Statement::Kind::DropDatabase:
        case Statement::Kind::DropTable:
            return Permission::DELETE;
        default:
            return Permission::READ;
    }
}

auto HandlerId() -> std::string {
    std::ostringstream output;
    output << std::this_thread::get_id();
    return output.str();
}

auto PermissionFromString(const std::string& value) -> Permission {
    if (value == "READ") return Permission::READ;
    if (value == "WRITE") return Permission::WRITE;
    if (value == "CREATE") return Permission::CREATE;
    if (value == "DELETE") return Permission::DELETE;
    return Permission::INVALID;
}

auto SetupRequired() -> qdb::core::Response {
    return Error("Superuser account setup required", {{"setup_required", true}});
}

auto GrantAll(RBACManager& rbac, const std::string& username) -> void {
    for (const auto permission : {Permission::READ, Permission::WRITE, Permission::CREATE, Permission::DELETE}) {
        rbac.GrantPermission(username, "*", "*", permission);
    }
}

auto ApplyDatabase(TableRef& table, const Session& session) -> bool {
    if (!table.Database.empty()) {
        return true;
    }
    if (!session.database.has_value()) {
        return false;
    }
    table.Database = session.database.value();
    return true;
}

auto ApplyDatabase(Statement& statement, const Session& session) -> bool {
    switch (statement.KindOf()) {
        case Statement::Kind::CreateTable: return ApplyDatabase(static_cast<CreateTableStmt&>(statement).Table, session);
        case Statement::Kind::DropTable: return ApplyDatabase(static_cast<DropTableStmt&>(statement).Table, session);
        case Statement::Kind::Insert: return ApplyDatabase(static_cast<InsertStmt&>(statement).Table, session);
        case Statement::Kind::Update: return ApplyDatabase(static_cast<UpdateStmt&>(statement).Table, session);
        case Statement::Kind::Delete: return ApplyDatabase(static_cast<DeleteStmt&>(statement).Table, session);
        case Statement::Kind::Select: return ApplyDatabase(static_cast<SelectStmt&>(statement).Table, session);
        case Statement::Kind::Revert: return ApplyDatabase(static_cast<RevertStmt&>(statement).Table, session);
        default: return true;
    }
}

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
            Session session;
            while (client->IsConnected()) {
                auto request = client->ReceiveRequest();

                if (!request.has_value()) {
                    break;
                }

                client->SendResponse(Process(request.value(), session));
            }
        }).detach();
    }

    monitor_.Stop();

    return 0;
}

auto Application::Process(const qdb::core::Request& request) -> qdb::core::Response {
    Session session;
    return Process(request, session);
}

auto Application::Process(const qdb::core::Request& request, Session& session) -> qdb::core::Response {
    const auto started = std::chrono::steady_clock::now();
    const auto user = Authenticate(request).value_or("anonymous");
    qdb::core::Response response = Error("Unsupported action: " + request.Action());

    try {
        const std::unordered_map<std::string, std::function<qdb::core::Response()>> handlers = {
            {"handshake", [this] { return HandleHandshake(); }},
            {"login", [this, &request] { return HandleLogin(request); }},
            {"query", [this, &request, &session] { return HandleExecute(request, session); }},
            {"check_task", [this, &request] { return HandleCheckTask(request); }}
        };

        if (request.Action().empty()) {
            response = Error("Request must contain string field 'action'");
        } else if (config_.AuthRequired() && accounts_.Empty() && request.Action() != "login" &&
                   request.Action() != "handshake") {
            response = SetupRequired();
        } else if (auto handler = handlers.find(request.Action()); handler != handlers.end()) {
            response = handler->second();
        } else {
            response = Error("Unsupported action: " + request.Action());
        }
    } catch (const std::exception& exception) {
        response = Error(exception.what());
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

auto Application::HandleHandshake() const -> qdb::core::Response {
    return Ok("Handshake complete", {
        {"auth_required", config_.AuthRequired()},
        {"setup_required", config_.AuthRequired() && accounts_.Empty()}
    });
}

auto Application::HandleLogin(const qdb::core::Request& request) -> qdb::core::Response {
    const auto data = request.Data();
    const auto username = data.value("username", "");
    const auto password = data.value("password", "");
    const auto create = data.value("create", false);

    if (username.empty() || password.empty()) {
        return Error("Login requires username and password");
    }

    if (accounts_.Empty()) {
        if (!create) {
            return SetupRequired();
        }
        if (!accounts_.CreateAccount(username, password)) {
            return Error("Invalid superuser credentials");
        }
        GrantAll(rbac_, username);
        return Ok("Superuser account created", {{"token", jwt_.GenerateToken(username)}});
    }

    if (create) {
        return Error("Superuser setup is already complete");
    }

    if (!accounts_.Authenticate(username, password)) {
        return Error("Invalid username or password");
    }

    return Ok("Login successful", {{"token", jwt_.GenerateToken(username)}});
}

auto Application::HandleExecute(const qdb::core::Request& request, Session& session) -> qdb::core::Response {
    if (request.Query().empty()) {
        return Error("query requires data.query");
    }

    const auto user = Authenticate(request);
    if (config_.AuthRequired() && accounts_.Empty()) {
        return SetupRequired();
    }
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
    const auto effective_user = user.value_or("");

    if (auto security_response = HandleSecurityStatement(effective_user, *statement)) {
        return security_response.value();
    }

    if (!ApplyDatabase(*statement, session)) {
        return Error("No database selected; run USE <database> or qualify the table as <database>.<table>");
    }

    if (statement->KindOf() == Statement::Kind::UseDatabase) {
        const auto& use_db = static_cast<const UseDatabaseStmt&>(*statement);
        auto response = router_.Route(*statement);
        if (response.IsSuccess()) {
            session.database = use_db.DatabaseName;
        }
        return response;
    }

    if (config_.AuthRequired() && !CheckAccess(user.value(), *statement)) {
        return Error("Permission denied");
    }

    if (IsLongRunning(*statement)) {
        auto task_id = tasks_.Submit(std::move(statement));

        return qdb::core::Pending("Operation is running in background", {{"task_id", std::move(task_id)}});
    }

    return router_.Route(*statement);
}

auto Application::HandleCheckTask(const qdb::core::Request& request) -> qdb::core::Response {
    if (config_.AuthRequired() && accounts_.Empty()) {
        return SetupRequired();
    }

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

    return qdb::core::Pending(
        task->status == TaskStatus::Running ? "Operation is still running" : "Operation is pending",
        {{"task_id", request.TaskId().value()}, {"status", task->status == TaskStatus::Running ? "running" : "pending"}}
    );
}

auto Application::Authenticate(const qdb::core::Request& request) const -> std::optional<std::string> {
    if (request.Token().empty()) {
        return std::nullopt;
    }

    auto user = jwt_.ValidateToken(request.Token());
    if (!user.has_value() || !accounts_.HasAccount(user.value())) {
        return std::nullopt;
    }
    return user;
}

auto Application::CheckAccess(const std::string& user, const Statement& statement) const -> bool {
    const auto permission = PermissionFor(statement);
    if (statement.KindOf() == Statement::Kind::CreateDatabase) {
        return rbac_.CheckPermission(user, static_cast<const CreateDatabaseStmt&>(statement).DatabaseName, "*", permission);
    }
    if (statement.KindOf() == Statement::Kind::DropDatabase) {
        return rbac_.CheckPermission(user, static_cast<const DropDatabaseStmt&>(statement).DatabaseName, "*", permission);
    }

    auto table = Analyzer::TableFromStatement(statement);
    if (!table.has_value()) {
        return true;
    }
    return rbac_.CheckPermission(user, DatabaseName(table.value()), table->Table, permission);
}

auto Application::HandleSecurityStatement(const std::string& user, const Statement& statement)
    -> std::optional<qdb::core::Response> {
    if (statement.KindOf() == Statement::Kind::CreateUser) {
        const auto& create_user = static_cast<const CreateUserStmt&>(statement);
        if (config_.AuthRequired() && !rbac_.CheckPermission(user, "*", "*", Permission::CREATE)) {
            return Error("Permission denied");
        }
        if (!accounts_.CreateAccount(create_user.Username, create_user.Password)) {
            return Error("User already exists or invalid credentials");
        }
        return Ok("User created");
    }

    const auto apply = [&](const auto& stmt, bool grant) -> qdb::core::Response {
        if (config_.AuthRequired() && !rbac_.CheckPermission(user, stmt.Scope.Database, stmt.Scope.Table, Permission::CREATE)) {
            return Error("Permission denied");
        }
        if (!accounts_.HasAccount(stmt.Username)) {
            return Error("User not found: " + stmt.Username);
        }
        for (const auto& permission_name : stmt.Permissions) {
            const auto permission = PermissionFromString(permission_name);
            if (permission == Permission::INVALID) {
                return Error("Invalid permission: " + permission_name);
            }
            if (grant) {
                rbac_.GrantPermission(stmt.Username, stmt.Scope.Database, stmt.Scope.Table, permission);
            } else {
                rbac_.RevokePermission(stmt.Username, stmt.Scope.Database, stmt.Scope.Table, permission);
            }
        }
        return Ok(grant ? "Permission granted" : "Permission revoked");
    };

    if (statement.KindOf() == Statement::Kind::Grant) {
        return apply(static_cast<const GrantStmt&>(statement), true);
    }
    if (statement.KindOf() == Statement::Kind::Revoke) {
        return apply(static_cast<const RevokeStmt&>(statement), false);
    }
    return std::nullopt;
}

}  // namespace qdb::server
