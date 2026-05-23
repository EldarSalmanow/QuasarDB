#include <qdb/storage/executor.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace qdb::storage {
namespace {

using namespace qdb::server;

auto Id(std::string name) -> std::unique_ptr<Expression> {
    return std::make_unique<IdentifierExpr>(std::move(name));
}

auto LitInt(int value) -> std::unique_ptr<Literal> {
    return std::make_unique<Literal>(Literal::Type::Integer, std::to_string(value));
}

auto LitStr(std::string value) -> std::unique_ptr<Literal> {
    return std::make_unique<Literal>(Literal::Type::String, std::move(value));
}

auto Int(int value) -> std::unique_ptr<Expression> {
    return std::make_unique<LiteralExpr>(LitInt(value));
}

auto Str(std::string value) -> std::unique_ptr<Expression> {
    return std::make_unique<LiteralExpr>(LitStr(std::move(value)));
}

auto Cmp(std::string column, ComparisonCondition::Operator op, std::unique_ptr<Expression> value)
    -> std::unique_ptr<Condition> {
    return std::make_unique<ComparisonCondition>(Id(std::move(column)), op, std::move(value));
}

auto Agg(AggregateExpr::Function function, std::string column, std::string alias) -> SelectItem {
    return SelectItem(std::make_unique<AggregateExpr>(function, std::move(column)), std::move(alias));
}

auto Row(std::unique_ptr<Literal> first, std::unique_ptr<Literal> second) -> std::vector<std::unique_ptr<Literal>> {
    std::vector<std::unique_ptr<Literal>> row;
    row.push_back(std::move(first));
    row.push_back(std::move(second));
    return row;
}

auto Row(std::unique_ptr<Literal> first, std::unique_ptr<Literal> second, std::unique_ptr<Literal> third)
    -> std::vector<std::unique_ptr<Literal>> {
    auto row = Row(std::move(first), std::move(second));
    row.push_back(std::move(third));
    return row;
}

class ExecutorTest : public ::testing::Test {
protected:
    std::filesystem::path root;
    Interner interner;
    std::unique_ptr<Table> table;
    std::unique_ptr<Executor> executor;

    void Init(Schema schema) {
        root = std::filesystem::temp_directory_path() / "qdb_executor_test";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        table = std::make_unique<Table>("users", root, std::move(schema), &interner);
        executor = std::make_unique<Executor>(*table, interner);
    }

    void TearDown() override {
        executor.reset();
        table.reset();
        std::filesystem::remove_all(root);
    }

    auto Exec(const Statement& statement) -> nlohmann::json {
        statement.Accept(*executor);
        return executor->Result().value();
    }
};

}  // namespace

TEST_F(ExecutorTest, CreateInsertSelectUpdateDelete) {
    Init(Schema({
        Column("id", Column::ColumnType::INT, Column::INDEXED_FLAG),
        Column("age", Column::ColumnType::INT),
        Column("name", Column::ColumnType::STRING, 0, Column::DefaultType::STRING, 0, "anon"),
    }));

    std::vector<std::vector<std::unique_ptr<Literal>>> rows;
    rows.push_back(Row(LitInt(1), LitInt(17)));
    rows.push_back(Row(LitInt(2), LitInt(21)));
    rows.push_back(Row(LitInt(3), LitInt(30)));
    InsertStmt insert(TableRef("users"), {"id", "age"}, std::move(rows));
    ASSERT_EQ(Exec(insert)["status"], "success");

    std::vector<SelectItem> items;
    items.emplace_back(Id("id"));
    items.emplace_back(Id("name"), "user_name");
    SelectStmt select(false, std::move(items), TableRef("users"),
                      Cmp("age", ComparisonCondition::Operator::GreaterEqual, Int(18)));
    auto selected = Exec(select);
    ASSERT_EQ(selected["status"], "success");
    ASSERT_EQ(selected["data"]["result"].size(), 2);
    EXPECT_EQ(selected["data"]["result"][0]["id"], 2);
    EXPECT_EQ(selected["data"]["result"][0]["user_name"], "anon");

    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> assignments;
    assignments.emplace_back("name", Str("Bob"));
    UpdateStmt update(TableRef("users"), std::move(assignments),
                      Cmp("id", ComparisonCondition::Operator::Equal, Int(2)));
    auto updated = Exec(update);
    EXPECT_EQ(updated["data"]["rows_affected"], 1);

    SelectStmt select_bob(true, {}, TableRef("users"), std::make_unique<LikeCondition>(Id("name"), "^B.*"));
    selected = Exec(select_bob);
    ASSERT_EQ(selected["data"]["result"].size(), 1);
    EXPECT_EQ(selected["data"]["result"][0]["id"], 2);

    DeleteStmt remove(TableRef("users"), Cmp("age", ComparisonCondition::Operator::Less, Int(18)));
    auto deleted = Exec(remove);
    EXPECT_EQ(deleted["data"]["rows_affected"], 1);

    SelectStmt select_all(true, {}, TableRef("users"));
    selected = Exec(select_all);
    EXPECT_EQ(selected["data"]["result"].size(), 2);
}

TEST_F(ExecutorTest, SelectAggregatesWithWhere) {
    Init(Schema({
        Column("id", Column::ColumnType::INT),
        Column("age", Column::ColumnType::INT),
        Column("name", Column::ColumnType::STRING),
    }));

    std::vector<std::vector<std::unique_ptr<Literal>>> rows;
    rows.push_back(Row(LitInt(1), LitInt(17), LitStr("Ann")));
    rows.push_back(Row(LitInt(2), LitInt(18), LitStr("Bob")));
    rows.push_back(Row(LitInt(3), LitInt(30), LitStr("Cara")));
    InsertStmt insert(TableRef("users"), {"id", "age", "name"}, std::move(rows));
    Exec(insert);

    std::vector<SelectItem> items;
    items.push_back(Agg(AggregateExpr::Function::Count, "id", "count_id"));
    items.push_back(Agg(AggregateExpr::Function::Sum, "age", "sum_age"));
    items.push_back(Agg(AggregateExpr::Function::Avg, "age", "avg_age"));
    SelectStmt select(false, std::move(items), TableRef("users"),
                      Cmp("age", ComparisonCondition::Operator::GreaterEqual, Int(18)));
    auto response = Exec(select);

    ASSERT_EQ(response["status"], "success");
    const auto& row = response["data"]["result"].at(0);
    EXPECT_EQ(row["count_id"], 2);
    EXPECT_EQ(row["sum_age"], 48);
    EXPECT_DOUBLE_EQ(row["avg_age"].get<double>(), 24.0);
}

}  // namespace qdb::storage
