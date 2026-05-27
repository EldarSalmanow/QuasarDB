#include <qdb/storage/executor.h>

#include <qdb/core/response.h>
#include <qdb/server/ast.h>

#include <algorithm>
#include <optional>
#include <regex>
#include <stdexcept>

namespace qdb::storage {

auto JsonValue(const Value& value, Interner& interner) -> nlohmann::json {
    if (value.IsNull()) {
        return nullptr;
    }

    if (value.IsInt()) {
        return value.AsInt();
    }

    return std::string(interner.View(value.AsString()));
}

auto LiteralValue(const qdb::server::Literal& literal, Interner& interner) -> Value {
    using Type = qdb::server::Literal::Type;

    if (literal.LiteralType == Type::Null) {
        return {};
    }

    if (literal.LiteralType == Type::Integer) {
        return Value(std::stoi(literal.Value));
    }

    return interner.Intern(literal.Value);
}

auto ExpressionValue(const qdb::server::Expression& expression, const Schema& schema, const Record& record,
                     Interner& interner) -> Value {
    using Kind = qdb::server::Expression::Kind;

    if (expression.KindOf() == Kind::Literal) {
        return LiteralValue(*static_cast<const qdb::server::LiteralExpr&>(expression).LiteralValue, interner);
    }

    if (expression.KindOf() == Kind::Identifier) {
        const auto& id = static_cast<const qdb::server::IdentifierExpr&>(expression);
        const auto index = schema.ColumnIndex(id.Name);

        if (index < 0) {
            throw std::runtime_error("[ERROR in qdb::storage::ExpressionValue]: "
                                     "Unknown column '" + id.Name + "'!");
        }

        return record[index];
    }

    throw std::runtime_error("[ERROR in qdb::storage::ExpressionValue]: "
                             "Unsupported expression!");
}

auto CompareStrings(std::string_view left, std::string_view right,
                    qdb::server::ComparisonCondition::Operator op) -> SqlBool {
    using Operator = qdb::server::ComparisonCondition::Operator;

    switch (op) {
        case Operator::Equal: return left == right ? SqlBool::TRUE : SqlBool::FALSE;
        case Operator::NotEqual: return left != right ? SqlBool::TRUE : SqlBool::FALSE;
        case Operator::Less: return left < right ? SqlBool::TRUE : SqlBool::FALSE;
        case Operator::Greater: return left > right ? SqlBool::TRUE : SqlBool::FALSE;
        case Operator::LessEqual: return left <= right ? SqlBool::TRUE : SqlBool::FALSE;
        case Operator::GreaterEqual: return left >= right ? SqlBool::TRUE : SqlBool::FALSE;
    }

    return SqlBool::UNKNOWN;
}

auto EvaluateCondition(const qdb::server::Condition& condition, const Schema& schema, const Record& record,
                       Interner& interner) -> SqlBool {
    using namespace qdb::server;

    if (condition.KindOf() == Condition::Kind::Comparison) {
        const auto& node = static_cast<const ComparisonCondition&>(condition);
        const auto left = ExpressionValue(*node.Left, schema, record, interner);
        const auto right = ExpressionValue(*node.Right, schema, record, interner);

        if (left.IsNull() || right.IsNull()) {
            return SqlBool::UNKNOWN;
        }

        if (left.IsString() && right.IsString()) {
            return CompareStrings(interner.View(left.AsString()), interner.View(right.AsString()), node.Op);
        }

        switch (node.Op) {
            case ComparisonCondition::Operator::Equal: return left == right;
            case ComparisonCondition::Operator::NotEqual: return left != right;
            case ComparisonCondition::Operator::Less: return left < right;
            case ComparisonCondition::Operator::Greater: return left > right;
            case ComparisonCondition::Operator::LessEqual: return left <= right;
            case ComparisonCondition::Operator::GreaterEqual: return left >= right;
        }
    }

    if (condition.KindOf() == Condition::Kind::Between) {
        const auto& node = static_cast<const BetweenCondition&>(condition);
        const auto value = ExpressionValue(*node.Value, schema, record, interner);

        return (value >= ExpressionValue(*node.Lower, schema, record, interner)) &&
               (value <= ExpressionValue(*node.Upper, schema, record, interner));
    }

    if (condition.KindOf() == Condition::Kind::Like) {
        const auto& node = static_cast<const LikeCondition&>(condition);
        const auto value = ExpressionValue(*node.Value, schema, record, interner);

        if (value.IsNull()) {
            return SqlBool::UNKNOWN;
        }

        if (!value.IsString()) {
            throw std::runtime_error("[ERROR in qdb::storage::EvaluateCondition]: LIKE expects string value!");
        }

        return std::regex_match(std::string(interner.View(value.AsString())), std::regex(node.Pattern))
            ? SqlBool::TRUE
            : SqlBool::FALSE;
    }

    if (condition.KindOf() == Condition::Kind::And) {
        const auto& node = static_cast<const AndCondition&>(condition);

        return EvaluateCondition(*node.Left, schema, record, interner) &&
               EvaluateCondition(*node.Right, schema, record, interner);
    }
    if (condition.KindOf() == Condition::Kind::Or) {
        const auto& node = static_cast<const OrCondition&>(condition);

        return EvaluateCondition(*node.Left, schema, record, interner) ||
               EvaluateCondition(*node.Right, schema, record, interner);
    }

    throw std::runtime_error("[ERROR in qdb::storage::EvaluateCondition]: Unsupported condition!");
}

auto AggregateName(qdb::server::AggregateExpr::Function function) -> std::string {
    switch (function) {
        case qdb::server::AggregateExpr::Function::Sum: return "SUM";
        case qdb::server::AggregateExpr::Function::Count: return "COUNT";
        case qdb::server::AggregateExpr::Function::Avg: return "AVG";
    }

    return "AGG";
}

auto ResultName(const qdb::server::SelectItem& item, const qdb::server::AggregateExpr& aggregate) -> std::string {
    return item.Alias.empty() ? AggregateName(aggregate.FunctionType) + "(" + aggregate.Column + ")" : item.Alias;
}

auto ColumnName(const qdb::server::SelectItem& item, const qdb::server::IdentifierExpr& id) -> std::string {
    return item.Alias.empty() ? id.Name : item.Alias;
}

auto IndexedRecords(Table& table, Interner& interner,
                    const qdb::server::Condition& condition) -> std::optional<std::vector<Record>> {
    using namespace qdb::server;

    if (condition.KindOf() == Condition::Kind::And) {
        const auto& node = static_cast<const AndCondition&>(condition);

        if (auto records = IndexedRecords(table, interner, *node.Left)) {
            return records;
        }

        return IndexedRecords(table, interner, *node.Right);
    }

    if (condition.KindOf() != Condition::Kind::Comparison) {
        return std::nullopt;
    }

    const auto& comparison = static_cast<const ComparisonCondition&>(condition);

    if (comparison.Op != ComparisonCondition::Operator::Equal) {
        return std::nullopt;
    }

    auto lookup = [&](const Expression& column, const Expression& value) -> std::optional<std::vector<Record>> {
        if (column.KindOf() != Expression::Kind::Identifier || value.KindOf() != Expression::Kind::Literal) {
            return std::nullopt;
        }

        const auto& id = static_cast<const IdentifierExpr&>(column);
        const auto& literal = static_cast<const LiteralExpr&>(value);

        return table.find_by_index(id.Name, LiteralValue(*literal.LiteralValue, interner));
    };

    if (auto records = lookup(*comparison.Left, *comparison.Right)) {
        return records;
    }

    return lookup(*comparison.Right, *comparison.Left);
}

auto RecordsFor(Table& table, Interner& interner, const qdb::server::Condition* where) -> std::vector<Record> {
    if (where == nullptr) {
        return table.records();
    }

    if (auto records = IndexedRecords(table, interner, *where)) {
        return std::move(*records);
    }

    return table.records();
}

auto MatchingRecords(Table& table, Interner& interner, const qdb::server::Condition* where) -> std::vector<Record> {
    auto records = RecordsFor(table, interner, where);

    if (where == nullptr) {
        return records;
    }

    std::vector<Record> result;
    for (auto& record : records) {
        if (EvaluateCondition(*where, table.schema(), record, interner) == SqlBool::TRUE) {
            result.push_back(std::move(record));
        }
    }

    return result;
}

Executor::Executor(Table& table, Interner& interner)
        : table_(table), interner_(interner) {}

auto Executor::Execute(const qdb::server::Statement& statement) -> nlohmann::json {
    using namespace qdb::server;

    if (statement.KindOf() == Statement::Kind::Insert) {
        const auto* node = static_cast<const InsertStmt*>(&statement);

        std::vector<std::vector<Value>> rows;
        rows.reserve(node->Values.size());
        for (const auto& source_row : node->Values) {
            std::vector<Value> row;
            row.reserve(source_row.size());
            for (const auto& literal : source_row) {
                row.push_back(LiteralValue(*literal, interner_));
            }

            rows.push_back(std::move(row));
        }

        table_.insert_multiple(std::move(rows), node->Columns);

        return qdb::core::SuccessJson("Rows inserted", {{"rows_affected", node->Values.size()}});
    }

    if (statement.KindOf() == Statement::Kind::Update) {
        const auto* node = static_cast<const UpdateStmt*>(&statement);

        std::vector<Record> changed;
        for (auto record : MatchingRecords(table_, interner_, node->WhereClause.get())) {
            for (const auto& [column, expression] : node->Assignments) {
                const auto index = table_.schema().ColumnIndex(column);

                if (index < 0) {
                    throw std::runtime_error("Unknown column: " + column);
                }

                record[index] = ExpressionValue(*expression, table_.schema(), record, interner_);
            }

            changed.push_back(std::move(record));
        }

        const auto errors = table_.update_multiple(changed);

        if (!errors.empty()) {
            throw std::runtime_error(errors.front().second);
        }

        return qdb::core::SuccessJson("Rows updated", {{"rows_affected", changed.size()}});
    }

    if (statement.KindOf() == Statement::Kind::Delete) {
        const auto* node = static_cast<const DeleteStmt*>(&statement);

        auto deleted = MatchingRecords(table_, interner_, node->WhereClause.get());

        table_.delete_multiple(deleted);

        return qdb::core::SuccessJson("Rows deleted", {{"rows_affected", deleted.size()}});
    }

    if (statement.KindOf() == Statement::Kind::Select) {
        return qdb::core::SuccessJson(
            "Rows selected",
            {{"result", SelectRows(static_cast<const SelectStmt&>(statement), interner_, table_)}}
        );
    }

    if (statement.KindOf() == Statement::Kind::Revert) {
        table_.revert(static_cast<const RevertStmt&>(statement).Timestamp);
        return qdb::core::SuccessJson("Table reverted", {{"rows_affected", 0}});
    }

    if (statement.KindOf() == Statement::Kind::CreateUser
     || statement.KindOf() == Statement::Kind::Grant
     || statement.KindOf() == Statement::Kind::Revoke) {
        return qdb::core::ErrorJson("Storage shard does not manage users or permissions");
    }

    if (statement.KindOf() == Statement::Kind::CreateDatabase
     || statement.KindOf() == Statement::Kind::DropDatabase
     || statement.KindOf() == Statement::Kind::UseDatabase) {
        return qdb::core::ErrorJson("Storage shard does not manage databases");
    }

    if (statement.KindOf() == Statement::Kind::CreateTable) {
        return qdb::core::ErrorJson(
            "Storage shard table lifecycle is handled by application: " +
            static_cast<const CreateTableStmt&>(statement).Table.Table
        );
    }

    if (statement.KindOf() == Statement::Kind::DropTable) {
        return qdb::core::ErrorJson(
            "Storage shard table lifecycle is handled by application: " +
            static_cast<const DropTableStmt&>(statement).Table.Table
        );
    }

    return qdb::core::ErrorJson("Unsupported statement");
}

auto Executor::SelectRows(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json {
    const auto has_aggregate = std::any_of(node.SelectItems.begin(), node.SelectItems.end(), [](const auto& item) {
        return item.Expr->KindOf() == qdb::server::Expression::Kind::Aggregate;
    });

    if (has_aggregate) {
        return nlohmann::json::array({AggregateSelect(node, interner, table)});
    }

    nlohmann::json result = nlohmann::json::array();
    for (const auto& record : MatchingRecords(table, interner, node.WhereClause.get())) {
        nlohmann::json row = nlohmann::json::object();

        if (node.SelectAll) {
            for (size_t i = 0; i < table.schema().Size(); ++i) {
                row[table.schema()[i].Name()] = JsonValue(record[i], interner);
            }
        } else {
            for (const auto& item : node.SelectItems) {
                if (item.Expr->KindOf() != qdb::server::Expression::Kind::Identifier) {
                    throw std::runtime_error("[ERROR in qdb::storage::SelectRows]: "
                                             "Only column expressions are supported in SELECT!");
                }

                const auto& id = static_cast<const qdb::server::IdentifierExpr&>(*item.Expr);

                row[ColumnName(item, id)] = JsonValue(ExpressionValue(id, table.schema(), record, interner), interner);
            }
        }

        result.push_back(std::move(row));
    }
    return result;
}

auto Executor::AggregateSelect(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json {
    nlohmann::json row = nlohmann::json::object();
    const auto records = MatchingRecords(table, interner, node.WhereClause.get());

    for (const auto& item : node.SelectItems) {
        if (item.Expr->KindOf() != qdb::server::Expression::Kind::Aggregate) {
            throw std::runtime_error("[ERROR in qdb::storage::AggregateSelect]: "
                                     "Only aggregate SELECT is implemented!");
        }

        const auto* aggregate = static_cast<const qdb::server::AggregateExpr*>(item.Expr.get());

        const auto column_index = table.schema().ColumnIndex(aggregate->Column);
        if (column_index < 0) {
            throw std::runtime_error("[ERROR in qdb::storage::AggregateSelect]: "
                                     "Unknown column '" + aggregate->Column + "'!");
        }

        int64_t sum = 0;
        int64_t count = 0;
        for (const auto& record : records) {
            const auto& value = record[column_index];

            if (aggregate->FunctionType == qdb::server::AggregateExpr::Function::Count) {
                count += value.IsNull() ? 0 : 1;

                continue;
            }

            if (value.IsNull()) {
                continue;
            }

            if (!value.IsInt()) {
                throw std::runtime_error("[ERROR in qdb::storage::AggregateSelect]: "
                                         "Aggregate expects INT column '" + aggregate->Column + "'!");
            }

            sum += value.AsInt();
            ++count;
        }

        const auto name = ResultName(item, *aggregate);
        switch (aggregate->FunctionType) {
            case qdb::server::AggregateExpr::Function::Count: {
                row[name] = count;

                break;
            }
            case qdb::server::AggregateExpr::Function::Sum: {
                row[name] = sum;

                break;
            }
            case qdb::server::AggregateExpr::Function::Avg: {
                row[name] = count == 0 ? nlohmann::json(nullptr) : nlohmann::json(static_cast<double>(sum) / count);

                break;
            }
        }
    }

    return row;
}

}  // namespace qdb::storage
