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
struct AstVisitor;

// Base AST node
struct ASTNode {
    virtual ~ASTNode();

    virtual auto Accept(AstVisitor& visitor) const -> void = 0;
};

// Literals
struct Literal : ASTNode {
    enum class Type { String, Integer, Null };

    Type LiteralType;
    std::string Value;

    Literal(Type type, std::string value);

    auto Accept(AstVisitor& visitor) const -> void override;
};

// Expressions
struct Expression : ASTNode {
    ~Expression() override;
};

struct IdentifierExpr : Expression {
    std::string Name;

    explicit IdentifierExpr(std::string name);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct LiteralExpr : Expression {
    std::unique_ptr<Literal> LiteralValue;

    explicit LiteralExpr(std::unique_ptr<Literal> literal);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct AggregateExpr : Expression {
    enum class Function { Sum, Count, Avg };

    Function FunctionType;
    std::string Column;

    AggregateExpr(Function function, std::string column);

    auto Accept(AstVisitor& visitor) const -> void override;
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

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct BetweenCondition : Condition {
    std::unique_ptr<Expression> Value;
    std::unique_ptr<Expression> Lower;
    std::unique_ptr<Expression> Upper;

    BetweenCondition(std::unique_ptr<Expression> value, std::unique_ptr<Expression> lower,
                     std::unique_ptr<Expression> upper);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct LikeCondition : Condition {
    std::unique_ptr<Expression> Value;
    std::string Pattern;

    LikeCondition(std::unique_ptr<Expression> value, std::string pattern);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct AndCondition : Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    AndCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct OrCondition : Condition {
    std::unique_ptr<Condition> Left;
    std::unique_ptr<Condition> Right;

    OrCondition(std::unique_ptr<Condition> left, std::unique_ptr<Condition> right);

    auto Accept(AstVisitor& visitor) const -> void override;
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

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct DropDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit DropDatabaseStmt(std::string name);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct UseDatabaseStmt : Statement {
    std::string DatabaseName;

    explicit UseDatabaseStmt(std::string name);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct RevertStmt : Statement {
    TableRef Table;
    std::string Timestamp;

    RevertStmt(TableRef table, std::string timestamp);

    auto Accept(AstVisitor& visitor) const -> void override;
};

// DDL statements
struct CreateTableStmt : Statement {
    TableRef Table;
    std::vector<ColumnDef> Columns;

    CreateTableStmt(TableRef table, std::vector<ColumnDef> columns);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct DropTableStmt : Statement {
    TableRef Table;

    explicit DropTableStmt(TableRef table);

    auto Accept(AstVisitor& visitor) const -> void override;
};

// DML statements
struct InsertStmt : Statement {
    TableRef Table;
    std::vector<std::string> Columns;
    std::vector<std::vector<std::unique_ptr<Literal>>> Values;

    InsertStmt(TableRef table, std::vector<std::string> columns,
               std::vector<std::vector<std::unique_ptr<Literal>>> values);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct UpdateStmt : Statement {
    TableRef Table;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> Assignments;
    std::unique_ptr<Condition> WhereClause;

    UpdateStmt(TableRef table, std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assigns,
               std::unique_ptr<Condition> where);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct DeleteStmt : Statement {
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    DeleteStmt(TableRef table, std::unique_ptr<Condition> where);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct SelectStmt : Statement {
    bool SelectAll = false;
    std::vector<SelectItem> SelectItems;
    TableRef Table;
    std::unique_ptr<Condition> WhereClause;

    SelectStmt(bool all, std::vector<SelectItem> items, TableRef table,
               std::unique_ptr<Condition> where = nullptr);

    auto Accept(AstVisitor& visitor) const -> void override;
};

struct AstVisitor {
    virtual ~AstVisitor() = default;

    virtual void Visit(const Literal& node) = 0;

    virtual void Visit(const IdentifierExpr& node) = 0;

    virtual void Visit(const LiteralExpr& node) = 0;

    virtual void Visit(const AggregateExpr& node) = 0;

    virtual void Visit(const ComparisonCondition& node) = 0;

    virtual void Visit(const BetweenCondition& node) = 0;

    virtual void Visit(const LikeCondition& node) = 0;

    virtual void Visit(const AndCondition& node) = 0;

    virtual void Visit(const OrCondition& node) = 0;

    virtual void Visit(const CreateDatabaseStmt& node) = 0;

    virtual void Visit(const DropDatabaseStmt& node) = 0;

    virtual void Visit(const UseDatabaseStmt& node) = 0;

    virtual void Visit(const RevertStmt& node) = 0;

    virtual void Visit(const CreateTableStmt& node) = 0;

    virtual void Visit(const DropTableStmt& node) = 0;

    virtual void Visit(const InsertStmt& node) = 0;

    virtual void Visit(const UpdateStmt& node) = 0;

    virtual void Visit(const DeleteStmt& node) = 0;

    virtual void Visit(const SelectStmt& node) = 0;
};

auto SerializeAst(const Statement& statement) -> nlohmann::json;

auto DeserializeAst(const nlohmann::json& json) -> std::unique_ptr<Statement>;

}  // namespace qdb::server

#endif  // QUASARDB_AST_H
