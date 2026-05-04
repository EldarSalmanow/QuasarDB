//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_AST_H
#define QUASARDB_AST_H

#include <memory>
#include <string>
#include <vector>

namespace qdb::server {

// Forward declarations
struct ASTNode;
struct Statement;
struct Expression;
struct Condition;

// Base AST node
struct ASTNode {
    virtual ~ASTNode() = default;
};

// Literals
struct Literal : public ASTNode {
    enum class Type { String, Integer, Null };
    Type LiteralType;
    std::string Value;

    Literal(Type t, std::string v) : LiteralType(t), Value(std::move(v)) {}
};

// Expressions
struct Expression : public ASTNode {
    virtual ~Expression() = default;
};

struct IdentifierExpr : public Expression {
    std::string Name;
    explicit IdentifierExpr(std::string n) : Name(std::move(n)) {}
};

struct LiteralExpr : public Expression {
    std::unique_ptr<Literal> LiteralValue;
    explicit LiteralExpr(std::unique_ptr<Literal> lit) : LiteralValue(std::move(lit)) {}
};

struct AggregateExpr : public Expression {
    enum class Function { Sum, Count, Avg };
    Function FunctionType;
    std::string Column;
    std::string Alias;

    AggregateExpr(Function f, std::string col, std::string a = "")
        : FunctionType(f), Column(std::move(col)), Alias(std::move(a)) {}
};

// Conditions
struct Condition : public ASTNode {
    virtual ~Condition() = default;
};

struct ComparisonCondition : public Condition {
    enum class Operator { Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual };
    std::unique_ptr<Expression> Left;
    Operator Op;
    std::unique_ptr<Expression> Right;

    ComparisonCondition(std::unique_ptr<Expression> l, Operator o, std::unique_ptr<Expression> r)
        : Left(std::move(l)), Op(o), Right(std::move(r)) {}
};

struct BetweenCondition : public Condition {
    std::unique_ptr<Expression> Value;
    std::unique_ptr<Expression> Lower;
    std::unique_ptr<Expression> Upper;

    BetweenCondition(std::unique_ptr<Expression> v, std::unique_ptr<Expression> l,
                     std::unique_ptr<Expression> u)
        : Value(std::move(v)), Lower(std::move(l)), Upper(std::move(u)) {}
};

struct LikeCondition : public Condition {
    std::unique_ptr<Expression> Value;
    std::string Pattern;

    LikeCondition(std::unique_ptr<Expression> v, std::string p)
        : Value(std::move(v)), Pattern(std::move(p)) {}
};

struct AndCondition : public Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    AndCondition(std::unique_ptr<Condition> l, std::unique_ptr<Condition> r)
        : Left(std::move(l)), Right(std::move(r)) {}
};

struct OrCondition : public Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    OrCondition(std::unique_ptr<Condition> l, std::unique_ptr<Condition> r)
        : Left(std::move(l)), Right(std::move(r)) {}
};

// Table reference
struct TableRef {
    std::string Database;
    std::string Table;

    explicit TableRef(std::string t) : Table(std::move(t)) {}
    TableRef(std::string db, std::string t) : Database(std::move(db)), Table(std::move(t)) {}
};

// Column definition
struct ColumnDef {
    enum class Type { Int, String };
    std::string Name;
    Type ColumnType;
    bool NotNull = false;
    bool Indexed = false;
    std::unique_ptr<Literal> DefaultValue;

    ColumnDef(std::string n, Type t) : Name(std::move(n)), ColumnType(t) {}
};

// Select item
struct SelectItem {
    std::unique_ptr<Expression> Expr;
    std::string Alias;

    explicit SelectItem(std::unique_ptr<Expression> expr, std::string a = "")
        : Expr(std::move(expr)), Alias(std::move(a)) {}
};

// Statements
struct Statement : public ASTNode {
    virtual ~Statement() = default;
};

// Database statements
struct CreateDatabaseStmt : public Statement {
    std::string DatabaseName;
    explicit CreateDatabaseStmt(std::string name) : DatabaseName(std::move(name)) {}
};

struct DropDatabaseStmt : public Statement {
    std::string DatabaseName;
    explicit DropDatabaseStmt(std::string name) : DatabaseName(std::move(name)) {}
};

struct UseDatabaseStmt : public Statement {
    std::string DatabaseName;
    explicit UseDatabaseStmt(std::string name) : DatabaseName(std::move(name)) {}
};

struct RevertStmt : public Statement {
    TableRef Table;
    std::string Timestamp;

    RevertStmt(TableRef t, std::string ts) : Table(std::move(t)), Timestamp(std::move(ts)) {}
};

// DDL statements
struct CreateTableStmt : public Statement {
    TableRef Table;
    std::vector<ColumnDef> Columns;

    CreateTableStmt(TableRef t, std::vector<ColumnDef> cols)
        : Table(std::move(t)), Columns(std::move(cols)) {}
};

struct DropTableStmt : public Statement {
    TableRef Table;
    explicit DropTableStmt(TableRef t) : Table(std::move(t)) {}
};

// DML statements
struct InsertStmt : public Statement {
    TableRef Table;
    std::vector<std::string> Columns;
    std::vector<std::vector<std::unique_ptr<Literal>>> Values;

    InsertStmt(TableRef t, std::vector<std::string> cols,
               std::vector<std::vector<std::unique_ptr<Literal>>> vals)
        : Table(std::move(t)), Columns(std::move(cols)), Values(std::move(vals)) {}
};

struct UpdateStmt : public Statement {
    TableRef Table;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> Assignments;
    std::unique_ptr<Condition> WhereClause;

    UpdateStmt(TableRef t, std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assigns,
               std::unique_ptr<Condition> where)
        : Table(std::move(t)), Assignments(std::move(assigns)), WhereClause(std::move(where)) {}
};

struct DeleteStmt : public Statement {
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    DeleteStmt(TableRef t, std::unique_ptr<Condition> where)
        : Table(std::move(t)), WhereClause(std::move(where)) {}
};

struct SelectStmt : public Statement {
    bool SelectAll = false;
    std::vector<SelectItem> SelectItems;
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    SelectStmt(bool all, std::vector<SelectItem> items, TableRef t,
               std::unique_ptr<Condition> where = nullptr)
        : SelectAll(all), SelectItems(std::move(items)), Table(std::move(t)),
          WhereClause(std::move(where)) {}
};

}  // namespace qdb::server

#endif  // QUASARDB_AST_H
