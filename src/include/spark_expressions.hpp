#pragma once

#include "duckdb/common/types/value.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/expression/bound_between_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "spark/connect/expressions.pb.h"

namespace duckdb {
namespace spark {

::spark::connect::Expression::Literal ValueToLiteral(const Value &value);
::spark::connect::Expression CreateSparkComparison(const string &col_name, const ConstantFilter &constant_filter);
::spark::connect::Expression CombineExpressionWithAnd(const vector<::spark::connect::Expression> &exprs);

::spark::connect::Expression ConvertExpression(const Expression &expr);
::spark::connect::Expression ConvertComparison(const BoundComparisonExpression &expr);
::spark::connect::Expression ConvertBetween(const BoundBetweenExpression &expr);
::spark::connect::Expression ConvertOperator(const BoundOperatorExpression &expr);
::spark::connect::Expression ConvertConjunction(const BoundConjunctionExpression &expr);
::spark::connect::Expression ConvertFunction(const BoundFunctionExpression &expr);
} // namespace spark
} // namespace duckdb
