//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_LEXER_H
#define QUASARDB_LEXER_H

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace qdb::server {

enum class TokenType {
    Keyword,
    Identifier,
    StringLiteral,
    IntegerLiteral,
    Operator,
    Punctuation,
    EndOfFile,
    Invalid
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), column(c) {}
};

class Lexer {
public:
    explicit Lexer(std::string_view source)
        : source_(source), pos_(0), line_(1), column_(1) {
        initKeywords();
    }

    Token nextToken() {
        skipWhitespace();

        if (isAtEnd()) {
            return makeToken(TokenType::EndOfFile, "");
        }

        char current = peek();

        if (current == '"') {
            return stringLiteral();
        }

        if (std::isdigit(current)) {
            return number();
        }

        if (std::isalpha(current) || current == '_') {
            return identifierOrKeyword();
        }

        if (current == '=' || current == '!' || current == '<' || current == '>' || current == '*') {
            return operatorToken();
        }

        if (current == '(' || current == ')' || current == ',' || current == ';' || current == '.') {
            return punctuation();
        }

        return makeToken(TokenType::Invalid, std::string(1, advance()));
    }

    bool hasNext() const {
        size_t temp_pos = pos_;

        while (temp_pos < source_.size()) {
            char c = source_[temp_pos];
            if (std::isspace(c)) {
                temp_pos++;
            } else {
                return true;
            }
        }
        return false;
    }

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            Token token = nextToken();
            tokens.push_back(token);
            if (token.type == TokenType::EndOfFile) {
                break;
            }
        }
        return tokens;
    }

private:
    std::string_view source_;
    size_t pos_;
    int line_;
    int column_;
    std::unordered_set<std::string> keywords_;

    void initKeywords() {
        keywords_ = {
            "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUE", "UPDATE", "SET",
            "DELETE", "CREATE", "DROP", "TABLE", "DATABASE", "USE", "REVERT",
            "INT", "STRING", "NOT_NULL", "INDEXED", "DEFAULT", "NULL",
            "AND", "OR", "BETWEEN", "LIKE", "AS",
            "SUM", "COUNT", "AVG"
        };
    }

    bool isAtEnd() const {
        return pos_ >= source_.size();
    }

    char peek() const {
        if (isAtEnd()) return '\0';
        return source_[pos_];
    }

    char peekNext() const {
        if (pos_ + 1 >= source_.size()) return '\0';
        return source_[pos_ + 1];
    }

    char advance() {
        char c = source_[pos_++];
        if (c == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        return c;
    }

    void skipWhitespace() {
        while (!isAtEnd()) {
            char c = peek();
            if (std::isspace(c)) {
                advance();
            } else {
                break;
            }
        }
    }

    Token makeToken(TokenType type, std::string value) {
        return Token(type, std::move(value), line_, column_);
    }

    Token stringLiteral() {
        int start_line = line_;
        int start_column = column_;
        advance();

        std::string value;
        while (!isAtEnd() && peek() != '"') {
            value += advance();
        }

        if (isAtEnd()) {
            return Token(TokenType::Invalid, value, start_line, start_column);
        }

        advance();
        return Token(TokenType::StringLiteral, value, start_line, start_column);
    }

    Token number() {
        int start_line = line_;
        int start_column = column_;
        std::string value;

        while (!isAtEnd() && std::isdigit(peek())) {
            value += advance();
        }

        return Token(TokenType::IntegerLiteral, value, start_line, start_column);
    }

    Token identifierOrKeyword() {
        int start_line = line_;
        int start_column = column_;
        std::string value;

        while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
            value += advance();
        }

        std::string upper_value = value;
        for (char& c : upper_value) {
            c = std::toupper(static_cast<unsigned char>(c));
        }

        if (keywords_.count(upper_value)) {
            return Token(TokenType::Keyword, upper_value, start_line, start_column);
        }

        return Token(TokenType::Identifier, value, start_line, start_column);
    }

    Token operatorToken() {
        int start_line = line_;
        int start_column = column_;
        char first = advance();

        if ((first == '=' || first == '!' || first == '<' || first == '>') && peek() == '=') {
            char second = advance();
            return Token(TokenType::Operator, std::string{first, second}, start_line, start_column);
        }

        return Token(TokenType::Operator, std::string(1, first), start_line, start_column);
    }

    Token punctuation() {
        int start_line = line_;
        int start_column = column_;
        char c = advance();
        return Token(TokenType::Punctuation, std::string(1, c), start_line, start_column);
    }
};

}  // namespace qdb::server

#endif  // QUASARDB_LEXER_H
