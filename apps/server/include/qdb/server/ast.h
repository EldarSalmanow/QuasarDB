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
    virtual ~ASTNode();
};

// Literals
struct Literal : ASTNode {
    enum class Type { String, Integer, Null };

    Type LiteralType;
    std::string Value;

    Literal(Type type, std::string value);
};

// Expressions
struct Expression : ASTNode {
    ~Expression() override;
};

struct IdentifierExpr : Expression {
    std::string Name;

    explicit IdentifierExpr(std::string name);
};

struct LiteralExpr : Expression {
    std::unique_ptr<Literal> LiteralValue;

    explicit LiteralExpr(std::unique_ptr<Literal> literal);
};

struct AggregateExpr : Expression {
    enum class Function { Sum, Count, Avg };

    Function FunctionType;
    std::string Column;

    AggregateExpr(Function function, std::string column);
};

// Conditions
struct Condition : ASTNode {
    ~Condition() override;
};

struct ComparisonCondition : Condition {
    enum class Operator { Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual };

    std::unique_ptr<Expression> Left;
    Operator Op;
    std::unique_ptr<Expression> Right;

    ComparisonCondition(std::unique_ptr<Expression> left, Operator op, std::unique_ptr<Expression> right);
};

struct BetweenCondition : Condition {
    std::unique_ptr<Expression> Value;
    std::unique_ptr<Expression> Lower;
    std::unique_ptr<Expression> Upper;

    BetweenCondition(std::unique_ptr<Expression> value, std::unique_ptr<Expression> lower,
                     std::unique_ptr<Expression> upper);
};

struct LikeCondition : Condition {
    std::unique_ptr<Expression> Value;
    std::string Pattern;

    LikeCondition(std::unique_ptr<Expression> value, std::string pattern);
};

struct AndCondition : Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    AndCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right);
};

struct OrCondition : Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    OrCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right);
};

// Table reference
struct TableRef {
    std::string Database;
    std::string Table;

    explicit TableRef(std::string table);

    TableRef(std::string database, std::string table);
};

// Column definition
struct ColumnDef {
    enum class Type { Int, String };

    std::string Name;
    Type ColumnType;
    bool NotNull = false;
    bool Indexed = false;
    std::unique_ptr<Literal> DefaultValue;

    ColumnDef(std::string name, Type type);
};

// Select item
struct SelectItem {
    std::unique_ptr<Expression> Expr;
    std::string Alias;

    explicit SelectItem(std::unique_ptr<Expression> expr, std::string alias = "");
};

// Statements
struct Statement : ASTNode {
    ~Statement() override;
};

// Database statements
struct CreateDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit CreateDatabaseStmt(std::string name);
};

struct DropDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit DropDatabaseStmt(std::string name);
};

struct UseDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit UseDatabaseStmt(std::string name);
};

struct RevertStmt : Statement {
    TableRef Table;
    std::string Timestamp;

    RevertStmt(TableRef table, std::string timestamp);
};

// DDL statements
struct CreateTableStmt : Statement {
    TableRef Table;
    std::vector<ColumnDef> Columns;

    CreateTableStmt(TableRef table, std::vector<ColumnDef> columns);
};

struct DropTableStmt : Statement {
    TableRef Table;

    explicit DropTableStmt(TableRef table);
};

// DML statements
struct InsertStmt : Statement {
    TableRef Table;
    std::vector<std::string> Columns;
    std::vector<std::vector<std::unique_ptr<Literal>>> Values;

    InsertStmt(TableRef table, std::vector<std::string> columns,
               std::vector<std::vector<std::unique_ptr<Literal>>> values);
};

struct UpdateStmt : Statement {
    TableRef Table;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> Assignments;
    std::unique_ptr<Condition> WhereClause;

    UpdateStmt(TableRef table, std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assigns,
               std::unique_ptr<Condition> where);
};

struct DeleteStmt : Statement {
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    DeleteStmt(TableRef table, std::unique_ptr<Condition> where);
};

struct SelectStmt : Statement {
    bool SelectAll = false;
    std::vector<SelectItem> SelectItems;
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    SelectStmt(bool all, std::vector<SelectItem> items, TableRef table,
               std::unique_ptr<Condition> where = nullptr);
};

}  // namespace qdb::server

#endif  // QUASARDB_AST_H
