#ifndef QUASARDB_EXECUTOR_H
#define QUASARDB_EXECUTOR_H

#include <qdb/server/ast.h>
#include <qdb/storage/db_manager.h>

#include <nlohmann/json.hpp>

#include <optional>

namespace qdb::storage {

class Executor : public qdb::server::AstVisitor {
public:
    explicit Executor(DatabaseManager& db_manager);

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
    void Visit(const qdb::server::RevertStmt& node) override;
    void Visit(const qdb::server::CreateTableStmt& node) override;
    void Visit(const qdb::server::DropTableStmt& node) override;
    void Visit(const qdb::server::InsertStmt& node) override;
    void Visit(const qdb::server::UpdateStmt& node) override;
    void Visit(const qdb::server::DeleteStmt& node) override;
    void Visit(const qdb::server::SelectStmt& node) override;

private:
    DatabaseManager& db_manager_;
    Database* current_db_ = nullptr;
    std::optional<nlohmann::json> result_;
};

}

#endif  // QUASARDB_EXECUTOR_H