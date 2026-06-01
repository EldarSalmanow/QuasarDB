#ifndef QUASARDB_AST_H
#define QUASARDB_AST_H

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>

namespace qdb::server {

// Forward declarations
struct ASTNode;
struct Statement;
struct Expression;
struct Condition;

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

struct Expression : ASTNode {
    enum class Kind { Identifier, Literal, Aggregate };

    ~Expression() override;

    virtual auto KindOf() const -> Kind = 0;
};

struct IdentifierExpr : Expression {
    std::string Name;

    explicit IdentifierExpr(std::string name);

    auto KindOf() const -> Kind override;
};

struct LiteralExpr : Expression {
    std::unique_ptr<Literal> LiteralValue;

    explicit LiteralExpr(std::unique_ptr<Literal> literal);

    auto KindOf() const -> Kind override;
};

struct AggregateExpr : Expression {
    enum class Function { Sum, Count, Avg };

    Function FunctionType;
    std::string Column;

    AggregateExpr(Function function, std::string column);

    auto KindOf() const -> Kind override;
};

struct Condition : ASTNode {
    enum class Kind { Comparison, Between, Like, And, Or };

    ~Condition() override;

    virtual auto KindOf() const -> Kind = 0;
};

struct ComparisonCondition : Condition {
    enum class Operator { Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual };

    std::unique_ptr<Expression> Left;
    Operator Op;
    std::unique_ptr<Expression> Right;

    ComparisonCondition(std::unique_ptr<Expression> left, Operator op, std::unique_ptr<Expression> right);

    auto KindOf() const -> Kind override;
};

struct BetweenCondition : Condition {
    std::unique_ptr<Expression> Value;
    std::unique_ptr<Expression> Lower;
    std::unique_ptr<Expression> Upper;

    BetweenCondition(
        std::unique_ptr<Expression> value,
        std::unique_ptr<Expression> lower,
        std::unique_ptr<Expression> upper
    );

    auto KindOf() const -> Kind override;
};

struct LikeCondition : Condition {
    std::unique_ptr<Expression> Value;
    std::string Pattern;

    LikeCondition(std::unique_ptr<Expression> value, std::string pattern);

    auto KindOf() const -> Kind override;
};

struct AndCondition : Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    AndCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right);

    auto KindOf() const -> Kind override;
};

struct OrCondition : Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    OrCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right);

    auto KindOf() const -> Kind override;
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

struct Statement : ASTNode {
    enum class Kind {
        CreateDatabase,
        DropDatabase,
        UseDatabase,
        CreateUser,
        Grant,
        Revoke,
        Revert,
        CreateTable,
        DropTable,
        Insert,
        Update,
        Delete,
        Select
    };

    ~Statement() override;

    virtual auto KindOf() const -> Kind = 0;
};

// Database statements
struct CreateDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit CreateDatabaseStmt(std::string name);

    auto KindOf() const -> Kind override;
};

struct DropDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit DropDatabaseStmt(std::string name);

    auto KindOf() const -> Kind override;
};

struct UseDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit UseDatabaseStmt(std::string name);

    auto KindOf() const -> Kind override;
};

struct CreateUserStmt : Statement {
    std::string Username;
    std::string Password;

    CreateUserStmt(std::string username, std::string password);

    auto KindOf() const -> Kind override;
};

struct GrantStmt : Statement {
    std::vector<std::string> Permissions;
    TableRef Scope;
    std::string Username;

    GrantStmt(std::vector<std::string> permissions, TableRef scope, std::string username);

    auto KindOf() const -> Kind override;
};

struct RevokeStmt : Statement {
    std::vector<std::string> Permissions;
    TableRef Scope;
    std::string Username;

    RevokeStmt(std::vector<std::string> permissions, TableRef scope, std::string username);

    auto KindOf() const -> Kind override;
};

struct RevertStmt : Statement {
    TableRef Table;
    std::string Timestamp;

    RevertStmt(TableRef table, std::string timestamp);

    auto KindOf() const -> Kind override;
};

// DDL statements
struct CreateTableStmt : Statement {
    TableRef Table;
    std::vector<ColumnDef> Columns;

    CreateTableStmt(TableRef table, std::vector<ColumnDef> columns);

    auto KindOf() const -> Kind override;
};

struct DropTableStmt : Statement {
    TableRef Table;

    explicit DropTableStmt(TableRef table);

    auto KindOf() const -> Kind override;
};

// DML statements
struct InsertStmt : Statement {
    TableRef Table;
    std::vector<std::string> Columns;
    std::vector<std::vector<std::unique_ptr<Literal>>> Values;

    InsertStmt(
        TableRef table,
        std::vector<std::string> columns,
        std::vector<std::vector<std::unique_ptr<Literal>>> values
    );

    auto KindOf() const -> Kind override;
};

struct UpdateStmt : Statement {
    TableRef Table;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> Assignments;
    std::unique_ptr<Condition> WhereClause;

    UpdateStmt(
        TableRef table,
        std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assigns,
        std::unique_ptr<Condition> where
    );

    auto KindOf() const -> Kind override;
};

struct DeleteStmt : Statement {
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    DeleteStmt(TableRef table, std::unique_ptr<Condition> where);

    auto KindOf() const -> Kind override;
};

struct SelectStmt : Statement {
    bool SelectAll = false;
    std::vector<SelectItem> SelectItems;
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    SelectStmt(bool all, std::vector<SelectItem> items, TableRef table, std::unique_ptr<Condition> where = nullptr);

    auto KindOf() const -> Kind override;
};

auto SerializeAst(const Statement& statement) -> nlohmann::json;

auto DeserializeAst(const nlohmann::json& json) -> std::unique_ptr<Statement>;

}  // namespace qdb::server

#endif  // QUASARDB_AST_H
