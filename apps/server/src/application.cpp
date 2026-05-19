#include <qdb/server/application.h>

#include <qdb/server/ast.h>
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

auto LiteralTypeName(Literal::Type type) -> std::string {
    switch (type) {
        case Literal::Type::String:
            return "string";
        case Literal::Type::Integer:
            return "int";
        case Literal::Type::Null:
            return "null";
    }
    return "unknown";
}

auto LiteralValue(const Literal& literal) -> nlohmann::json {
    switch (literal.LiteralType) {
        case Literal::Type::Integer:
            return std::stoi(literal.Value);
        case Literal::Type::Null:
            return nullptr;
        case Literal::Type::String:
            return literal.Value;
    }
    return literal.Value;
}

auto ToJson(const Literal& literal) -> nlohmann::json {
    return {{"node_type", "Literal"}, {"data_type", LiteralTypeName(literal.LiteralType)}, {"value", LiteralValue(literal)}};
}

auto AggregateName(AggregateExpr::Function function) -> std::string {
    switch (function) {
        case AggregateExpr::Function::Sum:
            return "SUM";
        case AggregateExpr::Function::Count:
            return "COUNT";
        case AggregateExpr::Function::Avg:
            return "AVG";
    }
    return "UNKNOWN";
}

auto ToJson(const Expression& expression) -> nlohmann::json {
    if (const auto* identifier = dynamic_cast<const IdentifierExpr*>(&expression)) {
        return {{"node_type", "Identifier"}, {"value", identifier->Name}};
    }
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        return ToJson(*literal->LiteralValue);
    }
    if (const auto* aggregate = dynamic_cast<const AggregateExpr*>(&expression)) {
        return {{"node_type", "AggregateFunction"},
                {"function", AggregateName(aggregate->FunctionType)},
                {"argument", {{"node_type", "Identifier"}, {"value", aggregate->Column}}}};
    }
    return {{"node_type", "Expression"}};
}

auto ComparisonOperatorName(ComparisonCondition::Operator op) -> std::string {
    switch (op) {
        case ComparisonCondition::Operator::Equal:
            return "=";
        case ComparisonCondition::Operator::NotEqual:
            return "!=";
        case ComparisonCondition::Operator::Less:
            return "<";
        case ComparisonCondition::Operator::Greater:
            return ">";
        case ComparisonCondition::Operator::LessEqual:
            return "<=";
        case ComparisonCondition::Operator::GreaterEqual:
            return ">=";
    }
    return "?";
}

auto ToJson(const Condition& condition) -> nlohmann::json {
    if (const auto* comparison = dynamic_cast<const ComparisonCondition*>(&condition)) {
        return {{"node_type", "BinaryExpression"},
                {"operator", ComparisonOperatorName(comparison->Op)},
                {"left", ToJson(*comparison->Left)},
                {"right", ToJson(*comparison->Right)}};
    }
    if (const auto* like = dynamic_cast<const LikeCondition*>(&condition)) {
        return {{"node_type", "BinaryExpression"},
                {"operator", "LIKE"},
                {"left", ToJson(*like->Value)},
                {"right", ToJson(Literal(Literal::Type::String, like->Pattern))}};
    }
    if (const auto* between = dynamic_cast<const BetweenCondition*>(&condition)) {
        return {{"node_type", "BetweenExpression"},
                {"value", ToJson(*between->Value)},
                {"lower", ToJson(*between->Lower)},
                {"upper", ToJson(*between->Upper)}};
    }
    if (const auto* and_condition = dynamic_cast<const AndCondition*>(&condition)) {
        return {{"node_type", "BinaryExpression"},
                {"operator", "AND"},
                {"left", ToJson(*and_condition->Left)},
                {"right", ToJson(*and_condition->Right)}};
    }
    if (const auto* or_condition = dynamic_cast<const OrCondition*>(&condition)) {
        return {{"node_type", "BinaryExpression"},
                {"operator", "OR"},
                {"left", ToJson(*or_condition->Left)},
                {"right", ToJson(*or_condition->Right)}};
    }
    return {{"node_type", "Condition"}};
}

auto ColumnTypeName(ColumnDef::Type type) -> std::string {
    return type == ColumnDef::Type::Int ? "int" : "string";
}

auto ToJson(const ColumnDef& column) -> nlohmann::json {
    nlohmann::json modifiers = nlohmann::json::array();
    if (column.Indexed) {
        modifiers.push_back("INDEXED");
    }
    if (column.NotNull) {
        modifiers.push_back("NOT_NULL");
    }

    nlohmann::json json = {{"name", column.Name}, {"data_type", ColumnTypeName(column.ColumnType)}, {"modifiers", modifiers}};
    if (column.DefaultValue) {
        json["default_value"] = ToJson(*column.DefaultValue);
    }
    return json;
}

auto ToJson(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

auto ToJson(const Statement& statement) -> nlohmann::json {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) {
        nlohmann::json columns = nlohmann::json::array();
        for (const auto& column : create->Columns) {
            columns.push_back(ToJson(column));
        }
        return {{"node_type", "CreateTableStatement"}, {"table", ToJson(create->Table)}, {"columns", columns}};
    }
    if (const auto* select = dynamic_cast<const SelectStmt*>(&statement)) {
        nlohmann::json projections = nlohmann::json::array();
        if (select->SelectAll) {
            projections.push_back({{"node_type", "Wildcard"}});
        } else {
            for (const auto& item : select->SelectItems) {
                auto projection = ToJson(*item.Expr);
                if (!item.Alias.empty()) {
                    projection["alias"] = item.Alias;
                }
                projections.push_back(std::move(projection));
            }
        }
        nlohmann::json json = {{"node_type", "SelectStatement"}, {"table", ToJson(select->Table)}, {"projections", projections}};
        if (select->WhereClause) {
            json["where_clause"] = ToJson(*select->WhereClause);
        }
        return json;
    }
    if (const auto* revert = dynamic_cast<const RevertStmt*>(&statement)) {
        return {{"node_type", "RevertStatement"}, {"table", ToJson(revert->Table)}, {"target_timestamp", revert->Timestamp}};
    }
    if (const auto* insert = dynamic_cast<const InsertStmt*>(&statement)) {
        return {{"node_type", "InsertStatement"}, {"table", ToJson(insert->Table)}, {"columns", insert->Columns}};
    }
    if (const auto* update = dynamic_cast<const UpdateStmt*>(&statement)) {
        nlohmann::json json = {{"node_type", "UpdateStatement"}, {"table", ToJson(update->Table)}};
        if (update->WhereClause) {
            json["where_clause"] = ToJson(*update->WhereClause);
        }
        return json;
    }
    if (const auto* del = dynamic_cast<const DeleteStmt*>(&statement)) {
        nlohmann::json json = {{"node_type", "DeleteStatement"}, {"table", ToJson(del->Table)}};
        if (del->WhereClause) {
            json["where_clause"] = ToJson(*del->WhereClause);
        }
        return json;
    }
    if (const auto* drop = dynamic_cast<const DropTableStmt*>(&statement)) {
        return {{"node_type", "DropTableStatement"}, {"table", ToJson(drop->Table)}};
    }
    return {{"node_type", "Statement"}};
}

auto TableFromStatement(const Statement& statement) -> std::optional<TableRef> {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) return create->Table;
    if (const auto* drop = dynamic_cast<const DropTableStmt*>(&statement)) return drop->Table;
    if (const auto* insert = dynamic_cast<const InsertStmt*>(&statement)) return insert->Table;
    if (const auto* update = dynamic_cast<const UpdateStmt*>(&statement)) return update->Table;
    if (const auto* del = dynamic_cast<const DeleteStmt*>(&statement)) return del->Table;
    if (const auto* select = dynamic_cast<const SelectStmt*>(&statement)) return select->Table;
    if (const auto* revert = dynamic_cast<const RevertStmt*>(&statement)) return revert->Table;

    return std::nullopt;
}

}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      server_(qdb::core::TcpServer::New(config_.Host(), config_.Port())),
      accounts_(config_.AccountPath()),
      jwt_(config_.JwtSecret()),
      router_(config_.StorageNodes()) {}

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
                auto request = client->Receive();
                if (!request.has_value()) {
                    break;
                }

                client->SendResponse(ProcessJson(request.value()));
            }
        }).detach();
    }

    return 0;
}

auto Application::ProcessJson(const nlohmann::json& json) -> qdb::core::Response {
    if (!json.contains("action") || !json["action"].is_string()) {
        return Error("Request must contain string field 'action'");
    }

    const auto action = json["action"].get<std::string>();
    if (action == "login") {
        return HandleLogin(json);
    }

    return Process(qdb::core::Request::FromJsonObject(json));
}

auto Application::Process(const qdb::core::Request& request) -> qdb::core::Response {
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

        if (request.Action() == "telemetry") {
            return HandleTelemetry(request);
        }

        return Error("Unsupported action: " + request.Action());
    } catch (const std::exception& exception) {
        return Error(exception.what());
    }
}

auto Application::HandleLogin(const nlohmann::json& json) -> qdb::core::Response {
    const auto data = json.value("data", nlohmann::json::object());
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

auto Application::HandleLogin(const qdb::core::Request& request) -> qdb::core::Response {
    return HandleLogin(request.ToJsonObject());
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

auto Application::HandleTelemetry(const qdb::core::Request& request) -> qdb::core::Response {
    const auto user = Authenticate(request);
    if (config_.AuthRequired() && (!user.has_value() || user.value() != "admin")) {
        return Error("Admin token is required");
    }
    return Ok("Telemetry snapshot", {
        {"current_rps", 0},
        {"avg_rps_10m", 0},
        {"max_rps_10m", 0},
        {"avg_response_time_10s_ms", 0.0},
        {"error_rate_1m", 0.0},
    });
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
                    {"ast_root", ToJson(*statement)}
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
                          {"ast_root", ToJson(*statement)}
                      }}
                  }}});
    }

    auto table = TableFromStatement(*statement);
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
                {"ast_root", ToJson(*statement)}
            }}
        }},
        {"rows", nlohmann::json::array()},
        {"rows_affected", 0}
    });
}

}  // namespace qdb::server
