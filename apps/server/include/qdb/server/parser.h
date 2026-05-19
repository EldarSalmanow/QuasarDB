#ifndef QUASARDB_PARSER_H
#define QUASARDB_PARSER_H

#include <qdb/server/ast.h>
#include <qdb/server/lexer.h>

#include <stdexcept>
#include <string>
#include <vector>
#include <memory>


namespace qdb::server {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& message);
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

public:
    auto ParseStatement() -> std::unique_ptr<Statement>;

private:
    auto IsAtEnd() const -> bool;

    auto Peek() const -> Token;

    auto Advance() -> Token;

    auto Match(TokenType type, const std::string& value = "") -> bool;

    auto Expect(TokenType type, const std::string& message) -> void;

    auto Expect(TokenType type, const std::string& value, const std::string& message) -> void;

    auto ExpectPunctuation(const std::string& punctuation) -> void;

    auto ExpectOperator(const std::string& op) -> void;

    auto ExpectKeyword(const std::string& keyword) -> void;

    auto ParseIdentifier() -> std::string;

    auto ParseTableRef() -> TableRef;

    auto ParseTimestampPart(const std::string& name, std::size_t expected_length) -> std::string;

    auto ParseTimestamp() -> std::string;

    auto ParseLiteral() -> std::unique_ptr<Literal>;

    auto ParseValue() -> std::unique_ptr<Expression>;

    auto ParseAggregateFunction(const std::string& func) -> AggregateExpr::Function;

    auto ParseExpression() -> std::unique_ptr<Expression>;

    auto ParseCondition() -> std::unique_ptr<Condition>;

    auto ParseOrCondition() -> std::unique_ptr<Condition>;

    auto ParseAndCondition() -> std::unique_ptr<Condition>;

    auto ParseComparisonOperator(const std::string& op) -> ComparisonCondition::Operator;

    auto ParsePredicate() -> std::unique_ptr<Condition>;

    auto ParseColumnDefinition() -> ColumnDef;

    auto ParseCreateStatement() -> std::unique_ptr<Statement>;

    auto ParseDropStatement() -> std::unique_ptr<Statement>;

    auto ParseUseStatement() -> std::unique_ptr<Statement>;

    auto ParseRevertStatement() -> std::unique_ptr<Statement>;

    auto ParseInsertStatement() -> std::unique_ptr<Statement>;

    auto ParseUpdateStatement() -> std::unique_ptr<Statement>;

    auto ParseDeleteStatement() -> std::unique_ptr<Statement>;

    auto ParseSelectStatement() -> std::unique_ptr<Statement>;

private:
    std::vector<Token> tokens_;

    std::size_t pos_{0};
};

}  // namespace qdb::server

#endif  // QUASARDB_PARSER_H
