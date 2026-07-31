#pragma once

#include "duckdb/common/types/value.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "spark/connect/expressions.pb.h"

namespace duckdb {
namespace spark {

::spark::connect::Expression::Literal ValueToLiteral(const Value &value);
::spark::connect::Expression CreateSparkComparison(const string &col_name, const ConstantFilter &constant_filter);
::spark::connect::Expression CombineExpressionWithAnd(const vector<::spark::connect::Expression> &exprs);

::spark::connect::Expression ConvertExpression(const Expression &expr);
::spark::connect::Expression ConvertComparison(const BoundComparisonExpression &expr);
} // namespace spark
} // namespace duckdb
