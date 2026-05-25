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
    return table.Database;
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
    if (auto* create = dynamic_cast<CreateTableStmt*>(&statement)) return ApplyDatabase(create->Table, session);
    if (auto* drop = dynamic_cast<DropTableStmt*>(&statement)) return ApplyDatabase(drop->Table, session);
    if (auto* insert = dynamic_cast<InsertStmt*>(&statement)) return ApplyDatabase(insert->Table, session);
    if (auto* update = dynamic_cast<UpdateStmt*>(&statement)) return ApplyDatabase(update->Table, session);
    if (auto* del = dynamic_cast<DeleteStmt*>(&statement)) return ApplyDatabase(del->Table, session);
    if (auto* select = dynamic_cast<SelectStmt*>(&statement)) return ApplyDatabase(select->Table, session);
    if (auto* revert = dynamic_cast<RevertStmt*>(&statement)) return ApplyDatabase(revert->Table, session);
    return true;
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

    if (request.Action().empty()) {
        response = Error("Request must contain string field 'action'");
    } else {
        try {
            if (config_.AuthRequired() && accounts_.Empty() && request.Action() != "login") {
                response = SetupRequired();
            } else if (request.Action() == "login") {
                response = HandleLogin(request);
            } else if (request.Action() == "query") {
                response = HandleExecute(request, session);
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

    if (const auto* use_db = dynamic_cast<const UseDatabaseStmt*>(statement.get())) {
        auto response = router_.Route(*statement);
        if (response.IsSuccess()) {
            session.database = use_db->DatabaseName;
        }
        return response;
    }

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

    return qdb::core::ResponseBuilder::Pending()
        .Message(task->status == TaskStatus::Running ? "Operation is still running" : "Operation is pending")
        .Data({{"task_id", request.TaskId().value()}, {"status", task->status == TaskStatus::Running ? "running" : "pending"}})
        .Build();
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

auto Application::HandleSecurityStatement(const std::string& user, const Statement& statement)
    -> std::optional<qdb::core::Response> {
    if (const auto* create_user = dynamic_cast<const CreateUserStmt*>(&statement)) {
        if (config_.AuthRequired() && !rbac_.CheckPermission(user, "*", "*", Permission::CREATE)) {
            return Error("Permission denied");
        }
        if (!accounts_.CreateAccount(create_user->Username, create_user->Password)) {
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

    if (const auto* grant = dynamic_cast<const GrantStmt*>(&statement)) {
        return apply(*grant, true);
    }
    if (const auto* revoke = dynamic_cast<const RevokeStmt*>(&statement)) {
        return apply(*revoke, false);
    }
    return std::nullopt;
}

}  // namespace qdb::server
