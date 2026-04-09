#pragma once

#include "duckdb.h"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/common/allocator.hpp"
#include "duckdb/common/constants.hpp"
#include "duckdb/common/enum_util.hpp"
#include "duckdb/common/enums/operator_result_type.hpp"
#include "duckdb/common/insertion_order_preserving_map.hpp"
#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/unordered_set.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/execution/physical_operator_states.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/parallel/pipeline.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/planner/bound_constraint.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/common/index_vector.hpp"
#include "duckdb/planner/logical_operator.hpp"
#include "duckdb/planner/parsed_data/bound_create_table_info.hpp"
#include "duckdb/storage/data_table.hpp"
#include "spark_table_entry.hpp"
#include <arrow/type.h>

namespace duckdb {
namespace spark {

class SparkInsertGlobalState : public GlobalSinkState {
public:
	explicit SparkInsertGlobalState(ClientContext &context, SparkTableEntry &table,
	                                const vector<LogicalType> &return_types)
	    : table(table), changed_count(0) {};
	SparkTableEntry &table;
	idx_t changed_count;
	vector<LogicalType> send_types;
	vector<string> send_names;
	std::shared_ptr<arrow::Schema> insert_schema;
};

class SparkInsertLocalState : public LocalSinkState {
public:
	SparkInsertLocalState(ClientContext &context, const vector<LogicalType> &types,
	                      const vector<unique_ptr<Expression>> &bound_defaults,
	                      const vector<unique_ptr<BoundConstraint>> &bound_constraints)
	    : default_executor(context), bound_constraints_(bound_constraints) {
		for (auto &bound_default : bound_defaults) {
			default_executor.AddExpression(*bound_default);
		}
		returning_data_chunk.Initialize(Allocator::Get(context), types);
	};
	ExpressionExecutor default_executor;
	const vector<unique_ptr<BoundConstraint>> &bound_constraints_;
	unique_ptr<ConstraintState> constraint_state_;
	DataChunk returning_data_chunk;
};

class SparkInsert : public PhysicalOperator {
public:
	// INSERT INTO
	SparkInsert(PhysicalPlan &physical_plan, vector<LogicalType> types, TableCatalogEntry &table,
	            vector<unique_ptr<BoundConstraint>> bound_constraints, vector<unique_ptr<Expression>> set_expressions,
	            vector<PhysicalIndex> set_columns, vector<LogicalType> set_types,
	            physical_index_vector_t<idx_t> column_index_map, idx_t estimated_cardinality,
	            OnConflictAction action_type, unique_ptr<Expression> on_conflict_condition,
	            unique_ptr<Expression> do_update_condition, unordered_set<column_t> on_conflict_filter,
	            vector<unique_ptr<Expression>> bound_defaults);
	// CTAS
	explicit SparkInsert(PhysicalPlan &physical_plan, LogicalOperator &op, SchemaCatalogEntry &schema,
	                     unique_ptr<BoundCreateTableInfo> info, idx_t estimated_cardinality);

	SourceResultType GetData(ExecutionContext &context, DataChunk &chunk, OperatorSourceInput &input) const override;

	unique_ptr<GlobalSinkState> GetGlobalSinkState(ClientContext &context) const override;
	unique_ptr<LocalSinkState> GetLocalSinkState(ExecutionContext &context) const override;
	SinkResultType Sink(ExecutionContext &context, DataChunk &chunk, OperatorSinkInput &input) const override;
	SinkFinalizeType Finalize(Pipeline &pipeline, Event &event, ClientContext &context,
	                          OperatorSinkFinalizeInput &input) const override;
	bool IsSource() const override {
		return true;
	}
	bool IsSink() const override {
		return true;
	}
	bool ParallelSink() const override {
		return false;
	}
	string GetName() const override;
	InsertionOrderPreservingMap<string> ParamsToString() const override;

protected:
	idx_t OnConflictHandling(TableCatalogEntry &table, ExecutionContext &context, SparkInsertGlobalState &gstate,
	                         SparkInsertLocalState &lstate, DataChunk &chunk) const;

private:
	//! The table to INSERT INTO
	optional_ptr<TableCatalogEntry> insert_table;
	//! Table schema, in case of CREATE TABLE AS
	optional_ptr<SchemaCatalogEntry> schema;
	//! Create table info, in case of CREATE TABLE AS
	unique_ptr<BoundCreateTableInfo> info;
	//! The insert types
	vector<LogicalType> insert_types;
	//! The default expressions of the columns for which no value is provided
	const vector<unique_ptr<Expression>> bound_defaults;
	//! The bound constraints for the table
	const vector<unique_ptr<BoundConstraint>> bound_constraints;
	// The DO UPDATE set expressions, if 'action_type' is UPDATE
	vector<unique_ptr<Expression>> set_expressions;
	// Which columns are targeted by the set expressions
	vector<PhysicalIndex> set_columns;
	// The types of the columns targeted by a SET expression
	vector<LogicalType> set_types;
	// For now always just throw errors.
	OnConflictAction action_type = OnConflictAction::THROW;
	// Condition for the ON CONFLICT clause
	unique_ptr<Expression> on_conflict_condition;
	// Condition for the DO UPDATE clause
	unique_ptr<Expression> do_update_condition;
	// The column ids to apply the ON CONFLICT on
	unordered_set<column_t> conflict_target;

public:
	//! column_index_map
	physical_index_vector_t<idx_t> column_index_map;
};
} // namespace spark
} // namespace duckdb
