#include <qdb/server/parser.h>

#include <utility>


namespace qdb::server {

ParseError::ParseError(const std::string& message) : std::runtime_error(message) {}

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

auto Parser::ParseStatement() -> std::unique_ptr<Statement> {
    if (IsAtEnd()) {
        throw ParseError("Unexpected end of input");
    }

    Token current = Peek();
    if (current.type != TokenType::Keyword) {
        throw ParseError("Expected statement, got: " + current.value);
    }

    const std::string& keyword = current.value;
    std::unique_ptr<Statement> stmt;

    if (keyword == "CREATE") stmt = ParseCreateStatement();
    else if (keyword == "DROP") stmt = ParseDropStatement();
    else if (keyword == "USE") stmt = ParseUseStatement();
    else if (keyword == "GRANT") stmt = ParseGrantStatement();
    else if (keyword == "REVOKE") stmt = ParseRevokeStatement();
    else if (keyword == "REVERT") stmt = ParseRevertStatement();
    else if (keyword == "INSERT") stmt = ParseInsertStatement();
    else if (keyword == "UPDATE") stmt = ParseUpdateStatement();
    else if (keyword == "DELETE") stmt = ParseDeleteStatement();
    else if (keyword == "SELECT") stmt = ParseSelectStatement();
    else throw ParseError("Unknown statement: " + keyword);

    if (Match(TokenType::Punctuation, ";")) {
        Advance();
    }

    if (!IsAtEnd()) {
        throw ParseError("Unexpected tokens after statement: " + Peek().value);
    }

    return stmt;
}

auto Parser::IsAtEnd() const -> bool {
    return pos_ >= tokens_.size() || Peek().type == TokenType::EndOfFile;
}

auto Parser::Peek() const -> Token {
    if (pos_ >= tokens_.size()) {
        return Token(TokenType::EndOfFile, "", 0, 0);
    }
    return tokens_[pos_];
}

auto Parser::Advance() -> Token {
    if (!IsAtEnd()) {
        return tokens_[pos_++];
    }
    return Token(TokenType::EndOfFile, "", 0, 0);
}

auto Parser::Match(TokenType type, const std::string& value) -> bool {
    if (IsAtEnd()) return false;
    Token current = Peek();
    if (current.type != type) return false;
    if (!value.empty() && current.value != value) return false;
    return true;
}

auto Parser::Expect(TokenType type, const std::string& message) -> void {
    if (IsAtEnd() || Peek().type != type) {
        throw ParseError(message);
    }
    Advance();
}

auto Parser::Expect(TokenType type, const std::string& value, const std::string& message) -> void {
    if (IsAtEnd() || Peek().type != type || Peek().value != value) {
        throw ParseError(message);
    }
    Advance();
}

auto Parser::ExpectPunctuation(const std::string& punctuation) -> void {
    if (!Match(TokenType::Punctuation, punctuation)) {
        throw ParseError("Expected '" + punctuation + "', got: " + Peek().value);
    }
    Advance();
}

auto Parser::ExpectOperator(const std::string& op) -> void {
    if (!Match(TokenType::Operator, op)) {
        throw ParseError("Expected operator '" + op + "', got: " + Peek().value);
    }
    Advance();
}

auto Parser::ExpectKeyword(const std::string& keyword) -> void {
    if (!Match(TokenType::Keyword, keyword)) {
        throw ParseError("Expected keyword '" + keyword + "', got: " + Peek().value);
    }
    Advance();
}

auto Parser::ParseIdentifier() -> std::string {
    if (!Match(TokenType::Identifier)) {
        throw ParseError("Expected identifier, got: " + Peek().value);
    }
    return Advance().value;
}

auto Parser::ParseTableRef() -> TableRef {
    std::string first = ParseIdentifier();

    if (Match(TokenType::Punctuation, ".")) {
        Advance();
        std::string second = ParseIdentifier();
        return TableRef(std::move(first), std::move(second));
    }

    return TableRef(std::move(first));
}

auto Parser::ParseAccessScope() -> TableRef {
    auto parse_part = [&]() -> std::string {
        if (Match(TokenType::Operator, "*")) {
            Advance();
            return "*";
        }
        return ParseIdentifier();
    };

    std::string first = parse_part();
    if (Match(TokenType::Punctuation, ".")) {
        Advance();
        return TableRef(std::move(first), parse_part());
    }
    return TableRef(std::move(first), "*");
}

auto Parser::ParsePermissions() -> std::vector<std::string> {
    std::vector<std::string> permissions;
    while (true) {
        if (!Match(TokenType::Keyword, "READ") && !Match(TokenType::Keyword, "WRITE") &&
            !Match(TokenType::Keyword, "CREATE") && !Match(TokenType::Keyword, "DELETE")) {
            throw ParseError("Expected permission READ, WRITE, CREATE, or DELETE");
        }
        permissions.push_back(Advance().value);
        if (!Match(TokenType::Punctuation, ",")) {
            return permissions;
        }
        Advance();
    }
}

auto Parser::ParseTimestampPart(const std::string& name, std::size_t expected_length) -> std::string {
    if (!Match(TokenType::IntegerLiteral)) {
        throw ParseError("Expected " + name + " in timestamp, got: " + Peek().value);
    }

    std::string value = Advance().value;
    if (value.size() != expected_length) {
        throw ParseError("Expected " + std::to_string(expected_length) + " digits for " + name +
                         " in timestamp");
    }

    return value;
}

auto Parser::ParseTimestamp() -> std::string {
    std::string timestamp;

    timestamp += ParseTimestampPart("year", 4);
    ExpectPunctuation(".");
    timestamp += ".";
    timestamp += ParseTimestampPart("month", 2);
    ExpectPunctuation(".");
    timestamp += ".";
    timestamp += ParseTimestampPart("day", 2);
    ExpectPunctuation("-");
    timestamp += "-";
    timestamp += ParseTimestampPart("hour", 2);
    ExpectPunctuation(":");
    timestamp += ":";
    timestamp += ParseTimestampPart("minute", 2);
    ExpectPunctuation(":");
    timestamp += ":";
    timestamp += ParseTimestampPart("second", 2);
    ExpectPunctuation(".");
    timestamp += ".";
    timestamp += ParseTimestampPart("millisecond", 3);

    return timestamp;
}

auto Parser::ParseLiteral() -> std::unique_ptr<Literal> {
    Token current = Peek();

    if (current.type == TokenType::StringLiteral) {
        Advance();
        return std::make_unique<Literal>(Literal::Type::String, current.value);
    }
    if (current.type == TokenType::IntegerLiteral) {
        Advance();
        return std::make_unique<Literal>(Literal::Type::Integer, current.value);
    }
    if (Match(TokenType::Keyword, "NULL")) {
        Advance();
        return std::make_unique<Literal>(Literal::Type::Null, "NULL");
    }

    throw ParseError("Expected literal, got: " + current.value);
}

auto Parser::ParseValue() -> std::unique_ptr<Expression> {
    Token current = Peek();

    if (current.type == TokenType::Identifier) {
        return std::make_unique<IdentifierExpr>(Advance().value);
    }

    return std::make_unique<LiteralExpr>(ParseLiteral());
}

auto Parser::ParseAggregateFunction(const std::string& func) -> AggregateExpr::Function {
    if (func == "SUM") return AggregateExpr::Function::Sum;
    if (func == "COUNT") return AggregateExpr::Function::Count;
    if (func == "AVG") return AggregateExpr::Function::Avg;
    throw ParseError("Unknown aggregate function: " + func);
}

auto Parser::ParseExpression() -> std::unique_ptr<Expression> {
    if (Match(TokenType::Keyword, "SUM") || Match(TokenType::Keyword, "COUNT") ||
        Match(TokenType::Keyword, "AVG")) {
        std::string func = Advance().value;
        ExpectPunctuation("(");
        std::string column = ParseIdentifier();
        ExpectPunctuation(")");
        return std::make_unique<AggregateExpr>(ParseAggregateFunction(func), std::move(column));
    }

    return ParseValue();
}

auto Parser::ParseCondition() -> std::unique_ptr<Condition> {
    return ParseOrCondition();
}

auto Parser::ParseOrCondition() -> std::unique_ptr<Condition> {
    auto left = ParseAndCondition();

    while (Match(TokenType::Keyword, "OR")) {
        Advance();
        auto right = ParseAndCondition();
        left = std::make_unique<OrCondition>(std::move(left), std::move(right));
    }

    return left;
}

auto Parser::ParseAndCondition() -> std::unique_ptr<Condition> {
    auto left = ParsePredicate();

    while (Match(TokenType::Keyword, "AND")) {
        Advance();
        auto right = ParsePredicate();
        left = std::make_unique<AndCondition>(std::move(left), std::move(right));
    }

    return left;
}

auto Parser::ParseComparisonOperator(const std::string& op) -> ComparisonCondition::Operator {
    if (op == "==") return ComparisonCondition::Operator::Equal;
    if (op == "!=") return ComparisonCondition::Operator::NotEqual;
    if (op == "<") return ComparisonCondition::Operator::Less;
    if (op == ">") return ComparisonCondition::Operator::Greater;
    if (op == "<=") return ComparisonCondition::Operator::LessEqual;
    if (op == ">=") return ComparisonCondition::Operator::GreaterEqual;
    throw ParseError("Unknown operator: " + op);
}

auto Parser::ParsePredicate() -> std::unique_ptr<Condition> {
    if (Match(TokenType::Punctuation, "(")) {
        Advance();
        auto condition = ParseCondition();
        ExpectPunctuation(")");
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
        return std::make_unique<LikeCondition>(std::move(value), std::move(pattern));
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

auto Parser::ParseColumnDefinition() -> ColumnDef {
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

auto Parser::ParseCreateStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("CREATE");

    if (Match(TokenType::Keyword, "DATABASE")) {
        Advance();
        std::string db_name = ParseIdentifier();
        return std::make_unique<CreateDatabaseStmt>(std::move(db_name));
    }

    if (Match(TokenType::Keyword, "USER")) {
        Advance();
        std::string username = ParseIdentifier();
        ExpectKeyword("PASSWORD");
        if (!Match(TokenType::StringLiteral)) {
            throw ParseError("Expected password string literal");
        }
        return std::make_unique<CreateUserStmt>(std::move(username), Advance().value);
    }

    if (Match(TokenType::Keyword, "TABLE")) {
        Advance();
        TableRef table = ParseTableRef();

        ExpectPunctuation("(");

        std::vector<ColumnDef> columns;
        columns.push_back(ParseColumnDefinition());
        while (Match(TokenType::Punctuation, ",")) {
            Advance();
            columns.push_back(ParseColumnDefinition());
        }

        ExpectPunctuation(")");

        return std::make_unique<CreateTableStmt>(std::move(table), std::move(columns));
    }

    throw ParseError("Expected DATABASE or TABLE after CREATE");
}

auto Parser::ParseGrantStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("GRANT");
    auto permissions = ParsePermissions();
    ExpectKeyword("ON");
    auto scope = ParseAccessScope();
    ExpectKeyword("TO");
    auto username = ParseIdentifier();
    return std::make_unique<GrantStmt>(std::move(permissions), std::move(scope), std::move(username));
}

auto Parser::ParseRevokeStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("REVOKE");
    auto permissions = ParsePermissions();
    ExpectKeyword("ON");
    auto scope = ParseAccessScope();
    ExpectKeyword("FROM");
    auto username = ParseIdentifier();
    return std::make_unique<RevokeStmt>(std::move(permissions), std::move(scope), std::move(username));
}

auto Parser::ParseDropStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("DROP");

    if (Match(TokenType::Keyword, "DATABASE")) {
        Advance();
        std::string db_name = ParseIdentifier();
        return std::make_unique<DropDatabaseStmt>(std::move(db_name));
    }

    if (Match(TokenType::Keyword, "TABLE")) {
        Advance();
        TableRef table = ParseTableRef();
        return std::make_unique<DropTableStmt>(std::move(table));
    }

    throw ParseError("Expected DATABASE or TABLE after DROP");
}

auto Parser::ParseUseStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("USE");
    std::string db_name = ParseIdentifier();
    return std::make_unique<UseDatabaseStmt>(std::move(db_name));
}

auto Parser::ParseRevertStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("REVERT");
    TableRef table = ParseTableRef();
    std::string timestamp = ParseTimestamp();

    return std::make_unique<RevertStmt>(std::move(table), std::move(timestamp));
}

auto Parser::ParseInsertStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("INSERT");
    ExpectKeyword("INTO");

    TableRef table = ParseTableRef();

    ExpectPunctuation("(");
    std::vector<std::string> columns;
    columns.push_back(ParseIdentifier());
    while (Match(TokenType::Punctuation, ",")) {
        Advance();
        columns.push_back(ParseIdentifier());
    }
    ExpectPunctuation(")");

    ExpectKeyword("VALUE");

    std::vector<std::vector<std::unique_ptr<Literal>>> values;
    do {
        if (Match(TokenType::Punctuation, ",")) {
            Advance();
        }

        ExpectPunctuation("(");
        std::vector<std::unique_ptr<Literal>> row;
        row.push_back(ParseLiteral());
        while (Match(TokenType::Punctuation, ",")) {
            Advance();
            row.push_back(ParseLiteral());
        }
        ExpectPunctuation(")");

        values.push_back(std::move(row));
    } while (Match(TokenType::Punctuation, ","));

    return std::make_unique<InsertStmt>(std::move(table), std::move(columns), std::move(values));
}

auto Parser::ParseUpdateStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("UPDATE");
    TableRef table = ParseTableRef();
    ExpectKeyword("SET");

    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assignments;
    while (true) {
        std::string column = ParseIdentifier();
        ExpectOperator("=");
        auto value = ParseValue();
        assignments.emplace_back(std::move(column), std::move(value));

        if (!Match(TokenType::Punctuation, ",")) {
            break;
        }
        Advance();
    }

    ExpectKeyword("WHERE");
    auto where = ParseCondition();

    return std::make_unique<UpdateStmt>(std::move(table), std::move(assignments), std::move(where));
}

auto Parser::ParseDeleteStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("DELETE");
    ExpectKeyword("FROM");

    TableRef table = ParseTableRef();

    ExpectKeyword("WHERE");
    auto where = ParseCondition();

    return std::make_unique<DeleteStmt>(std::move(table), std::move(where));
}

auto Parser::ParseSelectStatement() -> std::unique_ptr<Statement> {
    ExpectKeyword("SELECT");

    bool select_all = false;
    std::vector<SelectItem> items;

    if (Match(TokenType::Operator, "*")) {
        Advance();
        select_all = true;
    } else {
        while (true) {
            auto expr = ParseExpression();
            std::string alias;
            if (Match(TokenType::Keyword, "AS")) {
                Advance();
                alias = ParseIdentifier();
            }
            items.emplace_back(std::move(expr), std::move(alias));

            if (!Match(TokenType::Punctuation, ",")) {
                break;
            }
            Advance();
        }
    }

    ExpectKeyword("FROM");
    TableRef table = ParseTableRef();

    std::unique_ptr<Condition> where;
    if (Match(TokenType::Keyword, "WHERE")) {
        Advance();
        where = ParseCondition();
    }

    return std::make_unique<SelectStmt>(select_all, std::move(items), std::move(table), std::move(where));
}

}  // namespace qdb::server

