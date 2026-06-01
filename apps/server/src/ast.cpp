#include <qdb/server/ast.h>

#include <utility>

namespace qdb::server {

ASTNode::~ASTNode() = default;

Expression::~Expression() = default;

Condition::~Condition() = default;

Statement::~Statement() = default;

Literal::Literal(Type type, std::string value) : LiteralType(type), Value(std::move(value)) {}

IdentifierExpr::IdentifierExpr(std::string name) : Name(std::move(name)) {}

LiteralExpr::LiteralExpr(std::unique_ptr<Literal> literal) : LiteralValue(std::move(literal)) {}

AggregateExpr::AggregateExpr(Function function, std::string columns)
    : FunctionType(function), Column(std::move(columns)) {}

ComparisonCondition::
    ComparisonCondition(std::unique_ptr<Expression> left, Operator op, std::unique_ptr<Expression> right)
    : Left(std::move(left)), Op(op), Right(std::move(right)) {}

BetweenCondition::BetweenCondition(
    std::unique_ptr<Expression> value,
    std::unique_ptr<Expression> lower,
    std::unique_ptr<Expression> upper
)
    : Value(std::move(value)), Lower(std::move(lower)), Upper(std::move(upper)) {}

LikeCondition::LikeCondition(std::unique_ptr<Expression> value, std::string pattern)
    : Value(std::move(value)), Pattern(std::move(pattern)) {}

AndCondition::AndCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right)
    : Left(std::move(left)), Right(std::move(right)) {}

OrCondition::OrCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right)
    : Left(std::move(left)), Right(std::move(right)) {}

TableRef::TableRef(std::string table) : Table(std::move(table)) {}

TableRef::TableRef(std::string database, std::string table) : Database(std::move(database)), Table(std::move(table)) {}

ColumnDef::ColumnDef(std::string name, Type type) : Name(std::move(name)), ColumnType(type) {}

SelectItem::SelectItem(std::unique_ptr<Expression> expr, std::string alias)
    : Expr(std::move(expr)), Alias(std::move(alias)) {}

CreateDatabaseStmt::CreateDatabaseStmt(std::string name) : DatabaseName(std::move(name)) {}

DropDatabaseStmt::DropDatabaseStmt(std::string name) : DatabaseName(std::move(name)) {}

UseDatabaseStmt::UseDatabaseStmt(std::string name) : DatabaseName(std::move(name)) {}

CreateUserStmt::CreateUserStmt(std::string username, std::string password)
    : Username(std::move(username)), Password(std::move(password)) {}

GrantStmt::GrantStmt(std::vector<std::string> permissions, TableRef scope, std::string username)
    : Permissions(std::move(permissions)), Scope(std::move(scope)), Username(std::move(username)) {}

RevokeStmt::RevokeStmt(std::vector<std::string> permissions, TableRef scope, std::string username)
    : Permissions(std::move(permissions)), Scope(std::move(scope)), Username(std::move(username)) {}

RevertStmt::RevertStmt(TableRef table, std::string timestamp)
    : Table(std::move(table)), Timestamp(std::move(timestamp)) {}

CreateTableStmt::CreateTableStmt(TableRef table, std::vector<ColumnDef> columns)
    : Table(std::move(table)), Columns(std::move(columns)) {}

DropTableStmt::DropTableStmt(TableRef table) : Table(std::move(table)) {}

InsertStmt::InsertStmt(
    TableRef table,
    std::vector<std::string> columns,
    std::vector<std::vector<std::unique_ptr<Literal>>> values
)
    : Table(std::move(table)), Columns(std::move(columns)), Values(std::move(values)) {}

UpdateStmt::UpdateStmt(
    TableRef table,
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assigns,
    std::unique_ptr<Condition> where
)
    : Table(std::move(table)), Assignments(std::move(assigns)), WhereClause(std::move(where)) {}

DeleteStmt::DeleteStmt(TableRef table, std::unique_ptr<Condition> where)
    : Table(std::move(table)), WhereClause(std::move(where)) {}

SelectStmt::SelectStmt(bool all, std::vector<SelectItem> items, TableRef table, std::unique_ptr<Condition> where)
    : SelectAll(all), SelectItems(std::move(items)), Table(std::move(table)), WhereClause(std::move(where)) {}

auto IdentifierExpr::KindOf() const -> Kind { return Kind::Identifier; }

auto LiteralExpr::KindOf() const -> Kind { return Kind::Literal; }

auto AggregateExpr::KindOf() const -> Kind { return Kind::Aggregate; }

auto ComparisonCondition::KindOf() const -> Kind { return Kind::Comparison; }

auto BetweenCondition::KindOf() const -> Kind { return Kind::Between; }

auto LikeCondition::KindOf() const -> Kind { return Kind::Like; }

auto AndCondition::KindOf() const -> Kind { return Kind::And; }

auto OrCondition::KindOf() const -> Kind { return Kind::Or; }

auto CreateDatabaseStmt::KindOf() const -> Kind { return Kind::CreateDatabase; }

auto DropDatabaseStmt::KindOf() const -> Kind { return Kind::DropDatabase; }

auto UseDatabaseStmt::KindOf() const -> Kind { return Kind::UseDatabase; }

auto CreateUserStmt::KindOf() const -> Kind { return Kind::CreateUser; }

auto GrantStmt::KindOf() const -> Kind { return Kind::Grant; }

auto RevokeStmt::KindOf() const -> Kind { return Kind::Revoke; }

auto RevertStmt::KindOf() const -> Kind { return Kind::Revert; }

auto CreateTableStmt::KindOf() const -> Kind { return Kind::CreateTable; }

auto DropTableStmt::KindOf() const -> Kind { return Kind::DropTable; }

auto InsertStmt::KindOf() const -> Kind { return Kind::Insert; }

auto UpdateStmt::KindOf() const -> Kind { return Kind::Update; }

auto DeleteStmt::KindOf() const -> Kind { return Kind::Delete; }

auto SelectStmt::KindOf() const -> Kind { return Kind::Select; }

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

auto LiteralTypeFromString(const std::string& value) -> Literal::Type {
    if (value == "int") return Literal::Type::Integer;
    if (value == "string") return Literal::Type::String;
    return Literal::Type::Null;
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
    return {
        {"node_type", "Literal"},
        {"data_type", LiteralTypeName(literal.LiteralType)},
        {"value", LiteralValue(literal)}
    };
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

auto AggregateFromName(const std::string& name) -> AggregateExpr::Function {
    if (name == "SUM") return AggregateExpr::Function::Sum;
    if (name == "COUNT") return AggregateExpr::Function::Count;
    return AggregateExpr::Function::Avg;
}

auto ToJson(const Expression& expression) -> nlohmann::json {
    switch (expression.KindOf()) {
        case Expression::Kind::Identifier: {
            const auto* identifier = static_cast<const IdentifierExpr*>(&expression);
            return {{"node_type", "Identifier"}, {"value", identifier->Name}};
        }
        case Expression::Kind::Literal: {
            const auto* literal = static_cast<const LiteralExpr*>(&expression);
            return ToJson(*literal->LiteralValue);
        }
        case Expression::Kind::Aggregate: {
            const auto* aggregate = static_cast<const AggregateExpr*>(&expression);
            return {
                {"node_type", "AggregateFunction"},
                {"function", AggregateName(aggregate->FunctionType)},
                {"argument", {{"node_type", "Identifier"}, {"value", aggregate->Column}}}
            };
        }
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

auto ComparisonOperatorFromName(const std::string& op) -> ComparisonCondition::Operator {
    if (op == "=" || op == "==") return ComparisonCondition::Operator::Equal;
    if (op == "!=") return ComparisonCondition::Operator::NotEqual;
    if (op == "<") return ComparisonCondition::Operator::Less;
    if (op == ">") return ComparisonCondition::Operator::Greater;
    if (op == "<=") return ComparisonCondition::Operator::LessEqual;
    return ComparisonCondition::Operator::GreaterEqual;
}

auto ToJson(const Condition& condition) -> nlohmann::json {
    switch (condition.KindOf()) {
        case Condition::Kind::Comparison: {
            const auto* comparison = static_cast<const ComparisonCondition*>(&condition);
            return {
                {"node_type", "BinaryExpression"},
                {"operator", ComparisonOperatorName(comparison->Op)},
                {"left", ToJson(*comparison->Left)},
                {"right", ToJson(*comparison->Right)}
            };
        }
        case Condition::Kind::Like: {
            const auto* like = static_cast<const LikeCondition*>(&condition);
            return {
                {"node_type", "BinaryExpression"},
                {"operator", "LIKE"},
                {"left", ToJson(*like->Value)},
                {"right", ToJson(Literal(Literal::Type::String, like->Pattern))}
            };
        }
        case Condition::Kind::Between: {
            const auto* between = static_cast<const BetweenCondition*>(&condition);
            return {
                {"node_type", "BetweenExpression"},
                {"value", ToJson(*between->Value)},
                {"lower", ToJson(*between->Lower)},
                {"upper", ToJson(*between->Upper)}
            };
        }
        case Condition::Kind::And: {
            const auto* and_condition = static_cast<const AndCondition*>(&condition);
            return {
                {"node_type", "BinaryExpression"},
                {"operator", "AND"},
                {"left", ToJson(*and_condition->Left)},
                {"right", ToJson(*and_condition->Right)}
            };
        }
        case Condition::Kind::Or: {
            const auto* or_condition = static_cast<const OrCondition*>(&condition);
            return {
                {"node_type", "BinaryExpression"},
                {"operator", "OR"},
                {"left", ToJson(*or_condition->Left)},
                {"right", ToJson(*or_condition->Right)}
            };
        }
    }
    return {{"node_type", "Condition"}};
}

auto ColumnTypeName(ColumnDef::Type type) -> std::string { return type == ColumnDef::Type::Int ? "int" : "string"; }

auto ColumnTypeFromName(const std::string& value) -> ColumnDef::Type {
    return value == "int" ? ColumnDef::Type::Int : ColumnDef::Type::String;
}

auto ToJson(const ColumnDef& column) -> nlohmann::json {
    nlohmann::json modifiers = nlohmann::json::array();
    if (column.Indexed) {
        modifiers.push_back("INDEXED");
    }
    if (column.NotNull) {
        modifiers.push_back("NOT_NULL");
    }

    nlohmann::json json =
        {{"name", column.Name}, {"data_type", ColumnTypeName(column.ColumnType)}, {"modifiers", modifiers}};
    if (column.DefaultValue) {
        json["default_value"] = ToJson(*column.DefaultValue);
    }
    return json;
}

auto TableToString(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

auto TableFromString(const std::string& value) -> TableRef {
    const auto dot = value.find('.');
    if (dot == std::string::npos) {
        return TableRef(value);
    }
    return TableRef(value.substr(0, dot), value.substr(dot + 1));
}

auto LiteralFromJson(const nlohmann::json& json) -> std::unique_ptr<Literal> {
    if (!json.is_object()) {
        if (json.is_null()) return std::make_unique<Literal>(Literal::Type::Null, "NULL");
        if (json.is_number_integer())
            return std::make_unique<Literal>(Literal::Type::Integer, std::to_string(json.get<int>()));
        return std::make_unique<Literal>(Literal::Type::String, json.get<std::string>());
    }

    const auto data_type = json.value("data_type", "string");
    const auto literal_type = LiteralTypeFromString(data_type);
    if (literal_type == Literal::Type::Null) {
        return std::make_unique<Literal>(Literal::Type::Null, "NULL");
    }
    if (literal_type == Literal::Type::Integer) {
        if (json.contains("value") && json["value"].is_number_integer()) {
            return std::make_unique<Literal>(Literal::Type::Integer, std::to_string(json["value"].get<int>()));
        }
        return std::make_unique<Literal>(Literal::Type::Integer, json.value("value", "0"));
    }

    return std::make_unique<Literal>(Literal::Type::String, json.value("value", ""));
}

auto ExpressionFromJson(const nlohmann::json& json) -> std::unique_ptr<Expression> {
    const auto type = json.value("node_type", "");
    if (type == "Identifier") {
        return std::make_unique<IdentifierExpr>(json.value("value", ""));
    }
    if (type == "AggregateFunction") {
        auto func = AggregateFromName(json.value("function", "SUM"));
        const auto arg = json.value("argument", nlohmann::json::object());
        return std::make_unique<AggregateExpr>(func, arg.value("value", ""));
    }
    if (type == "Literal") {
        return std::make_unique<LiteralExpr>(LiteralFromJson(json));
    }

    return std::make_unique<LiteralExpr>(LiteralFromJson(json));
}

auto ConditionFromJson(const nlohmann::json& json) -> std::unique_ptr<Condition> {
    const auto type = json.value("node_type", "");
    if (type == "BetweenExpression") {
        return std::make_unique<BetweenCondition>(
            ExpressionFromJson(json.at("value")),
            ExpressionFromJson(json.at("lower")),
            ExpressionFromJson(json.at("upper"))
        );
    }

    if (type == "BinaryExpression") {
        const auto op = json.value("operator", "");
        if (op == "AND") {
            return std::make_unique<
                AndCondition>(ConditionFromJson(json.at("left")), ConditionFromJson(json.at("right")));
        }
        if (op == "OR") {
            return std::make_unique<
                OrCondition>(ConditionFromJson(json.at("left")), ConditionFromJson(json.at("right")));
        }
        if (op == "LIKE") {
            const auto pattern_literal = LiteralFromJson(json.at("right"));
            return std::make_unique<LikeCondition>(ExpressionFromJson(json.at("left")), pattern_literal->Value);
        }
        return std::make_unique<ComparisonCondition>(
            ExpressionFromJson(json.at("left")),
            ComparisonOperatorFromName(op),
            ExpressionFromJson(json.at("right"))
        );
    }

    return std::make_unique<ComparisonCondition>(
        ExpressionFromJson(json.at("left")),
        ComparisonCondition::Operator::Equal,
        ExpressionFromJson(json.at("right"))
    );
}

auto ColumnFromJson(const nlohmann::json& json) -> ColumnDef {
    ColumnDef column(json.value("name", ""), ColumnTypeFromName(json.value("data_type", "string")));
    if (json.contains("modifiers") && json["modifiers"].is_array()) {
        for (const auto& modifier : json["modifiers"]) {
            const auto value = modifier.get<std::string>();
            if (value == "NOT_NULL") column.NotNull = true;
            if (value == "INDEXED") column.Indexed = true;
        }
    }
    if (json.contains("default_value")) {
        column.DefaultValue = LiteralFromJson(json["default_value"]);
    }
    return column;
}

auto SelectItemFromJson(const nlohmann::json& json) -> SelectItem {
    auto expr = ExpressionFromJson(json);
    const auto alias = json.value("alias", "");
    return SelectItem(std::move(expr), alias);
}

auto SerializeAst(const Statement& statement) -> nlohmann::json {
    switch (statement.KindOf()) {
        case Statement::Kind::CreateDatabase: {
            const auto* create_db = static_cast<const CreateDatabaseStmt*>(&statement);
            return {{"node_type", "CreateDatabaseStatement"}, {"database", create_db->DatabaseName}};
        }
        case Statement::Kind::DropDatabase: {
            const auto* drop_db = static_cast<const DropDatabaseStmt*>(&statement);
            return {{"node_type", "DropDatabaseStatement"}, {"database", drop_db->DatabaseName}};
        }
        case Statement::Kind::UseDatabase: {
            const auto* use_db = static_cast<const UseDatabaseStmt*>(&statement);
            return {{"node_type", "UseDatabaseStatement"}, {"database", use_db->DatabaseName}};
        }
        case Statement::Kind::CreateUser: {
            const auto* create_user = static_cast<const CreateUserStmt*>(&statement);
            return {{"node_type", "CreateUserStatement"}, {"username", create_user->Username}};
        }
        case Statement::Kind::Grant: {
            const auto* grant = static_cast<const GrantStmt*>(&statement);
            return {
                {"node_type", "GrantStatement"},
                {"permissions", grant->Permissions},
                {"scope", TableToString(grant->Scope)},
                {"username", grant->Username}
            };
        }
        case Statement::Kind::Revoke: {
            const auto* revoke = static_cast<const RevokeStmt*>(&statement);
            return {
                {"node_type", "RevokeStatement"},
                {"permissions", revoke->Permissions},
                {"scope", TableToString(revoke->Scope)},
                {"username", revoke->Username}
            };
        }
        case Statement::Kind::CreateTable: {
            const auto* create = static_cast<const CreateTableStmt*>(&statement);
            nlohmann::json columns = nlohmann::json::array();
            for (const auto& column : create->Columns) {
                columns.push_back(ToJson(column));
            }
            return {
                {"node_type", "CreateTableStatement"},
                {"table", TableToString(create->Table)},
                {"columns", columns}
            };
        }
        case Statement::Kind::Select: {
            const auto* select = static_cast<const SelectStmt*>(&statement);
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
            nlohmann::json json =
                {{"node_type", "SelectStatement"}, {"table", TableToString(select->Table)}, {"projections", projections}
                };
            if (select->WhereClause) {
                json["where_clause"] = ToJson(*select->WhereClause);
            }
            return json;
        }
        case Statement::Kind::Revert: {
            const auto* revert = static_cast<const RevertStmt*>(&statement);
            return {
                {"node_type", "RevertStatement"},
                {"table", TableToString(revert->Table)},
                {"target_timestamp", revert->Timestamp}
            };
        }
        case Statement::Kind::Insert: {
            const auto* insert = static_cast<const InsertStmt*>(&statement);
            nlohmann::json values = nlohmann::json::array();
            for (const auto& row : insert->Values) {
                nlohmann::json row_json = nlohmann::json::array();
                for (const auto& literal : row) {
                    row_json.push_back(ToJson(*literal));
                }
                values.push_back(std::move(row_json));
            }
            return {
                {"node_type", "InsertStatement"},
                {"table", TableToString(insert->Table)},
                {"columns", insert->Columns},
                {"values", values}
            };
        }
        case Statement::Kind::Update: {
            const auto* update = static_cast<const UpdateStmt*>(&statement);
            nlohmann::json assignments = nlohmann::json::array();
            for (const auto& [column, value] : update->Assignments) {
                assignments.push_back({{"column", column}, {"value", ToJson(*value)}});
            }
            nlohmann::json json =
                {{"node_type", "UpdateStatement"}, {"table", TableToString(update->Table)}, {"assignments", assignments}
                };
            if (update->WhereClause) {
                json["where_clause"] = ToJson(*update->WhereClause);
            }
            return json;
        }
        case Statement::Kind::Delete: {
            const auto* del = static_cast<const DeleteStmt*>(&statement);
            nlohmann::json json = {{"node_type", "DeleteStatement"}, {"table", TableToString(del->Table)}};
            if (del->WhereClause) {
                json["where_clause"] = ToJson(*del->WhereClause);
            }
            return json;
        }
        case Statement::Kind::DropTable: {
            const auto* drop = static_cast<const DropTableStmt*>(&statement);
            return {{"node_type", "DropTableStatement"}, {"table", TableToString(drop->Table)}};
        }
    }
    return {{"node_type", "Statement"}};
}

auto DeserializeAst(const nlohmann::json& json) -> std::unique_ptr<Statement> {
    const auto type = json.value("node_type", "");
    if (type == "CreateDatabaseStatement") {
        return std::make_unique<CreateDatabaseStmt>(json.value("database", ""));
    }
    if (type == "DropDatabaseStatement") {
        return std::make_unique<DropDatabaseStmt>(json.value("database", ""));
    }
    if (type == "UseDatabaseStatement") {
        return std::make_unique<UseDatabaseStmt>(json.value("database", ""));
    }
    if (type == "CreateUserStatement") {
        return std::make_unique<CreateUserStmt>(json.value("username", ""), "");
    }
    if (type == "GrantStatement") {
        return std::make_unique<GrantStmt>(
            json.value("permissions", std::vector<std::string>{}),
            TableFromString(json.value("scope", "*.*")),
            json.value("username", "")
        );
    }
    if (type == "RevokeStatement") {
        return std::make_unique<RevokeStmt>(
            json.value("permissions", std::vector<std::string>{}),
            TableFromString(json.value("scope", "*.*")),
            json.value("username", "")
        );
    }
    if (type == "CreateTableStatement") {
        std::vector<ColumnDef> columns;
        if (json.contains("columns") && json["columns"].is_array()) {
            for (const auto& col : json["columns"]) {
                columns.push_back(ColumnFromJson(col));
            }
        }
        return std::make_unique<CreateTableStmt>(TableFromString(json.value("table", "")), std::move(columns));
    }
    if (type == "SelectStatement") {
        std::vector<SelectItem> items;
        bool select_all = false;
        if (json.contains("projections") && json["projections"].is_array()) {
            for (const auto& projection : json["projections"]) {
                if (projection.value("node_type", "") == "Wildcard") {
                    select_all = true;
                    continue;
                }
                items.push_back(SelectItemFromJson(projection));
            }
        }
        std::unique_ptr<Condition> where;
        if (json.contains("where_clause")) {
            where = ConditionFromJson(json["where_clause"]);
        }
        return std::make_unique<
            SelectStmt>(select_all, std::move(items), TableFromString(json.value("table", "")), std::move(where));
    }
    if (type == "RevertStatement") {
        return std::make_unique<
            RevertStmt>(TableFromString(json.value("table", "")), json.value("target_timestamp", ""));
    }
    if (type == "InsertStatement") {
        std::vector<std::vector<std::unique_ptr<Literal>>> values;
        if (json.contains("values") && json["values"].is_array()) {
            for (const auto& row : json["values"]) {
                std::vector<std::unique_ptr<Literal>> row_values;
                for (const auto& literal : row) {
                    row_values.push_back(LiteralFromJson(literal));
                }
                values.push_back(std::move(row_values));
            }
        }
        return std::make_unique<InsertStmt>(
            TableFromString(json.value("table", "")),
            json.value("columns", std::vector<std::string>{}),
            std::move(values)
        );
    }
    if (type == "UpdateStatement") {
        std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assignments;
        if (json.contains("assignments") && json["assignments"].is_array()) {
            for (const auto& item : json["assignments"]) {
                assignments.emplace_back(item.value("column", ""), ExpressionFromJson(item.at("value")));
            }
        }
        std::unique_ptr<Condition> where;
        if (json.contains("where_clause")) {
            where = ConditionFromJson(json["where_clause"]);
        }
        return std::make_unique<
            UpdateStmt>(TableFromString(json.value("table", "")), std::move(assignments), std::move(where));
    }
    if (type == "DeleteStatement") {
        std::unique_ptr<Condition> where;
        if (json.contains("where_clause")) {
            where = ConditionFromJson(json["where_clause"]);
        }
        return std::make_unique<DeleteStmt>(TableFromString(json.value("table", "")), std::move(where));
    }
    if (type == "DropTableStatement") {
        return std::make_unique<DropTableStmt>(TableFromString(json.value("table", "")));
    }
    return std::make_unique<SelectStmt>(false, std::vector<SelectItem>{}, TableRef(""));
}

}  // namespace qdb::server
