#include <qdb/server/analyzer.h>

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace qdb::server {

auto SemanticError(const std::string& message) -> std::runtime_error {
    return std::runtime_error("SEMANTIC_ERROR: " + message);
}

auto ValidateCreateTable(const CreateTableStmt& create) -> void {
    std::unordered_set<std::string> names;
    for (const auto& column : create.Columns) {
        if (!names.insert(column.Name).second) {
            throw SemanticError("duplicate column '" + column.Name + "'");
        }
        if (!column.DefaultValue) {
            continue;
        }

        if (column.DefaultValue->LiteralType == Literal::Type::Null) {
            if (column.NotNull) {
                throw SemanticError("column '" + column.Name + "' cannot be NOT_NULL with DEFAULT NULL");
            }
            continue;
        }

        const bool int_default = column.DefaultValue->LiteralType == Literal::Type::Integer;
        if (column.ColumnType == ColumnDef::Type::Int && !int_default) {
            throw SemanticError("column '" + column.Name + "' expects INT default");
        }
        if (column.ColumnType == ColumnDef::Type::String && int_default) {
            throw SemanticError("column '" + column.Name + "' expects STRING default");
        }
    }
}

auto ValidateInsert(const InsertStmt& insert) -> void {
    std::unordered_set<std::string> names;
    for (const auto& column : insert.Columns) {
        if (!names.insert(column).second) {
            throw SemanticError("duplicate insert column '" + column + "'");
        }
    }
    for (const auto& row : insert.Values) {
        if (row.size() != insert.Columns.size()) {
            throw SemanticError("INSERT values count does not match columns count");
        }
    }
}

auto Analyzer::ValidateStatement(const Statement& statement) -> void {
    switch (statement.KindOf()) {
        case Statement::Kind::CreateTable:
            ValidateCreateTable(static_cast<const CreateTableStmt&>(statement));
            break;
        case Statement::Kind::Insert:
            ValidateInsert(static_cast<const InsertStmt&>(statement));
            break;
        default:
            break;
    }
}

auto Analyzer::TableFromStatement(const Statement& statement) -> std::optional<TableRef> {
    switch (statement.KindOf()) {
        case Statement::Kind::CreateTable: return static_cast<const CreateTableStmt&>(statement).Table;
        case Statement::Kind::DropTable: return static_cast<const DropTableStmt&>(statement).Table;
        case Statement::Kind::Insert: return static_cast<const InsertStmt&>(statement).Table;
        case Statement::Kind::Update: return static_cast<const UpdateStmt&>(statement).Table;
        case Statement::Kind::Delete: return static_cast<const DeleteStmt&>(statement).Table;
        case Statement::Kind::Select: return static_cast<const SelectStmt&>(statement).Table;
        case Statement::Kind::Revert: return static_cast<const RevertStmt&>(statement).Table;
        default: return std::nullopt;
    }
}

}  // namespace qdb::server
