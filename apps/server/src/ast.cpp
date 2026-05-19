#include <qdb/server/ast.h>

#include <utility>


namespace qdb::server {

ASTNode::~ASTNode() = default;

Expression::~Expression() = default;

Condition::~Condition() = default;

Statement::~Statement() = default;

Literal::Literal(Type type, std::string value)
        : LiteralType(type), Value(std::move(value)) {}

IdentifierExpr::IdentifierExpr(std::string name)
        : Name(std::move(name)) {}

LiteralExpr::LiteralExpr(std::unique_ptr<Literal> literal)
        : LiteralValue(std::move(literal)) {}

AggregateExpr::AggregateExpr(Function function, std::string columns)
        : FunctionType(function), Column(std::move(columns)) {}

ComparisonCondition::ComparisonCondition(std::unique_ptr<Expression> left, Operator op,
                                         std::unique_ptr<Expression> right)
        : Left(std::move(left)), Op(op), Right(std::move(right)) {}

BetweenCondition::BetweenCondition(std::unique_ptr<Expression> value, std::unique_ptr<Expression> lower,
                                   std::unique_ptr<Expression> upper)
        : Value(std::move(value)), Lower(std::move(lower)), Upper(std::move(upper)) {}

LikeCondition::LikeCondition(std::unique_ptr<Expression> value, std::string pattern)
        : Value(std::move(value)), Pattern(std::move(pattern)) {}

AndCondition::AndCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right)
        : Left(std::move(left)), Right(std::move(right)) {}

OrCondition::OrCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right)
        : Left(std::move(left)), Right(std::move(right)) {}

TableRef::TableRef(std::string table)
        : Table(std::move(table)) {}

TableRef::TableRef(std::string database, std::string table)
        : Database(std::move(database)), Table(std::move(table)) {}

ColumnDef::ColumnDef(std::string name, Type type)
        : Name(std::move(name)), ColumnType(type) {}

SelectItem::SelectItem(std::unique_ptr<Expression> expr, std::string alias)
        : Expr(std::move(expr)), Alias(std::move(alias)) {}

CreateDatabaseStmt::CreateDatabaseStmt(std::string name)
        : DatabaseName(std::move(name)) {}

DropDatabaseStmt::DropDatabaseStmt(std::string name)
        : DatabaseName(std::move(name)) {}

UseDatabaseStmt::UseDatabaseStmt(std::string name)
        : DatabaseName(std::move(name)) {}

RevertStmt::RevertStmt(TableRef table, std::string timestamp)
        : Table(std::move(table)), Timestamp(std::move(timestamp)) {}

CreateTableStmt::CreateTableStmt(TableRef table, std::vector<ColumnDef> columns)
        : Table(std::move(table)), Columns(std::move(columns)) {}

DropTableStmt::DropTableStmt(TableRef table)
        : Table(std::move(table)) {}

InsertStmt::InsertStmt(TableRef table, std::vector<std::string> columns,
                       std::vector<std::vector<std::unique_ptr<Literal>>> values)
        : Table(std::move(table)), Columns(std::move(columns)), Values(std::move(values)) {}

UpdateStmt::UpdateStmt(TableRef table,
                       std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assigns,
                       std::unique_ptr<Condition> where)
        : Table(std::move(table)), Assignments(std::move(assigns)), WhereClause(std::move(where)) {}

DeleteStmt::DeleteStmt(TableRef table, std::unique_ptr<Condition> where)
        : Table(std::move(table)), WhereClause(std::move(where)) {}

SelectStmt::SelectStmt(bool all, std::vector<SelectItem> items, TableRef table,
                       std::unique_ptr<Condition> where)
        : SelectAll(all), SelectItems(std::move(items)), Table(std::move(table)), WhereClause(std::move(where)) {}

}  // namespace qdb::server

