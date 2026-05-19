#include <qdb/server/analyzer.h>

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace qdb::server {

namespace {

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

}  // namespace

auto Analyzer::ValidateStatement(const Statement& statement) -> void {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) {
        ValidateCreateTable(*create);
        return;
    }
    if (const auto* insert = dynamic_cast<const InsertStmt*>(&statement)) {
        ValidateInsert(*insert);
    }
}

auto Analyzer::TableFromStatement(const Statement& statement) -> std::optional<TableRef> {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) return create->Table;
    if (const auto* drop = dynamic_cast<const DropTableStmt*>(&statement)) return drop->Table;
    if (const auto* insert = dynamic_cast<const InsertStmt*>(&statement)) return insert->Table;
    if (const auto* update = dynamic_cast<const UpdateStmt*>(&statement)) return update->Table;
    if (const auto* del = dynamic_cast<const DeleteStmt*>(&statement)) return del->Table;
    if (const auto* select = dynamic_cast<const SelectStmt*>(&statement)) return select->Table;
    if (const auto* revert = dynamic_cast<const RevertStmt*>(&statement)) return revert->Table;

    return std::nullopt;
}

}  // namespace qdb::server
