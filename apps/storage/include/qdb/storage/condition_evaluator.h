#ifndef QUASARDB_CONDITION_EVALUATOR_H
#define QUASARDB_CONDITION_EVALUATOR_H

#include <qdb/server/ast.h>
#include <qdb/storage/interner.h>
#include <qdb/storage/record.h>

namespace qdb::storage {

class ConditionEvaluator {
public:
    ConditionEvaluator(const Schema& schema, const Record& record, Interner& interner);

    auto Evaluate(const qdb::server::Condition& condition) const -> SqlBool;

private:
    auto ValueOf(const qdb::server::Expression& expression) const -> Value;

    auto LiteralValue(const qdb::server::Literal& literal) const -> Value;

private:
    const Schema& schema_;
    const Record& record_;
    Interner& interner_;
};

}  // namespace qdb::storage

#endif  // QUASARDB_CONDITION_EVALUATOR_H
