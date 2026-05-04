//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_PARSER_H
#define QUASARDB_PARSER_H

#include <stdexcept>
#include <string>
#include <vector>

#include "qdb/server/ast.h"
#include "qdb/server/lexer.h"

namespace qdb::server {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), pos_(0) {}

    std::unique_ptr<Statement> ParseStatement() {
        if (IsAtEnd()) {
            throw ParseError("Unexpected end of input");
        }

        Token current = Peek();
        if (current.type != TokenType::Keyword) {
            throw ParseError("Expected statement, got: " + current.value);
        }

        const std::string& keyword = current.value;
        if (keyword == "CREATE") return ParseCreateStatement();
        if (keyword == "DROP") return ParseDropStatement();
        if (keyword == "USE") return ParseUseStatement();
        if (keyword == "REVERT") return ParseRevertStatement();
        if (keyword == "INSERT") return ParseInsertStatement();
        if (keyword == "UPDATE") return ParseUpdateStatement();
        if (keyword == "DELETE") return ParseDeleteStatement();
        if (keyword == "SELECT") return ParseSelectStatement();

        throw ParseError("Unknown statement: " + keyword);
    }

private:
    std::vector<Token> tokens_;
    size_t pos_;

    bool IsAtEnd() const { return pos_ >= tokens_.size() || Peek().type == TokenType::EndOfFile; }

    Token Peek() const {
        if (pos_ >= tokens_.size()) {
            return Token(TokenType::EndOfFile, "", 0, 0);
        }
        return tokens_[pos_];
    }

    Token Advance() {
        if (!IsAtEnd()) {
            return tokens_[pos_++];
        }
        return Token(TokenType::EndOfFile, "", 0, 0);
    }

    bool Match(TokenType type, const std::string& value = "") {
        if (IsAtEnd()) return false;
        Token current = Peek();
        if (current.type != type) return false;
        if (!value.empty() && current.value != value) return false;
        return true;
    }

    void Expect(TokenType type, const std::string& message) {
        if (IsAtEnd() || Peek().type != type) {
            throw ParseError(message);
        }
        Advance();
    }

    void ExpectKeyword(const std::string& keyword) {
        if (!Match(TokenType::Keyword, keyword)) {
            throw ParseError("Expected keyword '" + keyword + "', got: " + Peek().value);
        }
        Advance();
    }

    std::string ParseIdentifier() {
        if (!Match(TokenType::Identifier)) {
            throw ParseError("Expected identifier, got: " + Peek().value);
        }
        return Advance().value;
    }

    TableRef ParseTableRef() {
        std::string first = ParseIdentifier();

        if (Match(TokenType::Punctuation, ".")) {
            Advance();
            std::string second = ParseIdentifier();
            return TableRef(first, second);
        }

        return TableRef(first);
    }

    std::unique_ptr<Literal> ParseLiteral() {
        Token current = Peek();

        if (current.type == TokenType::StringLiteral) {
            Advance();
            return std::make_unique<Literal>(Literal::Type::String, current.value);
        } else if (current.type == TokenType::IntegerLiteral) {
            Advance();
            return std::make_unique<Literal>(Literal::Type::Integer, current.value);
        } else if (Match(TokenType::Keyword, "NULL")) {
            Advance();
            return std::make_unique<Literal>(Literal::Type::Null, "NULL");
        }

        throw ParseError("Expected literal, got: " + current.value);
    }

    std::unique_ptr<Expression> ParseValue() {
        Token current = Peek();

        if (current.type == TokenType::Identifier) {
            return std::make_unique<IdentifierExpr>(Advance().value);
        }

        return std::make_unique<LiteralExpr>(ParseLiteral());
    }

    AggregateExpr::Function ParseAggregateFunction(const std::string& func) {
        if (func == "SUM") return AggregateExpr::Function::Sum;
        if (func == "COUNT") return AggregateExpr::Function::Count;
        if (func == "AVG") return AggregateExpr::Function::Avg;
        throw ParseError("Unknown aggregate function: " + func);
    }

    std::unique_ptr<Expression> ParseExpression() {
        if (Match(TokenType::Keyword, "SUM") || Match(TokenType::Keyword, "COUNT") ||
            Match(TokenType::Keyword, "AVG")) {
            std::string func = Advance().value;
            Expect(TokenType::Punctuation, "Expected '(' after aggregate function");
            std::string column = ParseIdentifier();
            Expect(TokenType::Punctuation, "Expected ')' after column name");
            return std::make_unique<AggregateExpr>(ParseAggregateFunction(func), column);
        }

        return ParseValue();
    }

    std::unique_ptr<Condition> ParseCondition() { return ParseOrCondition(); }

    std::unique_ptr<Condition> ParseOrCondition() {
        auto left = ParseAndCondition();

        while (Match(TokenType::Keyword, "OR")) {
            Advance();
            auto right = ParseAndCondition();
            left = std::make_unique<OrCondition>(std::move(left), std::move(right));
        }

        return left;
    }

    std::unique_ptr<Condition> ParseAndCondition() {
        auto left = ParsePredicate();

        while (Match(TokenType::Keyword, "AND")) {
            Advance();
            auto right = ParsePredicate();
            left = std::make_unique<AndCondition>(std::move(left), std::move(right));
        }

        return left;
    }

    ComparisonCondition::Operator ParseComparisonOperator(const std::string& op) {
        if (op == "==") return ComparisonCondition::Operator::Equal;
        if (op == "!=") return ComparisonCondition::Operator::NotEqual;
        if (op == "<") return ComparisonCondition::Operator::Less;
        if (op == ">") return ComparisonCondition::Operator::Greater;
        if (op == "<=") return ComparisonCondition::Operator::LessEqual;
        if (op == ">=") return ComparisonCondition::Operator::GreaterEqual;
        throw ParseError("Unknown operator: " + op);
    }

    std::unique_ptr<Condition> ParsePredicate() {
        if (Match(TokenType::Punctuation, "(")) {
            Advance();
            auto condition = ParseCondition();
            Expect(TokenType::Punctuation, "Expected ')' after condition");
            return condition;
        }

        auto value = ParseValue();

        if (Match(TokenType::Keyword, "BETWEEN")) {
            Advance();
            auto lower = ParseValue();
            ExpectKeyword("AND");
            auto upper = ParseValue();
            return std::make_unique<BetweenCondition>(std::move(value), std::move(lower),
                                                      std::move(upper));
        }

        if (Match(TokenType::Keyword, "LIKE")) {
            Advance();
            if (!Match(TokenType::StringLiteral)) {
                throw ParseError("Expected string literal after LIKE");
            }
            std::string pattern = Advance().value;
            return std::make_unique<LikeCondition>(std::move(value), pattern);
        }

        if (Match(TokenType::Operator)) {
            std::string op = Advance().value;
            auto right = ParseValue();
            return std::make_unique<ComparisonCondition>(std::move(value),
                                                         ParseComparisonOperator(op),
                                                         std::move(right));
        }

        throw ParseError("Expected comparison operator, BETWEEN, or LIKE");
    }

    ColumnDef ParseColumnDefinition() {
        std::string col_name = ParseIdentifier();

        if (!Match(TokenType::Keyword, "INT") && !Match(TokenType::Keyword, "STRING")) {
            throw ParseError("Expected column type (INT or STRING)");
        }
        std::string type_str = Advance().value;
        ColumnDef::Type col_type = (type_str == "INT") ? ColumnDef::Type::Int : ColumnDef::Type::String;

        ColumnDef col_def(col_name, col_type);

        if (Match(TokenType::Keyword, "NOT_NULL")) {
            Advance();
            col_def.NotNull = true;
        }

        if (Match(TokenType::Keyword, "INDEXED")) {
            Advance();
            col_def.Indexed = true;
        }

        if (Match(TokenType::Keyword, "DEFAULT")) {
            Advance();
            col_def.DefaultValue = ParseLiteral();
        }

        return col_def;
    }

    std::unique_ptr<Statement> ParseCreateStatement() {
        ExpectKeyword("CREATE");

        if (Match(TokenType::Keyword, "DATABASE")) {
            Advance();
            std::string db_name = ParseIdentifier();
            return std::make_unique<CreateDatabaseStmt>(db_name);
        }

        if (Match(TokenType::Keyword, "TABLE")) {
            Advance();
            TableRef table = ParseTableRef();

            Expect(TokenType::Punctuation, "Expected '(' after table name");

            std::vector<ColumnDef> columns;
            do {
                if (Match(TokenType::Punctuation, ",")) {
                    Advance();
                }
                columns.push_back(ParseColumnDefinition());
            } while (Match(TokenType::Punctuation, ","));

            Expect(TokenType::Punctuation, "Expected ')' after column definitions");

            return std::make_unique<CreateTableStmt>(table, std::move(columns));
        }

        throw ParseError("Expected DATABASE or TABLE after CREATE");
    }

    std::unique_ptr<Statement> ParseDropStatement() {
        ExpectKeyword("DROP");

        if (Match(TokenType::Keyword, "DATABASE")) {
            Advance();
            std::string db_name = ParseIdentifier();
            return std::make_unique<DropDatabaseStmt>(db_name);
        }

        if (Match(TokenType::Keyword, "TABLE")) {
            Advance();
            TableRef table = ParseTableRef();
            return std::make_unique<DropTableStmt>(table);
        }

        throw ParseError("Expected DATABASE or TABLE after DROP");
    }

    std::unique_ptr<Statement> ParseUseStatement() {
        ExpectKeyword("USE");
        std::string db_name = ParseIdentifier();
        return std::make_unique<UseDatabaseStmt>(db_name);
    }

    std::unique_ptr<Statement> ParseRevertStatement() {
        ExpectKeyword("REVERT");
        TableRef table = ParseTableRef();

        std::string timestamp;
        while (!IsAtEnd() && !Match(TokenType::Punctuation, ";")) {
            timestamp += Advance().value;
        }

        if (timestamp.empty()) {
            throw ParseError("Expected timestamp after table name in REVERT");
        }

        return std::make_unique<RevertStmt>(table, timestamp);
    }

    template<typename T, typename ParseFunc>
    std::vector<T> ParseCommaSeparatedList(ParseFunc parse_func) {
        std::vector<T> items;
        do {
            if (Match(TokenType::Punctuation, ",")) {
                Advance();
            }
            items.push_back(parse_func());
        } while (Match(TokenType::Punctuation, ","));
        return items;
    }

    std::unique_ptr<Statement> ParseInsertStatement() {
        ExpectKeyword("INSERT");
        ExpectKeyword("INTO");

        TableRef table = ParseTableRef();

        Expect(TokenType::Punctuation, "Expected '(' after table name");
        auto columns = ParseCommaSeparatedList<std::string>([this]() { return ParseIdentifier(); });
        Expect(TokenType::Punctuation, "Expected ')' after column list");

        ExpectKeyword("VALUE");

        std::vector<std::vector<std::unique_ptr<Literal>>> values;
        do {
            if (Match(TokenType::Punctuation, ",")) {
                Advance();
            }

            Expect(TokenType::Punctuation, "Expected '(' before values");
            auto row = ParseCommaSeparatedList<std::unique_ptr<Literal>>([this]() { return ParseLiteral(); });
            Expect(TokenType::Punctuation, "Expected ')' after values");

            values.push_back(std::move(row));
        } while (Match(TokenType::Punctuation, ","));

        return std::make_unique<InsertStmt>(table, std::move(columns), std::move(values));
    }

    std::unique_ptr<Statement> ParseUpdateStatement() {
        ExpectKeyword("UPDATE");
        TableRef table = ParseTableRef();
        ExpectKeyword("SET");

        auto assignments = ParseCommaSeparatedList<std::pair<std::string, std::unique_ptr<Expression>>>([this]() {
            std::string column = ParseIdentifier();
            if (!Match(TokenType::Operator, "=")) {
                throw ParseError("Expected '=' in assignment");
            }
            Advance();
            auto value = ParseValue();
            return std::make_pair(column, std::move(value));
        });

        ExpectKeyword("WHERE");
        auto where = ParseCondition();

        return std::make_unique<UpdateStmt>(table, std::move(assignments), std::move(where));
    }

    std::unique_ptr<Statement> ParseDeleteStatement() {
        ExpectKeyword("DELETE");
        ExpectKeyword("FROM");

        TableRef table = ParseTableRef();

        ExpectKeyword("WHERE");
        auto where = ParseCondition();

        return std::make_unique<DeleteStmt>(table, std::move(where));
    }

    std::unique_ptr<Statement> ParseSelectStatement() {
        ExpectKeyword("SELECT");

        bool select_all = false;
        std::vector<SelectItem> items;

        if (Match(TokenType::Operator, "*")) {
            Advance();
            select_all = true;
        } else {
            items = ParseCommaSeparatedList<SelectItem>([this]() {
                auto expr = ParseExpression();
                std::string alias;
                if (Match(TokenType::Keyword, "AS")) {
                    Advance();
                    alias = ParseIdentifier();
                }
                return SelectItem(std::move(expr), alias);
            });
        }

        ExpectKeyword("FROM");
        TableRef table = ParseTableRef();

        std::unique_ptr<Condition> where;
        if (Match(TokenType::Keyword, "WHERE")) {
            Advance();
            where = ParseCondition();
        }

        return std::make_unique<SelectStmt>(select_all, std::move(items), table, std::move(where));
    }
};

}  // namespace qdb::server

#endif  // QUASARDB_PARSER_H