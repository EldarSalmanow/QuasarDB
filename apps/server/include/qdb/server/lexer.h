#ifndef QUASARDB_LEXER_H
#define QUASARDB_LEXER_H

#include <cstddef>
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

    Token(TokenType type, std::string value, int line, int column);
};

class Lexer {
public:
    explicit Lexer(std::string_view source);

public:
    auto NextToken() -> Token;

    auto HasNext() const -> bool;

    auto Tokenize() -> std::vector<Token>;

private:
    static auto GetKeywords() -> const std::unordered_set<std::string>&;

    static auto IsDigit(char c) -> bool;

    static auto IsAlpha(char c) -> bool;

    static auto IsOperatorChar(char c) -> bool;

    static auto IsPunctuationChar(char c) -> bool;

    auto IsAtEnd() const -> bool;

    auto Peek() const -> char;

    auto Advance() -> char;

    auto SkipWhitespace() -> void;

    auto MakeToken(TokenType type, std::string value) const -> Token;

    auto ScanStringLiteral() -> Token;

    auto ScanNumber() -> Token;

    auto ScanIdentifierOrKeyword() -> Token;

    auto ScanOperator() -> Token;

    auto ScanPunctuation() -> Token;

private:
    std::string_view source_;

    std::size_t pos_{0};

    int line_{1};

    int column_{1};
};

}  // namespace qdb::server

#endif  // QUASARDB_LEXER_H
