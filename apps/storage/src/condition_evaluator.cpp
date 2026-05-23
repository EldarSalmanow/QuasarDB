#include <qdb/storage/condition_evaluator.h>

#include <regex>

namespace qdb::storage {

ConditionEvaluator::ConditionEvaluator(const Schema& schema, const Record& record, Interner& interner)
        : schema_(schema), record_(record), interner_(interner) {}

auto ConditionEvaluator::Evaluate(const qdb::server::Condition& condition) const -> SqlBool {
    using namespace qdb::server;

    if (const auto* node = dynamic_cast<const ComparisonCondition*>(&condition)) {
        const auto left = ValueOf(*node->Left);
        const auto right = ValueOf(*node->Right);

        switch (node->Op) {
            case ComparisonCondition::Operator::Equal:
                return left == right;
            case ComparisonCondition::Operator::NotEqual:
                return left != right;
            case ComparisonCondition::Operator::Less:
                return left < right;
            case ComparisonCondition::Operator::Greater:
                return left > right;
            case ComparisonCondition::Operator::LessEqual:
                return left <= right;
            case ComparisonCondition::Operator::GreaterEqual:
                return left >= right;
        }
    }

    if (const auto* node = dynamic_cast<const BetweenCondition*>(&condition)) {
        const auto value = ValueOf(*node->Value);
        return (value >= ValueOf(*node->Lower)) && (value <= ValueOf(*node->Upper));
    }

    if (const auto* node = dynamic_cast<const LikeCondition*>(&condition)) {
        const auto value = ValueOf(*node->Value);
        if (value.is_null()) {
            return SqlBool::UNKNOWN;
        }
        if (!value.is_string()) {
            throw std::runtime_error("LIKE expects string value");
        }

        return std::regex_match(value.to_string(), std::regex(node->Pattern)) ? SqlBool::TRUE : SqlBool::FALSE;
    }

    if (const auto* node = dynamic_cast<const AndCondition*>(&condition)) {
        return Evaluate(*node->Left) && Evaluate(*node->Right);
    }

    if (const auto* node = dynamic_cast<const OrCondition*>(&condition)) {
        return Evaluate(*node->Left) || Evaluate(*node->Right);
    }

    throw std::runtime_error("Unsupported condition");
}

auto ConditionEvaluator::ValueOf(const qdb::server::Expression& expression) const -> Value {
    using namespace qdb::server;

    if (const auto* node = dynamic_cast<const IdentifierExpr*>(&expression)) {
        const auto index = schema_.get_column_idx(node->Name);
        if (index < 0) {
            throw std::runtime_error("Unknown column: " + node->Name);
        }

        return record_[index];
    }

    if (const auto* node = dynamic_cast<const LiteralExpr*>(&expression)) {
        return LiteralValue(*node->LiteralValue);
    }

    throw std::runtime_error("Unsupported expression in condition");
}

auto ConditionEvaluator::LiteralValue(const qdb::server::Literal& literal) const -> Value {
    using Type = qdb::server::Literal::Type;

    if (literal.LiteralType == Type::Null) {
        return {};
    }
    if (literal.LiteralType == Type::Integer) {
        return Value(std::stoi(literal.Value));
    }

    return interner_.str_to_value(literal.Value);
}

}  // namespace qdb::storage
