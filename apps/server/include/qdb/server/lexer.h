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

namespace qdb::server
{
    enum class TokenType
    {
        Keyword,
        Identifier,
        StringLiteral,
        IntegerLiteral,
        Operator,
        Punctuation,
        EndOfFile,
        Invalid
    };

    struct Token
    {
        TokenType type;
        std::string value;
        int line;
        int column;

        Token(TokenType type, std::string value, int line, int column)
            : type(type), value(std::move(value)), line(line), column(column)
        {
        }
    };

    class Lexer
    {
    public:
        explicit Lexer(std::string_view source)
            : source_(source), pos_(0), line_(1), column_(1)
        {
        }

        Token NextToken()
        {
            SkipWhitespace();

            if (IsAtEnd())
            {
                return MakeToken(TokenType::EndOfFile, "");
            }

            char current = Peek();

            if (current == '"')
            {
                return ScanStringLiteral();
            }

            if (IsDigit(current))
            {
                return ScanNumber();
            }

            if (IsAlpha(current) || current == '_')
            {
                return ScanIdentifierOrKeyword();
            }

            if (IsOperatorChar(current))
            {
                return ScanOperator();
            }

            if (IsPunctuationChar(current))
            {
                return ScanPunctuation();
            }

            int start_line = line_;
            int start_column = column_;
            char invalid_char = Advance();
            return Token(TokenType::Invalid, std::string(1, invalid_char), start_line, start_column);
        }

        bool HasNext() const
        {
            size_t temp_pos = pos_;

            while (temp_pos < source_.size())
            {
                if (std::isspace(static_cast<unsigned char>(source_[temp_pos])))
                {
                    temp_pos++;
                }
                else
                {
                    return true;
                }
            }
            return false;
        }

        std::vector<Token> Tokenize()
        {
            std::vector<Token> tokens;
            while (true)
            {
                Token token = NextToken();
                tokens.push_back(token);
                if (token.type == TokenType::EndOfFile)
                {
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

        static const std::unordered_set<std::string>& GetKeywords()
        {
            static const std::unordered_set<std::string> keywords = {
                "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUE", "UPDATE", "SET",
                "DELETE", "CREATE", "DROP", "TABLE", "DATABASE", "USE", "REVERT",
                "INT", "STRING", "NOT_NULL", "INDEXED", "DEFAULT", "NULL",
                "AND", "OR", "BETWEEN", "LIKE", "AS",
                "SUM", "COUNT", "AVG"
            };
            return keywords;
        }

        // Character classification helpers
        static bool IsDigit(char c)
        {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        }

        static bool IsAlpha(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) != 0;
        }

        static bool IsAlnum(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) != 0;
        }

        static bool IsOperatorChar(char c)
        {
            return c == '=' || c == '!' || c == '<' || c == '>' || c == '*';
        }

        static bool IsPunctuationChar(char c)
        {
            return c == '(' || c == ')' || c == ',' || c == ';' || c == '.';
        }

        // Position and navigation
        bool IsAtEnd() const
        {
            return pos_ >= source_.size();
        }

        char Peek() const
        {
            return IsAtEnd() ? '\0' : source_[pos_];
        }

        char PeekNext() const
        {
            return (pos_ + 1 >= source_.size()) ? '\0' : source_[pos_ + 1];
        }

        char Advance()
        {
            char c = source_[pos_++];
            if (c == '\n')
            {
                line_++;
                column_ = 1;
            }
            else
            {
                column_++;
            }
            return c;
        }

        void SkipWhitespace()
        {
            while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(Peek())))
            {
                Advance();
            }
        }

        // Token creation
        Token MakeToken(TokenType type, std::string value) const
        {
            return Token(type, std::move(value), line_, column_);
        }

        // Token scanning methods
        Token ScanStringLiteral()
        {
            int start_line = line_;
            int start_column = column_;

            Advance(); // Skip opening quote

            std::string value;
            while (!IsAtEnd() && Peek() != '"')
            {
                value += Advance();
            }

            if (IsAtEnd())
            {
                return Token(TokenType::Invalid, value, start_line, start_column);
            }

            Advance(); // Skip closing quote
            return Token(TokenType::StringLiteral, value, start_line, start_column);
        }

        Token ScanNumber()
        {
            int start_line = line_;
            int start_column = column_;

            std::string value;
            while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
            {
                value += Advance();
            }

            return Token(TokenType::IntegerLiteral, value, start_line, start_column);
        }

        Token ScanIdentifierOrKeyword()
        {
            int start_line = line_;
            int start_column = column_;

            std::string value;
            while (!IsAtEnd() && (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_'))
            {
                value += Advance();
            }

            // Convert to uppercase for keyword lookup
            std::string upper_value = value;
            for (char& c : upper_value)
            {
                c = std::toupper(static_cast<unsigned char>(c));
            }

            // Check if it's a keyword
            if (GetKeywords().count(upper_value))
            {
                return Token(TokenType::Keyword, upper_value, start_line, start_column);
            }

            return Token(TokenType::Identifier, value, start_line, start_column);
        }

        Token ScanOperator()
        {
            int start_line = line_;
            int start_column = column_;

            char first = Advance();

            // Check for two-character operators: ==, !=, <=, >=
            bool can_be_double = (first == '=' || first == '!' || first == '<' || first == '>');
            if (can_be_double && Peek() == '=')
            {
                char second = Advance();
                return Token(TokenType::Operator, std::string{first, second}, start_line, start_column);
            }

            // Single-character operator
            return Token(TokenType::Operator, std::string(1, first), start_line, start_column);
        }

        Token ScanPunctuation()
        {
            int start_line = line_;
            int start_column = column_;

            char c = Advance();
            return Token(TokenType::Punctuation, std::string(1, c), start_line, start_column);
        }
    };
} // namespace qdb::server

#endif  // QUASARDB_LEXER_H
