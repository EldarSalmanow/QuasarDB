#include <gtest/gtest.h>

#include "qdb/server/lexer.h"

using namespace qdb::server;

TEST(LexerTest, EmptyInput) {
    Lexer lexer("");
    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::EndOfFile);
    EXPECT_FALSE(lexer.HasNext());
}

TEST(LexerTest, WhitespaceOnly) {
    Lexer lexer("   \n\t  \n  ");
    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::EndOfFile);
    EXPECT_FALSE(lexer.HasNext());
}

TEST(LexerTest, KeywordsCaseInsensitive) {
    Lexer lexer("SELECT select SeLeCt");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::Keyword);
    EXPECT_EQ(token1.value, "SELECT");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::Keyword);
    EXPECT_EQ(token2.value, "SELECT");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::Keyword);
    EXPECT_EQ(token3.value, "SELECT");
}

TEST(LexerTest, AllKeywords) {
    std::vector<std::string> keywords = {
        "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUE", "UPDATE", "SET",
        "DELETE", "CREATE", "DROP", "TABLE", "DATABASE", "USE", "REVERT",
        "INT", "STRING", "NOT_NULL", "INDEXED", "DEFAULT", "NULL",
        "AND", "OR", "BETWEEN", "LIKE", "AS", "SUM", "COUNT", "AVG"
    };

    for (const auto& kw : keywords) {
        Lexer lexer(kw);
        Token token = lexer.NextToken();
        EXPECT_EQ(token.type, TokenType::Keyword) << "Failed for keyword: " << kw;
        EXPECT_EQ(token.value, kw);
    }
}

TEST(LexerTest, Identifiers) {
    Lexer lexer("user_id tableName _private column123");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::Identifier);
    EXPECT_EQ(token1.value, "user_id");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::Identifier);
    EXPECT_EQ(token2.value, "tableName");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::Identifier);
    EXPECT_EQ(token3.value, "_private");

    Token token4 = lexer.NextToken();
    EXPECT_EQ(token4.type, TokenType::Identifier);
    EXPECT_EQ(token4.value, "column123");
}

TEST(LexerTest, StringLiterals) {
    Lexer lexer(R"("hello" "world with spaces" "123")");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::StringLiteral);
    EXPECT_EQ(token1.value, "hello");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::StringLiteral);
    EXPECT_EQ(token2.value, "world with spaces");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::StringLiteral);
    EXPECT_EQ(token3.value, "123");
}

TEST(LexerTest, UnterminatedString) {
    Lexer lexer(R"("unterminated)");
    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::Invalid);
}

TEST(LexerTest, IntegerLiterals) {
    Lexer lexer("0 123 456789");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::IntegerLiteral);
    EXPECT_EQ(token1.value, "0");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::IntegerLiteral);
    EXPECT_EQ(token2.value, "123");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::IntegerLiteral);
    EXPECT_EQ(token3.value, "456789");
}

TEST(LexerTest, Operators) {
    Lexer lexer("= == != < > <= >=");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::Operator);
    EXPECT_EQ(token1.value, "=");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::Operator);
    EXPECT_EQ(token2.value, "==");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::Operator);
    EXPECT_EQ(token3.value, "!=");

    Token token4 = lexer.NextToken();
    EXPECT_EQ(token4.type, TokenType::Operator);
    EXPECT_EQ(token4.value, "<");

    Token token5 = lexer.NextToken();
    EXPECT_EQ(token5.type, TokenType::Operator);
    EXPECT_EQ(token5.value, ">");

    Token token6 = lexer.NextToken();
    EXPECT_EQ(token6.type, TokenType::Operator);
    EXPECT_EQ(token6.value, "<=");

    Token token7 = lexer.NextToken();
    EXPECT_EQ(token7.type, TokenType::Operator);
    EXPECT_EQ(token7.value, ">=");
}

TEST(LexerTest, Punctuation) {
    Lexer lexer("( ) , ; .");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::Punctuation);
    EXPECT_EQ(token1.value, "(");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::Punctuation);
    EXPECT_EQ(token2.value, ")");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::Punctuation);
    EXPECT_EQ(token3.value, ",");

    Token token4 = lexer.NextToken();
    EXPECT_EQ(token4.type, TokenType::Punctuation);
    EXPECT_EQ(token4.value, ";");

    Token token5 = lexer.NextToken();
    EXPECT_EQ(token5.type, TokenType::Punctuation);
    EXPECT_EQ(token5.value, ".");
}

TEST(LexerTest, ComplexSelectStatement) {
    Lexer lexer("SELECT id, name FROM users WHERE age >= 18;");

    auto tokens = lexer.Tokenize();

    ASSERT_GE(tokens.size(), 12);
    ASSERT_EQ(tokens.size(), 12);

    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "SELECT");

    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "id");

    EXPECT_EQ(tokens[2].type, TokenType::Punctuation);
    EXPECT_EQ(tokens[2].value, ",");

    EXPECT_EQ(tokens[3].type, TokenType::Identifier);
    EXPECT_EQ(tokens[3].value, "name");

    EXPECT_EQ(tokens[4].type, TokenType::Keyword);
    EXPECT_EQ(tokens[4].value, "FROM");

    EXPECT_EQ(tokens[5].type, TokenType::Identifier);
    EXPECT_EQ(tokens[5].value, "users");

    EXPECT_EQ(tokens[6].type, TokenType::Keyword);
    EXPECT_EQ(tokens[6].value, "WHERE");

    EXPECT_EQ(tokens[7].type, TokenType::Identifier);
    EXPECT_EQ(tokens[7].value, "age");

    EXPECT_EQ(tokens[8].type, TokenType::Operator);
    EXPECT_EQ(tokens[8].value, ">=");

    EXPECT_EQ(tokens[9].type, TokenType::IntegerLiteral);
    EXPECT_EQ(tokens[9].value, "18");

    EXPECT_EQ(tokens[10].type, TokenType::Punctuation);
    EXPECT_EQ(tokens[10].value, ";");

    EXPECT_EQ(tokens[11].type, TokenType::EndOfFile);
}

TEST(LexerTest, InsertStatement) {
    Lexer lexer(R"(INSERT INTO products (id, name) VALUE (1, "Apple");)");

    auto tokens = lexer.Tokenize();

    ASSERT_GE(tokens.size(), 3);

    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "INSERT");

    EXPECT_EQ(tokens[1].type, TokenType::Keyword);
    EXPECT_EQ(tokens[1].value, "INTO");

    EXPECT_EQ(tokens[2].type, TokenType::Identifier);
    EXPECT_EQ(tokens[2].value, "products");
}

TEST(LexerTest, CreateTableStatement) {
    Lexer lexer("CREATE TABLE users (id INT NOT_NULL INDEXED, name STRING);");

    auto tokens = lexer.Tokenize();

    ASSERT_GE(tokens.size(), 8);

    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "CREATE");

    EXPECT_EQ(tokens[1].type, TokenType::Keyword);
    EXPECT_EQ(tokens[1].value, "TABLE");

    EXPECT_EQ(tokens[6].type, TokenType::Keyword);
    EXPECT_EQ(tokens[6].value, "NOT_NULL");

    EXPECT_EQ(tokens[7].type, TokenType::Keyword);
    EXPECT_EQ(tokens[7].value, "INDEXED");
}

TEST(LexerTest, QualifiedTableReference) {
    Lexer lexer("shop.products");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::Identifier);
    EXPECT_EQ(token1.value, "shop");

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::Punctuation);
    EXPECT_EQ(token2.value, ".");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::Identifier);
    EXPECT_EQ(token3.value, "products");
}

TEST(LexerTest, AggregateFunction) {
    Lexer lexer("SELECT COUNT(id), AVG(price), SUM(total) FROM orders;");

    auto tokens = lexer.Tokenize();

    ASSERT_GE(tokens.size(), 12);

    EXPECT_EQ(tokens[1].type, TokenType::Keyword);
    EXPECT_EQ(tokens[1].value, "COUNT");

    EXPECT_EQ(tokens[6].type, TokenType::Keyword);
    EXPECT_EQ(tokens[6].value, "AVG");

    EXPECT_EQ(tokens[11].type, TokenType::Keyword);
    EXPECT_EQ(tokens[11].value, "SUM");
}

TEST(LexerTest, BetweenPredicate) {
    Lexer lexer("WHERE price BETWEEN 10 AND 100");

    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[2].type, TokenType::Keyword);
    EXPECT_EQ(tokens[2].value, "BETWEEN");

    EXPECT_EQ(tokens[4].type, TokenType::Keyword);
    EXPECT_EQ(tokens[4].value, "AND");
}

TEST(LexerTest, LikePredicate) {
    Lexer lexer(R"(WHERE name LIKE "^A.*")");

    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[2].type, TokenType::Keyword);
    EXPECT_EQ(tokens[2].value, "LIKE");

    EXPECT_EQ(tokens[3].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[3].value, "^A.*");
}

TEST(LexerTest, RevertStatement) {
    Lexer lexer("REVERT products 2026.05.02-14:30:45.123;");

    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "REVERT");

    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "products");

    EXPECT_EQ(tokens[2].type, TokenType::IntegerLiteral);
    EXPECT_EQ(tokens[2].value, "2026");

    EXPECT_EQ(tokens[3].type, TokenType::Punctuation);
    EXPECT_EQ(tokens[3].value, ".");
    EXPECT_EQ(tokens[7].type, TokenType::Punctuation);
    EXPECT_EQ(tokens[7].value, "-");
    EXPECT_EQ(tokens[9].type, TokenType::Punctuation);
    EXPECT_EQ(tokens[9].value, ":");
    EXPECT_EQ(tokens[11].type, TokenType::Punctuation);
    EXPECT_EQ(tokens[11].value, ":");
}

TEST(LexerTest, MultilineQuery) {
    Lexer lexer(R"(
        SELECT *
        FROM shop.products
        WHERE (price >= 1 AND price <= 50)
           OR id == 99;
    )");

    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "SELECT");

    EXPECT_GT(tokens.size(), 10);
    EXPECT_EQ(tokens.back().type, TokenType::EndOfFile);
}

TEST(LexerTest, LineAndColumnTracking) {
    Lexer lexer("SELECT\nFROM");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.line, 1);
    EXPECT_EQ(token1.column, 1);

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.line, 2);
    EXPECT_EQ(token2.column, 1);
}

TEST(LexerTest, InvalidCharacter) {
    Lexer lexer("SELECT @ FROM");

    Token token1 = lexer.NextToken();
    EXPECT_EQ(token1.type, TokenType::Keyword);

    Token token2 = lexer.NextToken();
    EXPECT_EQ(token2.type, TokenType::Invalid);
    EXPECT_EQ(token2.value, "@");

    Token token3 = lexer.NextToken();
    EXPECT_EQ(token3.type, TokenType::Keyword);
}

TEST(LexerTest, HasNextMethod) {
    Lexer lexer("SELECT FROM");

    EXPECT_TRUE(lexer.HasNext());
    lexer.NextToken();

    EXPECT_TRUE(lexer.HasNext());
    lexer.NextToken();

    EXPECT_FALSE(lexer.HasNext());
}

TEST(LexerTest, EmptyStringLiteral) {
    Lexer lexer(R"("")");

    Token token = lexer.NextToken();
    EXPECT_EQ(token.type, TokenType::StringLiteral);
    EXPECT_EQ(token.value, "");
}

TEST(LexerTest, ConsecutiveOperators) {
    Lexer lexer("a==b!=c<d>e<=f>=g");

    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[1].type, TokenType::Operator);
    EXPECT_EQ(tokens[1].value, "==");

    EXPECT_EQ(tokens[3].type, TokenType::Operator);
    EXPECT_EQ(tokens[3].value, "!=");

    EXPECT_EQ(tokens[5].type, TokenType::Operator);
    EXPECT_EQ(tokens[5].value, "<");

    EXPECT_EQ(tokens[7].type, TokenType::Operator);
    EXPECT_EQ(tokens[7].value, ">");

    EXPECT_EQ(tokens[9].type, TokenType::Operator);
    EXPECT_EQ(tokens[9].value, "<=");

    EXPECT_EQ(tokens[11].type, TokenType::Operator);
    EXPECT_EQ(tokens[11].value, ">=");
}

TEST(LexerTest, StarOperator) {
    Lexer lexer("SELECT * FROM users");

    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "SELECT");

    EXPECT_EQ(tokens[1].type, TokenType::Operator);
    EXPECT_EQ(tokens[1].value, "*");

    EXPECT_EQ(tokens[2].type, TokenType::Keyword);
    EXPECT_EQ(tokens[2].value, "FROM");
}
