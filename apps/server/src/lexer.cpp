#include <qdb/server/lexer.h>

#include <cctype>
#include <utility>

namespace qdb::server {

Token::Token(TokenType token_type, std::string token_value, int token_line, int token_column)
    : type(token_type), value(std::move(token_value)), line(token_line), column(token_column) {}

Lexer::Lexer(std::string_view source) : source_(source) {}

auto Lexer::NextToken() -> Token {
    SkipWhitespace();

    if (IsAtEnd()) {
        return MakeToken(TokenType::EndOfFile, "");
    }

    const auto current = Peek();
    if (current == '"') return ScanStringLiteral();
    if (IsDigit(current)) return ScanNumber();
    if (IsAlpha(current) || current == '_') return ScanIdentifierOrKeyword();
    if (IsOperatorChar(current)) return ScanOperator();
    if (IsPunctuationChar(current)) return ScanPunctuation();

    const auto start_line = line_;
    const auto start_column = column_;
    return Token(TokenType::Invalid, std::string(1, Advance()), start_line, start_column);
}

auto Lexer::HasNext() const -> bool {
    auto pos = pos_;
    while (pos < source_.size()) {
        if (!std::isspace(static_cast<unsigned char>(source_[pos]))) {
            return true;
        }
        ++pos;
    }
    return false;
}

auto Lexer::Tokenize() -> std::vector<Token> {
    std::vector<Token> tokens;
    while (true) {
        auto token = NextToken();
        tokens.push_back(token);
        if (token.type == TokenType::EndOfFile) {
            return tokens;
        }
    }
}

auto Lexer::GetKeywords() -> const std::unordered_set<std::string>& {
    static const std::unordered_set<std::string> keywords = {
        "SELECT",  "FROM",    "WHERE",    "INSERT", "INTO",     "VALUE",   "UPDATE", "SET", "DELETE", "CREATE",
        "DROP",    "TABLE",   "DATABASE", "USER",   "PASSWORD", "USE",     "REVERT", "INT", "STRING", "NOT_NULL",
        "INDEXED", "DEFAULT", "NULL",     "AND",    "OR",       "BETWEEN", "LIKE",   "AS",  "SUM",    "COUNT",
        "AVG",     "GRANT",   "REVOKE",   "ON",     "TO",       "READ",    "WRITE"
    };
    return keywords;
}

auto Lexer::IsDigit(char c) -> bool { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

auto Lexer::IsAlpha(char c) -> bool { return std::isalpha(static_cast<unsigned char>(c)) != 0; }

auto Lexer::IsOperatorChar(char c) -> bool { return c == '=' || c == '!' || c == '<' || c == '>' || c == '*'; }

auto Lexer::IsPunctuationChar(char c) -> bool {
    return c == '(' || c == ')' || c == ',' || c == ';' || c == '.' || c == '-' || c == ':';
}

auto Lexer::IsAtEnd() const -> bool { return pos_ >= source_.size(); }

auto Lexer::Peek() const -> char { return IsAtEnd() ? '\0' : source_[pos_]; }

auto Lexer::Advance() -> char {
    const auto c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

auto Lexer::SkipWhitespace() -> void {
    while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(Peek()))) {
        Advance();
    }
}

auto Lexer::MakeToken(TokenType type, std::string value) const -> Token {
    return Token(type, std::move(value), line_, column_);
}

auto Lexer::ScanStringLiteral() -> Token {
    const auto start_line = line_;
    const auto start_column = column_;
    Advance();

    std::string value;
    while (!IsAtEnd() && Peek() != '"') {
        value += Advance();
    }

    if (IsAtEnd()) {
        return Token(TokenType::Invalid, value, start_line, start_column);
    }

    Advance();
    return Token(TokenType::StringLiteral, value, start_line, start_column);
}

auto Lexer::ScanNumber() -> Token {
    const auto start_line = line_;
    const auto start_column = column_;

    std::string value;
    while (!IsAtEnd() && IsDigit(Peek())) {
        value += Advance();
    }

    return Token(TokenType::IntegerLiteral, value, start_line, start_column);
}

auto Lexer::ScanIdentifierOrKeyword() -> Token {
    const auto start_line = line_;
    const auto start_column = column_;

    std::string value;
    while (!IsAtEnd() && (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_')) {
        value += Advance();
    }

    auto upper_value = value;
    for (auto& c : upper_value) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    if (GetKeywords().contains(upper_value)) {
        return Token(TokenType::Keyword, upper_value, start_line, start_column);
    }

    return Token(TokenType::Identifier, value, start_line, start_column);
}

auto Lexer::ScanOperator() -> Token {
    const auto start_line = line_;
    const auto start_column = column_;
    const auto first = Advance();

    if ((first == '=' || first == '!' || first == '<' || first == '>') && Peek() == '=') {
        const auto second = Advance();
        return Token(TokenType::Operator, std::string{first, second}, start_line, start_column);
    }

    return Token(TokenType::Operator, std::string(1, first), start_line, start_column);
}

auto Lexer::ScanPunctuation() -> Token {
    const auto start_line = line_;
    const auto start_column = column_;
    return Token(TokenType::Punctuation, std::string(1, Advance()), start_line, start_column);
}

}  // namespace qdb::server
