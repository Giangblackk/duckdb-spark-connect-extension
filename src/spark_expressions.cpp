#include "spark_expressions.hpp"

#include "duckdb.h"
#include "duckdb/common/enums/expression_type.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/hugeint.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/common/types/hugeint.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/planner/bound_tokens.hpp"
#include "duckdb/planner/expression/bound_between_expression.hpp"
#include "duckdb/planner/expression/bound_columnref_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "spark/connect/expressions.pb.h"
#include "spark/connect/types.pb.h"
#include "spark_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace duckdb {
namespace spark {

::spark::connect::Expression::Literal ValueToLiteral(const Value &value) {
	::spark::connect::Expression::Literal literal;

	// Handle NULL value case first, because any data type can be NULL
	if (value.IsNull()) {
		auto literal_null = literal.mutable_null();
		literal_null->mutable_null()->CopyFrom(::spark::connect::DataType::NULL_());
		return literal;
	}

	// parsing only duckdb literal/primitive data types to Spark literal data types
	switch (value.type().id()) {
	case LogicalTypeId::SQLNULL: {
		auto literal_null = literal.mutable_null();
		literal_null->mutable_null()->CopyFrom(::spark::connect::DataType::NULL_());
		break;
	}
	case LogicalTypeId::BOOLEAN: {
		literal.set_boolean(value.GetValue<bool>());
		break;
	}
	case LogicalTypeId::TINYINT: {
		literal.set_byte(value.GetValue<int8_t>());
		break;
	}
	case LogicalTypeId::SMALLINT: {
		literal.set_short_(value.GetValue<int16_t>());
		break;
	}
	case LogicalTypeId::INTEGER: {
		literal.set_integer(value.GetValue<int32_t>());
		break;
	}
	case LogicalTypeId::BIGINT: {
		literal.set_long_(value.GetValue<int64_t>());
		break;
	}
	case LogicalTypeId::DATE: {
		literal.set_date(value.GetValue<date_t>().days);
		break;
	}
	// all timestamp types is converted to int64 - units of microseconds since the UNIX epoch.
	case LogicalTypeId::TIMESTAMP_SEC: {
		literal.set_timestamp(value.GetValue<timestamp_sec_t>().epoch().value);
		break;
	}
	case LogicalTypeId::TIMESTAMP_MS: {
		literal.set_timestamp(value.GetValue<timestamp_ms_t>().epoch().value);
		break;
	}
	case LogicalTypeId::TIMESTAMP: {
		literal.set_timestamp_ntz(value.GetValue<timestamp_t>().epoch().value);
		break;
	}
	case LogicalTypeId::TIMESTAMP_TZ: {
		literal.set_timestamp(value.GetValue<timestamp_tz_t>().epoch().value);
		break;
	}
	case LogicalTypeId::TIMESTAMP_NS: {
		literal.set_timestamp(value.GetValue<timestamp_ns_t>().epoch().value);
		break;
	}
	case LogicalTypeId::DECIMAL: {
		auto literal_decimal = literal.mutable_decimal();
		auto hugeint_val = value.GetValueUnsafe<hugeint_t>();
		auto decimal_str = Hugeint::ToString(hugeint_val);
		auto scale = DecimalType::GetScale(value.type());
		auto precision = DecimalType::GetWidth(value.type());

		if (scale > 0) {
			if (decimal_str.length() <= static_cast<size_t>(scale)) {
				// Add leading zeros if necessary
				decimal_str = string(scale - decimal_str.length(), '0') + decimal_str;
			}
			decimal_str.insert(decimal_str.length() - scale, ".");
		}
		literal_decimal->set_value(decimal_str);
		literal_decimal->set_scale(scale);
		literal_decimal->set_precision(precision);
		break;
	}
	case LogicalTypeId::FLOAT: {
		literal.set_float_(value.GetValue<float>());
		break;
	}
	case LogicalTypeId::DOUBLE: {
		literal.set_double_(value.GetValue<double>());
		break;
	}
	case LogicalTypeId::CHAR:
	case LogicalTypeId::VARCHAR: {
		literal.set_string(value.GetValue<string>());
		break;
	}
	case LogicalTypeId::BLOB: {
		auto str_val = value.GetValue<string>();
		auto *chr = str_val.c_str();
		literal.set_binary(chr, str_val.size());
		break;
	}
	case LogicalTypeId::BIT: {
		literal.set_string(value.GetValue<string>());
		break;
	}

	case LogicalTypeId::STRUCT: {
		const auto &value_type = value.type();
		auto struct_literal = literal.mutable_struct_();

		auto child_types_and_names = StructType::GetChildTypes(value_type);
		vector<string> child_names;
		vector<LogicalType> child_types;
		for (auto &[child_name, child_type] : child_types_and_names) {
			child_names.push_back(child_name);
			child_types.push_back(child_type);
		}
		vector<idx_t> not_nulls;
		struct_literal->mutable_struct_type()->CopyFrom(ConvertDuckDBToSparkType(child_types, child_names));

		vector<Value> children = StructValue::GetChildren(value);
		for (auto &child : children) {
			struct_literal->mutable_elements()->Add()->CopyFrom(ValueToLiteral(child));
		}
		break;
	}
	case LogicalTypeId::MAP: {
		auto map_literal = literal.mutable_map();

		auto key_type = MapType::KeyType(value.type());
		map_literal->mutable_key_type()->CopyFrom(SetSparkType(key_type));
		auto value_type = MapType::ValueType(value.type());
		map_literal->mutable_value_type()->CopyFrom(SetSparkType(value_type));
		auto map_literal_keys = map_literal->mutable_keys();
		auto map_literal_values = map_literal->mutable_values();
		const vector<Value> children = MapValue::GetChildren(value);
		for (auto &child : children) {
			const vector<Value> &struct_child = StructValue::GetChildren(child);
			// key first, value second
			const Value &child_key = struct_child[0];
			const Value &child_value = struct_child[1];
			map_literal_keys->Add()->CopyFrom(ValueToLiteral(child_key));
			map_literal_values->Add()->CopyFrom(ValueToLiteral(child_value));
		}
		break;
	}
	case LogicalTypeId::LIST: {
		auto array_literal = literal.mutable_array();

		// extract child type, convert to spark type and set element_type attribute
		auto &child_type = ListType::GetChildType(value.type());
		array_literal->mutable_element_type()->CopyFrom(SetSparkType(child_type));

		// extract children and add to elements list
		const vector<Value> &children = ListValue::GetChildren(value);
		auto array_elements = array_literal->mutable_elements();
		for (auto &child : children) {
			array_elements->Add()->CopyFrom(ValueToLiteral(child));
		}
		break;
	}
	case LogicalTypeId::ARRAY: {
		auto array_literal = literal.mutable_array();

		// extract child type, convert to spark type and set element_type attribute
		auto &child_type = ArrayType::GetChildType(value.type());
		array_literal->mutable_element_type()->CopyFrom(SetSparkType(child_type));

		// extract children and add to elements list
		const vector<Value> &children = ArrayValue::GetChildren(value);
		auto array_elements = array_literal->mutable_elements();
		for (auto &child : children) {
			array_elements->Add()->CopyFrom(ValueToLiteral(child));
		}
		break;
	}

	default: {
		throw NotImplementedException("Unsupported literal type for filter pushdown: %s", value.type().ToString());
		break;
	}
	}
	return literal;
}

// Helper function to create a Spark binary comparison expression
::spark::connect::Expression CreateSparkComparison(const string &col_name, const ConstantFilter &constant_filter) {
	auto compare_type = constant_filter.comparison_type;
	auto constant_value = constant_filter.constant;
	::spark::connect::Expression spark_expr;
	auto func = spark_expr.mutable_unresolved_function();

	string function_name;
	switch (compare_type) {
	case ExpressionType::COMPARE_EQUAL:
		function_name = "=";
		break;
	case ExpressionType::COMPARE_GREATERTHAN:
		function_name = ">";
		break;
	case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
		function_name = ">=";
		break;
	case ExpressionType::COMPARE_LESSTHAN:
		function_name = "<";
		break;
	case ExpressionType::COMPARE_LESSTHANOREQUALTO:
		function_name = "<=";
		break;
	case ExpressionType::COMPARE_NOTEQUAL:
		function_name = "!=";
		break;
	default:
		throw NotImplementedException("Unsupported comparison type");
	}

	// set function name
	func->set_function_name(function_name);

	// set first argument - the column name
	::spark::connect::Expression attr_expr;
	attr_expr.mutable_unresolved_attribute()->set_unparsed_identifier(col_name);
	func->add_arguments()->CopyFrom(attr_expr);

	// set second argument - the constant value
	::spark::connect::Expression const_expr;
	const_expr.mutable_literal()->CopyFrom(ValueToLiteral(constant_value));
	func->add_arguments()->CopyFrom(const_expr);
	return spark_expr;
}

::spark::connect::Expression CombineExpressionWithAnd(const vector<::spark::connect::Expression> &exprs) {
	::spark::connect::Expression and_expr;
	auto and_func = and_expr.mutable_unresolved_function();
	and_func->set_function_name("and");
	for (const auto &expr : exprs) {
		and_func->add_arguments()->CopyFrom(expr);
	}
	return and_expr;
}

::spark::connect::Expression ConvertComparison(const BoundComparisonExpression &expr) {
	::spark::connect::Expression spark_expr;

	auto func = spark_expr.mutable_unresolved_function();
	// handle not comparisons
	auto expr_type = expr.GetExpressionType();
	if (expr_type == ExpressionType::COMPARE_NOT_IN || expr_type == ExpressionType::COMPARE_DISTINCT_FROM) {
		func->set_function_name("not");
		auto sub_expr = func->add_arguments();
		auto sub_func = sub_expr->mutable_unresolved_function();
		switch (expr_type) {
		case ExpressionType::COMPARE_NOT_IN:
			sub_func->set_function_name("in");
			break;
		case ExpressionType::COMPARE_DISTINCT_FROM:
			sub_func->set_function_name("<=>");
			break;
		default:
			break;
		}
		// add function's arguments from left to right
		sub_func->add_arguments()->CopyFrom(ConvertExpression(*expr.left));
		sub_func->add_arguments()->CopyFrom(ConvertExpression(*expr.right));
		return spark_expr;
	}
	// handle between comparison
	if (expr_type == ExpressionType::COMPARE_BETWEEN) {
		auto &between_expr = expr.Cast<BoundBetweenExpression>();
		return ConvertBetween(between_expr);
	}

	string function_name;

	switch (expr_type) {
	case ExpressionType::COMPARE_EQUAL:
		function_name = "=";
		break;
	case ExpressionType::COMPARE_NOTEQUAL:
		function_name = "!=";
		break;
	case ExpressionType::COMPARE_LESSTHAN:
		function_name = "<";
		break;
	case ExpressionType::COMPARE_GREATERTHAN:
		function_name = ">";
		break;
	case ExpressionType::COMPARE_LESSTHANOREQUALTO:
		function_name = "<=";
		break;
	case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
		function_name = ">=";
		break;
	case ExpressionType::COMPARE_IN:
		function_name = "in";
		break;
	case ExpressionType::COMPARE_NOT_DISTINCT_FROM:
		function_name = "<=>";
		break;
	default:
		throw NotImplementedException("Unsupported comparison type: %s", ExpressionTypeToString(expr_type));
		break;
	}

	// set function name
	func->set_function_name(function_name);
	func->add_arguments()->CopyFrom(ConvertExpression(*expr.left));
	func->add_arguments()->CopyFrom(ConvertExpression(*expr.right));

	return spark_expr;
}

::spark::connect::Expression ConvertBetween(const BoundBetweenExpression &expr) {
	// X between A and B equivalent to X >= A and X <= B
	auto &upper_expr = *expr.upper;
	auto &lower_expr = *expr.lower;
	auto &input_expr = *expr.input;
	::spark::connect::Expression spark_expr;
	auto func = spark_expr.mutable_unresolved_function();
	func->set_function_name("and");
	// X >= A part
	auto gte_expr = func->add_arguments();
	auto gte_expr_func = gte_expr->mutable_unresolved_function();
	gte_expr_func->set_function_name(">=");
	gte_expr_func->add_arguments()->CopyFrom(ConvertExpression(input_expr));
	gte_expr_func->add_arguments()->CopyFrom(ConvertExpression(lower_expr));

	// X <= B part
	auto lte_expr = func->add_arguments();
	auto lte_expr_func = lte_expr->mutable_unresolved_function();
	lte_expr_func->set_function_name("<=");
	lte_expr_func->add_arguments()->CopyFrom(ConvertExpression(input_expr));
	lte_expr_func->add_arguments()->CopyFrom(ConvertExpression(upper_expr));
	return spark_expr;
}

::spark::connect::Expression ConvertOperator(const BoundOperatorExpression &expr) {
	::spark::connect::Expression spark_expr;
	auto func = spark_expr.mutable_unresolved_function();
	auto op_type = expr.type;
	switch (op_type) {
	case ExpressionType::OPERATOR_NOT: {
		func->set_function_name("not");
		break;
	}
	case duckdb::ExpressionType::COMPARE_IN: {
		func->set_function_name("in");
		break;
	}
	default:
		throw NotImplementedException("Unsupported operation type: %s in BOUND_OPERATOR",
		                              ExpressionTypeToString(op_type));
		break;
	}
	for (idx_t i = 0; i < expr.children.size(); i++) {
		auto &child_expr = *expr.children[i];
		func->add_arguments()->CopyFrom(ConvertExpression(child_expr));
	}
	return spark_expr;
}

::spark::connect::Expression ConvertConjunction(const BoundConjunctionExpression &expr) {
	::spark::connect::Expression spark_expr;
	auto func = spark_expr.mutable_unresolved_function();
	auto conj_type = expr.type;
	switch (conj_type) {
	case ExpressionType::CONJUNCTION_AND:
		func->set_function_name("and");
		break;
	case ExpressionType::CONJUNCTION_OR:
		func->set_function_name("and");
		break;
	case ExpressionType::COMPARE_IN:
		func->set_function_name("in");
		break;
	default:
		throw NotImplementedException("Unsupported operation type: %s in BOUND_CONJUNCTION",
		                              ExpressionTypeToString(conj_type));
		break;
	}
	for (idx_t i = 0; i < expr.children.size(); i++) {
		auto &child_expr = *expr.children[i];
		func->add_arguments()->CopyFrom(ConvertExpression(child_expr));
	}
	return spark_expr;
}

::spark::connect::Expression ConvertFunction(const BoundFunctionExpression &expr) {
	// TODO: verify that function is available in Spark Catalog and the function signature is correct
	::spark::connect::Expression spark_expr;
	auto func = spark_expr.mutable_unresolved_function();
	func->set_function_name(expr.function.name);
	for (idx_t i = 0; i < expr.children.size(); i++) {
		auto &child_expr = *expr.children[i];
		func->add_arguments()->CopyFrom(ConvertExpression(child_expr));
	}
	return spark_expr;
}

::spark::connect::Expression ConvertExpression(const Expression &expression) {
	auto expression_class = expression.GetExpressionClass();
	switch (expression_class) {
	case ExpressionClass::INVALID:
		throw InvalidInputException("Invalid Expression");
		break;
	case ExpressionClass::BOUND_COMPARISON: {
		auto &comp_expr = expression.Cast<BoundComparisonExpression>();
		return ConvertComparison(comp_expr);
	}
	case ExpressionClass::BOUND_CONSTANT: {
		auto &const_expr = expression.Cast<BoundConstantExpression>();
		auto value = const_expr.value;
		// set second argument - the constant value
		::spark::connect::Expression spark_expr;
		spark_expr.mutable_literal()->CopyFrom(ValueToLiteral(value));
		return spark_expr;
	}
	case ExpressionClass::BOUND_COLUMN_REF: {
		auto &col_expr = expression.Cast<BoundColumnRefExpression>();
		::spark::connect::Expression spark_expr;
		spark_expr.mutable_unresolved_attribute()->set_unparsed_identifier(col_expr.GetName());
		return spark_expr;
	}
	case ExpressionClass::BOUND_BETWEEN: {
		auto &between_expr = expression.Cast<BoundBetweenExpression>();
		return ConvertBetween(between_expr);
	}
	case ExpressionClass::BOUND_OPERATOR: {
		auto &op_expr = expression.Cast<BoundOperatorExpression>();
		return ConvertOperator(op_expr);
	}
	case ExpressionClass::BOUND_CONJUNCTION: {
		auto &conj_expr = expression.Cast<BoundConjunctionExpression>();
		return ConvertConjunction(conj_expr);
	}
	case ExpressionClass::BOUND_FUNCTION: {
		auto &func_expr = expression.Cast<BoundFunctionExpression>();
		return ConvertFunction(func_expr);
	}

	case ExpressionClass::AGGREGATE:
	case ExpressionClass::CASE:
	case ExpressionClass::CAST:
	case ExpressionClass::COLUMN_REF:
	case ExpressionClass::COMPARISON:
	case ExpressionClass::CONJUNCTION:
	case ExpressionClass::CONSTANT:
	case ExpressionClass::DEFAULT:
	case ExpressionClass::FUNCTION:
	case ExpressionClass::OPERATOR:
	case ExpressionClass::STAR:
	case ExpressionClass::SUBQUERY:
	case ExpressionClass::WINDOW:
	case ExpressionClass::PARAMETER:
	case ExpressionClass::COLLATE:
	case ExpressionClass::LAMBDA:
	case ExpressionClass::POSITIONAL_REFERENCE:
	case ExpressionClass::BETWEEN:
	case ExpressionClass::LAMBDA_REF:

	case ExpressionClass::BOUND_AGGREGATE:
	case ExpressionClass::BOUND_CASE:
	case ExpressionClass::BOUND_CAST:
	case ExpressionClass::BOUND_DEFAULT:
	case ExpressionClass::BOUND_PARAMETER:
	case ExpressionClass::BOUND_REF:
	case ExpressionClass::BOUND_SUBQUERY:
	case ExpressionClass::BOUND_WINDOW:
	case ExpressionClass::BOUND_UNNEST:
	case ExpressionClass::BOUND_LAMBDA:
	case ExpressionClass::BOUND_LAMBDA_REF:
	case ExpressionClass::BOUND_EXPRESSION:
	case ExpressionClass::BOUND_EXPANDED:
	default: {
		throw InvalidInputException("Unsupported Expression class: %s", ExpressionClassToString(expression_class));
		break;
	}
	}
}
} // namespace spark
} // namespace duckdb
