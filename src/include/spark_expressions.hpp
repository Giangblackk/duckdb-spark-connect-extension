#pragma once

#include "duckdb/common/types/value.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/expression/bound_between_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "spark/connect/expressions.pb.h"

namespace duckdb {
namespace spark {

unique_ptr<::spark::connect::Expression::Literal> ValueToLiteral(const Value &value);
::spark::connect::Expression CombineExpressionWithAnd(const vector<unique_ptr<::spark::connect::Expression>> &exprs);

unique_ptr<::spark::connect::Expression> ConvertExpression(const Expression &expr);
unique_ptr<::spark::connect::Expression> ConvertComparison(const BoundComparisonExpression &expr);
unique_ptr<::spark::connect::Expression> ConvertBetween(const BoundBetweenExpression &expr);
unique_ptr<::spark::connect::Expression> ConvertOperator(const BoundOperatorExpression &expr);
unique_ptr<::spark::connect::Expression> ConvertConjunction(const BoundConjunctionExpression &expr);
unique_ptr<::spark::connect::Expression> ConvertFunction(const BoundFunctionExpression &expr);
} // namespace spark
} // namespace duckdb
