#include <qdb/storage/executor.h>
#include <qdb/core/response.h>
#include <qdb/server/ast.h>
#include <qdb/storage/condition_evaluator.h>

#include <cstdint>
#include <stdexcept>
#include <string>

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

}  // namespace

Executor::Executor(DatabaseManager& db_manager) : db_manager_(db_manager) {}

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

void Executor::Visit(const qdb::server::CreateDatabaseStmt&) {
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("CREATE DATABASE - stub")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::DropDatabaseStmt&) {
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("DROP DATABASE - stub")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::UseDatabaseStmt& node) {
    current_db_ = db_manager_.UseDatabase(node.DatabaseName);
    if (current_db_ == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error()
            .Message("Database not found: " + node.DatabaseName)
            .Build()
            .ToJsonObject();
        return;
    }

    result_ = qdb::core::ResponseBuilder::Success()
        .Message("Database selected")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::RevertStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("REVERT not implemented")
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::CreateTableStmt&) {
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("CREATE TABLE - stub")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::DropTableStmt&) {
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("DROP TABLE - stub")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::InsertStmt&) {
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("INSERT - stub")
        .Data({{"rows_affected", 1}})
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::UpdateStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("UPDATE not implemented")
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::DeleteStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("DELETE not implemented")
        .Build()
        .ToJsonObject();
}

void Executor::Visit(const qdb::server::SelectStmt& node) {
    auto* db = CurrentDb(node.Table);
    if (db == nullptr) {
        result_ = qdb::core::ResponseBuilder::Error().Message("Database is not selected").Build().ToJsonObject();
        return;
    }

    auto table = db->tables_.find(node.Table.Table);
    if (table == db->tables_.end()) {
        result_ = qdb::core::ResponseBuilder::Error()
            .Message("Table not found: " + node.Table.Table)
            .Build()
            .ToJsonObject();
        return;
    }

    if (node.SelectAll || node.SelectItems.empty()) {
        result_ = qdb::core::ResponseBuilder::Error().Message("SELECT not implemented").Build().ToJsonObject();
        return;
    }

    result_ = qdb::core::ResponseBuilder::Success()
        .Message("SELECT aggregates")
        .Data({{"result", nlohmann::json::array({AggregateSelect(node, *db, table->second)})}})
        .Build()
        .ToJsonObject();
}

auto Executor::CurrentDb(const qdb::server::TableRef& table) -> Database* {
    return table.Database.empty() ? current_db_ : db_manager_.UseDatabase(table.Database);
}

auto Executor::AggregateSelect(const qdb::server::SelectStmt& node, Database& db, Table& table) -> nlohmann::json {
    nlohmann::json row = nlohmann::json::object();
    const auto records = table.records();

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
                ConditionEvaluator(table.schema(), record, db.interner_).Evaluate(*node.WhereClause) !=
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
