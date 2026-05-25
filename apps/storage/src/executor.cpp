#include <qdb/storage/executor.h>
#include <qdb/core/response.h>
#include <qdb/server/ast.h>
#include <qdb/storage/condition_evaluator.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace qdb::storage {

namespace {

auto LiteralColumnName(qdb::server::AggregateExpr::Function function) -> std::string {
    switch (function) {
        case qdb::server::AggregateExpr::Function::Sum:
            return "SUM";
        case qdb::server::AggregateExpr::Function::Count:
            return "COUNT";
        case qdb::server::AggregateExpr::Function::Avg:
            return "AVG";
    }
    return "AGG";
}

auto ResultName(const qdb::server::SelectItem& item, const qdb::server::AggregateExpr& aggregate) -> std::string {
    return item.Alias.empty() ? LiteralColumnName(aggregate.FunctionType) + "(" + aggregate.Column + ")" : item.Alias;
}

auto ColumnName(const qdb::server::SelectItem& item, const qdb::server::IdentifierExpr& id) -> std::string {
    return item.Alias.empty() ? id.Name : item.Alias;
}

auto JsonValue(const Value& value) -> nlohmann::json {
    if (value.is_null()) {
        return nullptr;
    }
    if (value.is_int()) {
        return value.as_int();
    }
    return value.to_string();
}

}  // namespace

Executor::Executor(Table& table, Interner& interner) : table_(table), interner_(interner) {}

auto Executor::Result() -> std::optional<nlohmann::json> {
    return result_;
}

void Executor::Visit(const qdb::server::Literal&) {}
void Executor::Visit(const qdb::server::IdentifierExpr&) {}
void Executor::Visit(const qdb::server::LiteralExpr&) {}
void Executor::Visit(const qdb::server::AggregateExpr&) {}
void Executor::Visit(const qdb::server::ComparisonCondition&) {}
void Executor::Visit(const qdb::server::BetweenCondition&) {}
void Executor::Visit(const qdb::server::LikeCondition&) {}
void Executor::Visit(const qdb::server::AndCondition&) {}
void Executor::Visit(const qdb::server::OrCondition&) {}

void Executor::Visit(const qdb::server::CreateDatabaseStmt& node) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard does not manage databases: " + node.DatabaseName)
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::DropDatabaseStmt& node) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard does not manage databases: " + node.DatabaseName)
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::UseDatabaseStmt& node) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard does not manage databases: " + node.DatabaseName)
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::CreateUserStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard does not manage users")
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::GrantStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard does not manage permissions")
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::RevokeStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard does not manage permissions")
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::RevertStmt& node) {
    auto* table = CurrentTable(node.Table);
    if (table == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error().Message("Table not found: " + node.Table.Table).Build().ToJsonObject();
        return;
    }

    table->revert(node.Timestamp);
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("Table reverted")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::CreateTableStmt& node) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard table lifecycle is handled by application: " + node.Table.Table)
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::DropTableStmt& node) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("Storage shard table lifecycle is handled by application: " + node.Table.Table)
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::InsertStmt& node) {
    auto* table = CurrentTable(node.Table);
    auto* interner = CurrentInterner(node.Table);
    if (table == nullptr || interner == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error().Message("Table not found: " + node.Table.Table).Build().ToJsonObject();
        return;
    }

    std::vector<std::vector<Value>> rows;
    rows.reserve(node.Values.size());
    for (const auto& source_row : node.Values) {
        std::vector<Value> row;
        row.reserve(source_row.size());
        for (const auto& literal : source_row) {
            row.push_back(ValueOf(*literal, *interner));
        }
        rows.push_back(std::move(row));
    }

    table->insert_multiple(std::move(rows), node.Columns);
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("Rows inserted")
        .Data({{"rows_affected", node.Values.size()}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::UpdateStmt& node) {
    auto* table = CurrentTable(node.Table);
    auto* interner = CurrentInterner(node.Table);
    if (table == nullptr || interner == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error().Message("Table not found: " + node.Table.Table).Build().ToJsonObject();
        return;
    }

    std::vector<Record> changed;
    for (auto record : RecordsFor(*table, *interner, node.WhereClause.get())) {
        if (node.WhereClause &&
            ConditionEvaluator(table->schema(), record, *interner).Evaluate(*node.WhereClause) != SqlBool::TRUE)
        {
            continue;
        }
        for (const auto& [column, expression] : node.Assignments) {
            const auto index = table->schema().get_column_idx(column);
            if (index < 0) {
                throw std::runtime_error("Unknown column: " + column);
            }
            record[index] = ValueOf(*expression, record, table->schema(), *interner);
        }
        changed.push_back(std::move(record));
    }

    const auto errors = table->update_multiple(changed);
    if (!errors.empty()) {
        throw std::runtime_error(errors.front().second);
    }
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("Rows updated")
        .Data({{"rows_affected", changed.size()}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::DeleteStmt& node) {
    auto* table = CurrentTable(node.Table);
    auto* interner = CurrentInterner(node.Table);
    if (table == nullptr || interner == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error().Message("Table not found: " + node.Table.Table).Build().ToJsonObject();
        return;
    }

    std::vector<Record> deleted;
    for (auto record : RecordsFor(*table, *interner, node.WhereClause.get())) {
        if (!node.WhereClause ||
            ConditionEvaluator(table->schema(), record, *interner).Evaluate(*node.WhereClause) == SqlBool::TRUE)
        {
            deleted.push_back(std::move(record));
        }
    }

    table->delete_multiple(deleted);
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("Rows deleted")
        .Data({{"rows_affected", deleted.size()}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::SelectStmt& node) {
    auto* table = CurrentTable(node.Table);
    auto* interner = CurrentInterner(node.Table);
    if (table == nullptr || interner == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error()
            .Message("Table not found: " + node.Table.Table)
            .Build()
            .ToJsonObject();
        return;
    }

    const auto result = SelectRows(node, *interner, *table);
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("Rows selected")
        .Data({{"result", result}})
        .Build()
        .ToJsonObject();
}

auto Executor::CurrentTable(const qdb::server::TableRef& table) -> Table* {
    return table.Table.empty() || table.Table == table_.name() ? &table_ : nullptr;
}

auto Executor::CurrentInterner(const qdb::server::TableRef& table) -> Interner* {
    return CurrentTable(table) == nullptr ? nullptr : &interner_;
}

auto Executor::ValueOf(const qdb::server::Literal& literal, Interner& interner) -> Value {
    if (literal.LiteralType == qdb::server::Literal::Type::Null) {
        return {};
    }
    if (literal.LiteralType == qdb::server::Literal::Type::Integer) {
        return Value(std::stoi(literal.Value));
    }
    return interner.str_to_value(literal.Value);
}

auto Executor::ValueOf(const qdb::server::Expression& expression, const Record& record, const Schema& schema,
                       Interner& interner) -> Value {
    if (const auto* literal = dynamic_cast<const qdb::server::LiteralExpr*>(&expression)) {
        return ValueOf(*literal->LiteralValue, interner);
    }
    if (const auto* id = dynamic_cast<const qdb::server::IdentifierExpr*>(&expression)) {
        const auto index = schema.get_column_idx(id->Name);
        if (index < 0) {
            throw std::runtime_error("Unknown column: " + id->Name);
        }
        return record[index];
    }
    throw std::runtime_error("Unsupported expression");
}

auto Executor::RecordsFor(Table& table, Interner& interner, const qdb::server::Condition* where)
    -> std::vector<Record> {
    if (where == nullptr) {
        return table.records();
    }
    if (auto records = IndexedRecords(table, interner, *where)) {
        return std::move(*records);
    }
    return table.records();
}

auto Executor::IndexedRecords(Table& table, Interner& interner, const qdb::server::Condition& condition)
    -> std::optional<std::vector<Record>> {
    using namespace qdb::server;

    if (const auto* node = dynamic_cast<const AndCondition*>(&condition)) {
        if (auto records = IndexedRecords(table, interner, *node->Left)) {
            return records;
        }
        return IndexedRecords(table, interner, *node->Right);
    }

    const auto* comparison = dynamic_cast<const ComparisonCondition*>(&condition);
    if (comparison == nullptr || comparison->Op != ComparisonCondition::Operator::Equal) {
        return std::nullopt;
    }

    auto lookup = [&](const Expression& column, const Expression& value) -> std::optional<std::vector<Record>> {
        const auto* id = dynamic_cast<const IdentifierExpr*>(&column);
        const auto* literal = dynamic_cast<const LiteralExpr*>(&value);
        if (id == nullptr || literal == nullptr) {
            return std::nullopt;
        }
        return table.find_by_index(id->Name, ValueOf(*literal->LiteralValue, interner));
    };

    if (auto records = lookup(*comparison->Left, *comparison->Right)) {
        return records;
    }
    return lookup(*comparison->Right, *comparison->Left);
}

auto Executor::SelectRows(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json {
    const auto has_aggregate = std::any_of(node.SelectItems.begin(), node.SelectItems.end(), [](const auto& item) {
        return dynamic_cast<const qdb::server::AggregateExpr*>(item.Expr.get()) != nullptr;
    });
    if (has_aggregate) {
        return nlohmann::json::array({AggregateSelect(node, interner, table)});
    }

    nlohmann::json result = nlohmann::json::array();
    for (const auto& record : RecordsFor(table, interner, node.WhereClause.get())) {
        if (node.WhereClause &&
            ConditionEvaluator(table.schema(), record, interner).Evaluate(*node.WhereClause) != SqlBool::TRUE) {
            continue;
        }

        nlohmann::json row = nlohmann::json::object();
        if (node.SelectAll) {
            for (size_t i = 0; i < table.schema().size(); ++i) {
                row[table.schema()[i].name()] = JsonValue(record[i]);
            }
        } else {
            for (const auto& item : node.SelectItems) {
                const auto* id = dynamic_cast<const qdb::server::IdentifierExpr*>(item.Expr.get());
                if (id == nullptr) {
                    throw std::runtime_error("Only column expressions are supported in SELECT");
                }
                row[ColumnName(item, *id)] = JsonValue(ValueOf(*id, record, table.schema(), interner));
            }
        }
        result.push_back(std::move(row));
    }

    return result;
}

auto Executor::AggregateSelect(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json {
    nlohmann::json row = nlohmann::json::object();
    const auto records = RecordsFor(table, interner, node.WhereClause.get());

    for (const auto& item : node.SelectItems) {
        const auto* aggregate = dynamic_cast<const qdb::server::AggregateExpr*>(item.Expr.get());
        if (aggregate == nullptr) {
            throw std::runtime_error("Only aggregate SELECT is implemented");
        }

        const auto column_index = table.schema().get_column_idx(aggregate->Column);
        if (column_index < 0) {
            throw std::runtime_error("Unknown column: " + aggregate->Column);
        }

        int64_t sum = 0;
        int64_t count = 0;
        for (const auto& record : records) {
            if (node.WhereClause &&
                ConditionEvaluator(table.schema(), record, interner).Evaluate(*node.WhereClause) !=
                    SqlBool::TRUE) {
                continue;
            }

            const auto& value = record[column_index];
            if (aggregate->FunctionType == qdb::server::AggregateExpr::Function::Count) {
                count += value.is_null() ? 0 : 1;
                continue;
            }
            if (value.is_null()) {
                continue;
            }
            if (!value.is_int()) {
                throw std::runtime_error("Aggregate expects INT column: " + aggregate->Column);
            }
            sum += value.as_int();
            ++count;
        }

        const auto name = ResultName(item, *aggregate);
        switch (aggregate->FunctionType) {
            case qdb::server::AggregateExpr::Function::Count:
                row[name] = count;
                break;
            case qdb::server::AggregateExpr::Function::Sum:
                row[name] = sum;
                break;
            case qdb::server::AggregateExpr::Function::Avg:
                row[name] = count == 0 ? nlohmann::json(nullptr) : nlohmann::json(static_cast<double>(sum) / count);
                break;
        }
    }

    return row;
}

}
