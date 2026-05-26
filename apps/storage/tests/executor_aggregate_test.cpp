#include <qdb/storage/executor.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace qdb::storage {
namespace {

using namespace qdb::server;

auto Id(std::string name) -> std::unique_ptr<Expression> {
    return std::make_unique<IdentifierExpr>(std::move(name));
}

auto Int(int value) -> std::unique_ptr<Expression> {
    return std::make_unique<LiteralExpr>(std::make_unique<Literal>(Literal::Type::Integer, std::to_string(value)));
}

auto Agg(AggregateExpr::Function function, std::string column, std::string alias) -> SelectItem {
    return SelectItem(std::make_unique<AggregateExpr>(function, std::move(column)), std::move(alias));
}

}  // namespace

TEST(ExecutorAggregateTest, SelectAggregatesWithWhere) {
    const auto root = std::filesystem::temp_directory_path() / "qdb_executor_aggregate";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    Interner interner;
    Table table("users", root, Schema({
        Column("id", Column::ColumnType::INT),
        Column("age", Column::ColumnType::INT),
        Column("name", Column::ColumnType::STRING),
    }), &interner);
    table.insert_record({Value(1), Value(17), interner.Intern("Ann")});
    table.insert_record({Value(2), Value(18), interner.Intern("Bob")});
    table.insert_record({Value(3), Value(30), interner.Intern("Cara")});

    Executor executor(table, interner);

    std::vector<SelectItem> items;
    items.push_back(Agg(AggregateExpr::Function::Count, "id", "count_id"));
    items.push_back(Agg(AggregateExpr::Function::Sum, "age", "sum_age"));
    items.push_back(Agg(AggregateExpr::Function::Avg, "age", "avg_age"));

    auto where = std::make_unique<ComparisonCondition>(
        Id("age"),
        ComparisonCondition::Operator::GreaterEqual,
        Int(18)
    );
    SelectStmt select(false, std::move(items), TableRef("users"), std::move(where));

    auto response = executor.Execute(select);
    ASSERT_EQ(response["status"], "success");

    const auto& row = response["data"]["result"].at(0);
    EXPECT_EQ(row["count_id"], 2);
    EXPECT_EQ(row["sum_age"], 48);
    EXPECT_DOUBLE_EQ(row["avg_age"].get<double>(), 24.0);

    std::filesystem::remove_all(root);
}

}  // namespace qdb::storage
