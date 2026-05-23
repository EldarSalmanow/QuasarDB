#include <qdb/storage/condition_evaluator.h>

#include <gtest/gtest.h>

namespace qdb::storage {
namespace {

using namespace qdb::server;

auto Id(std::string name) -> std::unique_ptr<Expression> {
    return std::make_unique<IdentifierExpr>(std::move(name));
}

auto Int(int value) -> std::unique_ptr<Expression> {
    return std::make_unique<LiteralExpr>(std::make_unique<Literal>(Literal::Type::Integer, std::to_string(value)));
}

auto Str(std::string value) -> std::unique_ptr<Expression> {
    return std::make_unique<LiteralExpr>(std::make_unique<Literal>(Literal::Type::String, std::move(value)));
}

}  // namespace

TEST(ConditionEvaluatorTest, AndHasHigherPriorityInAst) {
    Interner interner;
    Schema schema({Column("age", Column::ColumnType::INT), Column("name", Column::ColumnType::STRING)});
    Record record(1, {Value(17), interner.str_to_value("Alice")});

    auto condition = OrCondition(
        std::make_unique<ComparisonCondition>(Id("age"), ComparisonCondition::Operator::GreaterEqual, Int(18)),
        std::make_unique<AndCondition>(
            std::make_unique<ComparisonCondition>(Id("name"), ComparisonCondition::Operator::Equal, Str("Alice")),
            std::make_unique<ComparisonCondition>(Id("age"), ComparisonCondition::Operator::Less, Int(20))
        )
    );

    EXPECT_EQ(ConditionEvaluator(schema, record, interner).Evaluate(condition), SqlBool::TRUE);
}

TEST(ConditionEvaluatorTest, LikeUsesRegex) {
    Interner interner;
    Schema schema({Column("name", Column::ColumnType::STRING)});
    Record record(1, {interner.str_to_value("Alice")});

    LikeCondition matches(Id("name"), "^Al.*$");
    LikeCondition misses(Id("name"), "^Bo.*$");

    ConditionEvaluator evaluator(schema, record, interner);
    EXPECT_EQ(evaluator.Evaluate(matches), SqlBool::TRUE);
    EXPECT_EQ(evaluator.Evaluate(misses), SqlBool::FALSE);
}

}  // namespace qdb::storage
