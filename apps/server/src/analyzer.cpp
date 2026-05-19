#include <qdb/server/analyzer.h>

namespace qdb::server {

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

