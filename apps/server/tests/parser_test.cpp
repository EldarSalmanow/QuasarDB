#include <gtest/gtest.h>

#include "qdb/server/ast.h"
#include "qdb/server/lexer.h"
#include "qdb/server/parser.h"

using namespace qdb::server;

// Helper function to parse a statement from SQL string
std::unique_ptr<Statement> parseSQL(const std::string& sql) {
    Lexer lexer(sql);
    auto tokens = lexer.Tokenize();
    Parser parser(tokens);
    return parser.ParseStatement();
}

// Database statements tests
TEST(ParserTest, CreateDatabase) {
    auto stmt = parseSQL("CREATE DATABASE shop;");
    auto* create_db = dynamic_cast<CreateDatabaseStmt*>(stmt.get());
    ASSERT_NE(create_db, nullptr);
    EXPECT_EQ(create_db->DatabaseName, "shop");
}

TEST(ParserTest, DropDatabase) {
    auto stmt = parseSQL("DROP DATABASE shop;");
    auto* drop_db = dynamic_cast<DropDatabaseStmt*>(stmt.get());
    ASSERT_NE(drop_db, nullptr);
    EXPECT_EQ(drop_db->DatabaseName, "shop");
}

TEST(ParserTest, UseDatabase) {
    auto stmt = parseSQL("USE shop;");
    auto* use_db = dynamic_cast<UseDatabaseStmt*>(stmt.get());
    ASSERT_NE(use_db, nullptr);
    EXPECT_EQ(use_db->DatabaseName, "shop");
}

TEST(ParserTest, RevertStatement) {
    auto stmt = parseSQL("REVERT products 2026.05.02-14:30:45.123;");
    auto* revert = dynamic_cast<RevertStmt*>(stmt.get());
    ASSERT_NE(revert, nullptr);
    EXPECT_EQ(revert->Table.Table, "products");
    EXPECT_EQ(revert->Timestamp, "2026.05.02-14:30:45.123");
}

TEST(ParserTest, RevertStatementQualified) {
    auto stmt = parseSQL("REVERT shop.products 2026.04.28-09:15:00.000;");
    auto* revert = dynamic_cast<RevertStmt*>(stmt.get());
    ASSERT_NE(revert, nullptr);
    EXPECT_EQ(revert->Table.Database, "shop");
    EXPECT_EQ(revert->Table.Table, "products");
}

// DDL statements tests
TEST(ParserTest, CreateTableSimple) {
    auto stmt = parseSQL("CREATE TABLE users (id INT, name STRING);");
    auto* create_table = dynamic_cast<CreateTableStmt*>(stmt.get());
    ASSERT_NE(create_table, nullptr);
    EXPECT_EQ(create_table->Table.Table, "users");
    ASSERT_EQ(create_table->Columns.size(), 2);
    EXPECT_EQ(create_table->Columns[0].Name, "id");
    EXPECT_EQ(create_table->Columns[0].ColumnType, ColumnDef::Type::Int);
    EXPECT_EQ(create_table->Columns[1].Name, "name");
    EXPECT_EQ(create_table->Columns[1].ColumnType, ColumnDef::Type::String);
}

TEST(ParserTest, CreateTableWithModifiers) {
    auto stmt = parseSQL("CREATE TABLE users (id INT NOT_NULL INDEXED DEFAULT 0, name STRING NOT_NULL);");
    auto* create_table = dynamic_cast<CreateTableStmt*>(stmt.get());
    ASSERT_NE(create_table, nullptr);
    ASSERT_EQ(create_table->Columns.size(), 2);

    EXPECT_EQ(create_table->Columns[0].Name, "id");
    EXPECT_TRUE(create_table->Columns[0].NotNull);
    EXPECT_TRUE(create_table->Columns[0].Indexed);
    ASSERT_NE(create_table->Columns[0].DefaultValue, nullptr);
    EXPECT_EQ(create_table->Columns[0].DefaultValue->Value, "0");

    EXPECT_EQ(create_table->Columns[1].Name, "name");
    EXPECT_TRUE(create_table->Columns[1].NotNull);
    EXPECT_FALSE(create_table->Columns[1].Indexed);
}

TEST(ParserTest, CreateTableQualified) {
    auto stmt = parseSQL("CREATE TABLE shop.products (id INT, name STRING);");
    auto* create_table = dynamic_cast<CreateTableStmt*>(stmt.get());
    ASSERT_NE(create_table, nullptr);
    EXPECT_EQ(create_table->Table.Database, "shop");
    EXPECT_EQ(create_table->Table.Table, "products");
}

TEST(ParserTest, DropTable) {
    auto stmt = parseSQL("DROP TABLE users;");
    auto* drop_table = dynamic_cast<DropTableStmt*>(stmt.get());
    ASSERT_NE(drop_table, nullptr);
    EXPECT_EQ(drop_table->Table.Table, "users");
}

TEST(ParserTest, DropTableQualified) {
    auto stmt = parseSQL("DROP TABLE shop.products;");
    auto* drop_table = dynamic_cast<DropTableStmt*>(stmt.get());
    ASSERT_NE(drop_table, nullptr);
    EXPECT_EQ(drop_table->Table.Database, "shop");
    EXPECT_EQ(drop_table->Table.Table, "products");
}

// INSERT tests
TEST(ParserTest, InsertSingleRow) {
    auto stmt = parseSQL("INSERT INTO users (id, name) VALUE (1, \"Alice\");");
    auto* insert = dynamic_cast<InsertStmt*>(stmt.get());
    ASSERT_NE(insert, nullptr);
    EXPECT_EQ(insert->Table.Table, "users");
    ASSERT_EQ(insert->Columns.size(), 2);
    EXPECT_EQ(insert->Columns[0], "id");
    EXPECT_EQ(insert->Columns[1], "name");
    ASSERT_EQ(insert->Values.size(), 1);
    ASSERT_EQ(insert->Values[0].size(), 2);
    EXPECT_EQ(insert->Values[0][0]->Value, "1");
    EXPECT_EQ(insert->Values[0][1]->Value, "Alice");
}

TEST(ParserTest, InsertMultipleRows) {
    auto stmt = parseSQL("INSERT INTO products (id, name) VALUE (1, \"Apple\"), (2, \"Banana\");");
    auto* insert = dynamic_cast<InsertStmt*>(stmt.get());
    ASSERT_NE(insert, nullptr);
    ASSERT_EQ(insert->Values.size(), 2);
    EXPECT_EQ(insert->Values[0][0]->Value, "1");
    EXPECT_EQ(insert->Values[0][1]->Value, "Apple");
    EXPECT_EQ(insert->Values[1][0]->Value, "2");
    EXPECT_EQ(insert->Values[1][1]->Value, "Banana");
}

TEST(ParserTest, InsertWithNull) {
    auto stmt = parseSQL("INSERT INTO users (id, name) VALUE (1, NULL);");
    auto* insert = dynamic_cast<InsertStmt*>(stmt.get());
    ASSERT_NE(insert, nullptr);
    ASSERT_EQ(insert->Values[0].size(), 2);
    EXPECT_EQ(insert->Values[0][1]->LiteralType, Literal::Type::Null);
}

// UPDATE tests
TEST(ParserTest, UpdateSimple) {
    auto stmt = parseSQL("UPDATE users SET name = \"Bob\" WHERE id == 1;");
    auto* update = dynamic_cast<UpdateStmt*>(stmt.get());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->Table.Table, "users");
    ASSERT_EQ(update->Assignments.size(), 1);
    EXPECT_EQ(update->Assignments[0].first, "name");
    ASSERT_NE(update->WhereClause, nullptr);
}

TEST(ParserTest, UpdateMultipleColumns) {
    auto stmt = parseSQL("UPDATE products SET price = 99, stock = 10 WHERE id == 1;");
    auto* update = dynamic_cast<UpdateStmt*>(stmt.get());
    ASSERT_NE(update, nullptr);
    ASSERT_EQ(update->Assignments.size(), 2);
    EXPECT_EQ(update->Assignments[0].first, "price");
    EXPECT_EQ(update->Assignments[1].first, "stock");
}

TEST(ParserTest, UpdateWithComplexCondition) {
    auto stmt = parseSQL("UPDATE products SET price = 99 WHERE id == 1 AND name == \"Apple\";");
    auto* update = dynamic_cast<UpdateStmt*>(stmt.get());
    ASSERT_NE(update, nullptr);
    auto* and_cond = dynamic_cast<AndCondition*>(update->WhereClause.get());
    ASSERT_NE(and_cond, nullptr);
}

// DELETE tests
TEST(ParserTest, DeleteSimple) {
    auto stmt = parseSQL("DELETE FROM users WHERE id == 1;");
    auto* del = dynamic_cast<DeleteStmt*>(stmt.get());
    ASSERT_NE(del, nullptr);
    EXPECT_EQ(del->Table.Table, "users");
    ASSERT_NE(del->WhereClause, nullptr);
}

TEST(ParserTest, DeleteWithComplexCondition) {
    auto stmt = parseSQL("DELETE FROM products WHERE price >= 100 OR stock == 0;");
    auto* del = dynamic_cast<DeleteStmt*>(stmt.get());
    ASSERT_NE(del, nullptr);
    auto* or_cond = dynamic_cast<OrCondition*>(del->WhereClause.get());
    ASSERT_NE(or_cond, nullptr);
}

// SELECT tests
TEST(ParserTest, SelectAll) {
    auto stmt = parseSQL("SELECT * FROM users;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    EXPECT_TRUE(select->SelectAll);
    EXPECT_EQ(select->Table.Table, "users");
    EXPECT_EQ(select->WhereClause, nullptr);
}

TEST(ParserTest, KeywordsAreCaseInsensitive) {
    auto stmt = parseSQL("sElEcT * FrOm users WhErE age >= 18;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    EXPECT_TRUE(select->SelectAll);
    ASSERT_NE(select->WhereClause, nullptr);
}

TEST(ParserTest, SelectColumns) {
    auto stmt = parseSQL("SELECT id, name FROM users;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    EXPECT_FALSE(select->SelectAll);
    ASSERT_EQ(select->SelectItems.size(), 2);
}

TEST(ParserTest, SelectWithWhere) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age >= 18;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    ASSERT_NE(select->WhereClause, nullptr);
}

TEST(ParserTest, SelectWithAlias) {
    auto stmt = parseSQL("SELECT name AS user_name FROM users;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->SelectItems.size(), 1);
    EXPECT_EQ(select->SelectItems[0].Alias, "user_name");
}

TEST(ParserTest, SelectWithAggregates) {
    auto stmt = parseSQL("SELECT COUNT(id), AVG(price), SUM(total) FROM orders;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->SelectItems.size(), 3);

    auto* count_expr = dynamic_cast<AggregateExpr*>(select->SelectItems[0].Expr.get());
    ASSERT_NE(count_expr, nullptr);
    EXPECT_EQ(count_expr->FunctionType, AggregateExpr::Function::Count);

    auto* avg_expr = dynamic_cast<AggregateExpr*>(select->SelectItems[1].Expr.get());
    ASSERT_NE(avg_expr, nullptr);
    EXPECT_EQ(avg_expr->FunctionType, AggregateExpr::Function::Avg);

    auto* sum_expr = dynamic_cast<AggregateExpr*>(select->SelectItems[2].Expr.get());
    ASSERT_NE(sum_expr, nullptr);
    EXPECT_EQ(sum_expr->FunctionType, AggregateExpr::Function::Sum);
}

// Condition tests
TEST(ParserTest, ComparisonConditions) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age == 18;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* comp = dynamic_cast<ComparisonCondition*>(select->WhereClause.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->Op, ComparisonCondition::Operator::Equal);
}

TEST(ParserTest, ComparisonNotEqual) {
    auto stmt = parseSQL("SELECT * FROM users WHERE status != \"active\";");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* comp = dynamic_cast<ComparisonCondition*>(select->WhereClause.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->Op, ComparisonCondition::Operator::NotEqual);
}

TEST(ParserTest, ComparisonLess) {
    auto stmt = parseSQL("SELECT * FROM products WHERE price < 100;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* comp = dynamic_cast<ComparisonCondition*>(select->WhereClause.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->Op, ComparisonCondition::Operator::Less);
}

TEST(ParserTest, ComparisonGreater) {
    auto stmt = parseSQL("SELECT * FROM products WHERE stock > 0;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* comp = dynamic_cast<ComparisonCondition*>(select->WhereClause.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->Op, ComparisonCondition::Operator::Greater);
}

TEST(ParserTest, ComparisonLessEqual) {
    auto stmt = parseSQL("SELECT * FROM orders WHERE total <= 500;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* comp = dynamic_cast<ComparisonCondition*>(select->WhereClause.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->Op, ComparisonCondition::Operator::LessEqual);
}

TEST(ParserTest, ComparisonGreaterEqual) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age >= 18;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* comp = dynamic_cast<ComparisonCondition*>(select->WhereClause.get());
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->Op, ComparisonCondition::Operator::GreaterEqual);
}

TEST(ParserTest, BetweenCondition) {
    auto stmt = parseSQL("SELECT * FROM products WHERE price BETWEEN 10 AND 100;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* between = dynamic_cast<BetweenCondition*>(select->WhereClause.get());
    ASSERT_NE(between, nullptr);
}

TEST(ParserTest, LikeCondition) {
    auto stmt = parseSQL("SELECT * FROM users WHERE name LIKE \"^A.*\";");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* like = dynamic_cast<LikeCondition*>(select->WhereClause.get());
    ASSERT_NE(like, nullptr);
    EXPECT_EQ(like->Pattern, "^A.*");
}

TEST(ParserTest, AndCondition) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age >= 18 AND name == \"Alice\";");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* and_cond = dynamic_cast<AndCondition*>(select->WhereClause.get());
    ASSERT_NE(and_cond, nullptr);
}

TEST(ParserTest, OrCondition) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age < 18 OR age > 65;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* or_cond = dynamic_cast<OrCondition*>(select->WhereClause.get());
    ASSERT_NE(or_cond, nullptr);
}

TEST(ParserTest, ComplexConditionWithParentheses) {
    auto stmt = parseSQL("SELECT * FROM products WHERE (price >= 1 AND price <= 50) OR id == 99;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* or_cond = dynamic_cast<OrCondition*>(select->WhereClause.get());
    ASSERT_NE(or_cond, nullptr);
    auto* and_cond = dynamic_cast<AndCondition*>(or_cond->Left.get());
    ASSERT_NE(and_cond, nullptr);
}

TEST(ParserTest, NestedAndOrConditions) {
    auto stmt = parseSQL("SELECT * FROM users WHERE age >= 18 AND (name == \"Alice\" OR name == \"Bob\");");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* and_cond = dynamic_cast<AndCondition*>(select->WhereClause.get());
    ASSERT_NE(and_cond, nullptr);
    auto* or_cond = dynamic_cast<OrCondition*>(and_cond->Right.get());
    ASSERT_NE(or_cond, nullptr);
}

// Error handling tests
TEST(ParserTest, ErrorEmptyInput) {
    Lexer lexer("");
    auto tokens = lexer.Tokenize();
    Parser parser(tokens);
    EXPECT_THROW(parser.ParseStatement(), ParseError);
}

TEST(ParserTest, ErrorInvalidKeyword) { EXPECT_THROW(parseSQL("INVALID STATEMENT;"), ParseError); }

TEST(ParserTest, ErrorMissingSemicolon) {
    // Parser treats semicolon as optional; when omitted, parsing stops at EOF
    // This test ensures parser handles tokens correctly
    auto stmt = parseSQL("SELECT * FROM users");
    ASSERT_NE(stmt, nullptr);
}

TEST(ParserTest, ErrorMissingTableName) { EXPECT_THROW(parseSQL("SELECT * FROM;"), ParseError); }

TEST(ParserTest, ErrorMissingWhereCondition) { EXPECT_THROW(parseSQL("DELETE FROM users WHERE;"), ParseError); }

TEST(ParserTest, ErrorInvalidColumnType) { EXPECT_THROW(parseSQL("CREATE TABLE users (id INVALID);"), ParseError); }

TEST(ParserTest, ErrorMissingParentheses) {
    EXPECT_THROW(parseSQL("CREATE TABLE users id INT, name STRING;"), ParseError);
}

// Edge case tests
TEST(ParserTest, DeeplyNestedConditions) {
    auto stmt = parseSQL(
        "SELECT * FROM users WHERE ((age >= 18 AND status == \"active\") OR (role == \"admin\" AND verified == 1)) AND "
        "country == \"US\";"
    );
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    ASSERT_NE(select->WhereClause, nullptr);
    auto* and_cond = dynamic_cast<AndCondition*>(select->WhereClause.get());
    ASSERT_NE(and_cond, nullptr);
}

TEST(ParserTest, BetweenWithReversedBounds) {
    // Parser should accept this, semantic validation is separate
    auto stmt = parseSQL("SELECT * FROM products WHERE price BETWEEN 100 AND 10;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* between = dynamic_cast<BetweenCondition*>(select->WhereClause.get());
    ASSERT_NE(between, nullptr);
}

TEST(ParserTest, MultipleAggregatesWithDifferentColumns) {
    auto stmt = parseSQL("SELECT SUM(price), COUNT(id), AVG(rating), SUM(quantity) FROM products;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->SelectItems.size(), 4);

    auto* sum1 = dynamic_cast<AggregateExpr*>(select->SelectItems[0].Expr.get());
    ASSERT_NE(sum1, nullptr);
    EXPECT_EQ(sum1->Column, "price");

    auto* sum2 = dynamic_cast<AggregateExpr*>(select->SelectItems[3].Expr.get());
    ASSERT_NE(sum2, nullptr);
    EXPECT_EQ(sum2->Column, "quantity");
}

TEST(ParserTest, AllColumnModifiersTogether) {
    auto stmt = parseSQL("CREATE TABLE test (id INT NOT_NULL INDEXED DEFAULT 42);");
    auto* create_table = dynamic_cast<CreateTableStmt*>(stmt.get());
    ASSERT_NE(create_table, nullptr);
    ASSERT_EQ(create_table->Columns.size(), 1);

    EXPECT_TRUE(create_table->Columns[0].NotNull);
    EXPECT_TRUE(create_table->Columns[0].Indexed);
    ASSERT_NE(create_table->Columns[0].DefaultValue, nullptr);
    EXPECT_EQ(create_table->Columns[0].DefaultValue->Value, "42");
}

TEST(ParserTest, InsertWithOnlyNullValues) {
    auto stmt = parseSQL("INSERT INTO users (id, name, email) VALUE (NULL, NULL, NULL);");
    auto* insert = dynamic_cast<InsertStmt*>(stmt.get());
    ASSERT_NE(insert, nullptr);
    ASSERT_EQ(insert->Values[0].size(), 3);
    EXPECT_EQ(insert->Values[0][0]->LiteralType, Literal::Type::Null);
    EXPECT_EQ(insert->Values[0][1]->LiteralType, Literal::Type::Null);
    EXPECT_EQ(insert->Values[0][2]->LiteralType, Literal::Type::Null);
}

TEST(ParserTest, SelectWithMultipleAliases) {
    auto stmt = parseSQL("SELECT id AS user_id, name AS user_name, email AS contact FROM users;");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->SelectItems.size(), 3);
    EXPECT_EQ(select->SelectItems[0].Alias, "user_id");
    EXPECT_EQ(select->SelectItems[1].Alias, "user_name");
    EXPECT_EQ(select->SelectItems[2].Alias, "contact");
}

TEST(ParserTest, LikeWithEmptyPattern) {
    auto stmt = parseSQL("SELECT * FROM users WHERE name LIKE \"\";");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    auto* like = dynamic_cast<LikeCondition*>(select->WhereClause.get());
    ASSERT_NE(like, nullptr);
    EXPECT_EQ(like->Pattern, "");
}

TEST(ParserTest, UpdateMultipleColumnsWithComplexValues) {
    auto stmt = parseSQL("UPDATE products SET price = 99, stock = 0, status = \"sold\" WHERE id == 1;");
    auto* update = dynamic_cast<UpdateStmt*>(stmt.get());
    ASSERT_NE(update, nullptr);
    ASSERT_EQ(update->Assignments.size(), 3);
    EXPECT_EQ(update->Assignments[0].first, "price");
    EXPECT_EQ(update->Assignments[1].first, "stock");
    EXPECT_EQ(update->Assignments[2].first, "status");
}

// Additional error tests
TEST(ParserTest, ErrorInvalidOperator) { EXPECT_THROW(parseSQL("SELECT * FROM users WHERE age === 18;"), ParseError); }

TEST(ParserTest, ErrorMissingColumnType) {
    EXPECT_THROW(parseSQL("CREATE TABLE users (id, name STRING);"), ParseError);
}

TEST(ParserTest, ErrorMissingValueKeyword) {
    EXPECT_THROW(parseSQL("INSERT INTO users (id, name) (1, \"Alice\");"), ParseError);
}

TEST(ParserTest, ErrorMissingSetKeyword) {
    EXPECT_THROW(parseSQL("UPDATE users name = \"Bob\" WHERE id == 1;"), ParseError);
}

TEST(ParserTest, ErrorMissingFromKeyword) { EXPECT_THROW(parseSQL("SELECT * users WHERE id == 1;"), ParseError); }

TEST(ParserTest, ErrorMissingBetweenAnd) {
    EXPECT_THROW(parseSQL("SELECT * FROM products WHERE price BETWEEN 10 100;"), ParseError);
}

TEST(ParserTest, ErrorInvalidRevertTimestamp) {
    EXPECT_THROW(parseSQL("REVERT products 2026.5.02-14:30:45.123;"), ParseError);
    EXPECT_THROW(parseSQL("REVERT products tomorrow;"), ParseError);
}

// Complex real-world queries
TEST(ParserTest, ComplexSelectQuery) {
    auto stmt = parseSQL(R"(
        SELECT name AS product_name, AVG(price) AS avg_price, COUNT(id) AS total
        FROM products
        WHERE price >= 1 AND name LIKE "^A.*";
    )");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->SelectItems.size(), 3);
    EXPECT_EQ(select->SelectItems[0].Alias, "product_name");
    EXPECT_EQ(select->SelectItems[1].Alias, "avg_price");
    EXPECT_EQ(select->SelectItems[2].Alias, "total");
}

TEST(ParserTest, ComplexUpdateQuery) {
    auto stmt = parseSQL(R"(
        UPDATE products
        SET price = 99
        WHERE id == 1 AND (name == "Apple" OR name == "Mango");
    )");
    auto* update = dynamic_cast<UpdateStmt*>(stmt.get());
    ASSERT_NE(update, nullptr);
    auto* and_cond = dynamic_cast<AndCondition*>(update->WhereClause.get());
    ASSERT_NE(and_cond, nullptr);
}

TEST(ParserTest, ComplexDeleteQuery) {
    auto stmt = parseSQL("DELETE FROM products WHERE price BETWEEN 0 AND 10;");
    auto* del = dynamic_cast<DeleteStmt*>(stmt.get());
    ASSERT_NE(del, nullptr);
    auto* between = dynamic_cast<BetweenCondition*>(del->WhereClause.get());
    ASSERT_NE(between, nullptr);
}

TEST(ParserTest, MultilineQuery) {
    auto stmt = parseSQL(R"(
        SELECT *
        FROM shop.products
        WHERE (price >= 1 AND price <= 50)
           OR id == 99;
    )");
    auto* select = dynamic_cast<SelectStmt*>(stmt.get());
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->Table.Database, "shop");
    EXPECT_EQ(select->Table.Table, "products");
}

// Validation tests for parser fixes
TEST(ParserTest, ErrorTrailingGarbageTokens) { EXPECT_THROW(parseSQL("SELECT * FROM users; EXTRA"), ParseError); }

TEST(ParserTest, ErrorTrailingGarbageAfterStatement) {
    EXPECT_THROW(parseSQL("CREATE DATABASE shop garbage"), ParseError);
}

TEST(ParserTest, ErrorLeadingCommaInColumnList) { EXPECT_THROW(parseSQL("SELECT ,id FROM users;"), ParseError); }

TEST(ParserTest, ErrorLeadingCommaInInsertColumns) {
    EXPECT_THROW(parseSQL("INSERT INTO users (,id, name) VALUE (1, \"Alice\");"), ParseError);
}

TEST(ParserTest, ErrorLeadingCommaInCreateTable) {
    EXPECT_THROW(parseSQL("CREATE TABLE users (, id INT, name STRING);"), ParseError);
}

TEST(ParserTest, ErrorWrongPunctuationInAggregate) { EXPECT_THROW(parseSQL("SELECT SUM[id] FROM users;"), ParseError); }

TEST(ParserTest, ErrorWrongPunctuationInCondition) {
    EXPECT_THROW(parseSQL("SELECT * FROM users WHERE [id == 1];"), ParseError);
}