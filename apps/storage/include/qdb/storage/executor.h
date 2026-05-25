#ifndef QUASARDB_EXECUTOR_H
#define QUASARDB_EXECUTOR_H

#include <qdb/server/ast.h>
#include <qdb/storage/interner.h>
#include <qdb/storage/table.h>

#include <nlohmann/json.hpp>

#include <optional>

namespace qdb::storage {

class Executor : public qdb::server::AstVisitor {
public:
    Executor(Table& table, Interner& interner);

    auto Result() -> std::optional<nlohmann::json>;

    void Visit(const qdb::server::Literal& node) override;
    void Visit(const qdb::server::IdentifierExpr& node) override;
    void Visit(const qdb::server::LiteralExpr& node) override;
    void Visit(const qdb::server::AggregateExpr& node) override;
    void Visit(const qdb::server::ComparisonCondition& node) override;
    void Visit(const qdb::server::BetweenCondition& node) override;
    void Visit(const qdb::server::LikeCondition& node) override;
    void Visit(const qdb::server::AndCondition& node) override;
    void Visit(const qdb::server::OrCondition& node) override;
    void Visit(const qdb::server::CreateDatabaseStmt& node) override;
    void Visit(const qdb::server::DropDatabaseStmt& node) override;
    void Visit(const qdb::server::UseDatabaseStmt& node) override;
    void Visit(const qdb::server::CreateUserStmt& node) override;
    void Visit(const qdb::server::GrantStmt& node) override;
    void Visit(const qdb::server::RevokeStmt& node) override;
    void Visit(const qdb::server::RevertStmt& node) override;
    void Visit(const qdb::server::CreateTableStmt& node) override;
    void Visit(const qdb::server::DropTableStmt& node) override;
    void Visit(const qdb::server::InsertStmt& node) override;
    void Visit(const qdb::server::UpdateStmt& node) override;
    void Visit(const qdb::server::DeleteStmt& node) override;
    void Visit(const qdb::server::SelectStmt& node) override;

private:
    auto CurrentTable(const qdb::server::TableRef& table) -> Table*;

    auto ValueOf(const qdb::server::Literal& literal, Interner& interner) -> Value;

    auto ValueOf(const qdb::server::Expression& expression, const Record& record, const Schema& schema,
                 Interner& interner) -> Value;

    auto RecordsFor(Table& table, Interner& interner, const qdb::server::Condition* where) -> std::vector<Record>;

    auto IndexedRecords(Table& table, Interner& interner, const qdb::server::Condition& condition)
        -> std::optional<std::vector<Record>>;

    auto CurrentInterner(const qdb::server::TableRef& table) -> Interner*;

    auto SelectRows(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json;

    auto AggregateSelect(const qdb::server::SelectStmt& node, Interner& interner, Table& table) -> nlohmann::json;

private:
    Table& table_;
    Interner& interner_;
    std::optional<nlohmann::json> result_;
};

}

#endif  // QUASARDB_EXECUTOR_H
