#include <qdb/storage/executor.h>
#include <qdb/core/response.h>
#include <qdb/server/ast.h>

namespace qdb::storage {

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

void Executor::Visit(const qdb::server::UseDatabaseStmt&) {
    result_ = qdb::core::ResponseBuilder::Success()
        .Message("USE DATABASE - stub")
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

void Executor::Visit(const qdb::server::SelectStmt&) {
    result_ = qdb::core::ResponseBuilder::Error()
        .Message("SELECT not implemented")
        .Build()
        .ToJsonObject();
}

}
