#include "spark_insert.hpp"

#include "duckdb/common/arrow/arrow.hpp"
#include "duckdb/common/arrow/arrow_appender.hpp"
#include "duckdb/common/arrow/arrow_converter.hpp"
#include "duckdb/common/assert.hpp"
#include "duckdb/common/constants.hpp"
#include "duckdb/common/enums/operator_result_type.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/common/pair.hpp"
#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/execution/operator/persistent/physical_insert.hpp"
#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/planner/operator/logical_create_table.hpp"
#include "duckdb/planner/operator/logical_insert.hpp"
#include "duckdb/storage/table/append_state.hpp"
#include "spark_catalog.hpp"
#include "spark_table_entry.hpp"

#include <arrow/buffer.h>
#include <arrow/c/bridge.h>
#include <arrow/io/api.h>
#include <arrow/io/type_fwd.h>
#include <arrow/ipc/api.h>
#include <arrow/type_fwd.h>
#include <string>

namespace duckdb {
namespace spark {

SparkInsert::SparkInsert(PhysicalPlan &physical_plan, vector<LogicalType> types, TableCatalogEntry &table,
                         vector<unique_ptr<BoundConstraint>> bound_constraints_p,
                         vector<unique_ptr<Expression>> set_expressions, vector<PhysicalIndex> set_columns,
                         vector<LogicalType> set_types, physical_index_vector_t<idx_t> column_index_map_p,
                         idx_t estimated_cardinality, OnConflictAction action_type,
                         unique_ptr<Expression> on_conflict_condition_p, unique_ptr<Expression> do_update_condition_p,
                         unordered_set<column_t> conflict_target_p, vector<unique_ptr<Expression>> bound_defaults)
    : PhysicalOperator(physical_plan, PhysicalOperatorType::EXTENSION, std::move(types), estimated_cardinality),
      insert_table(&table), insert_types(table.GetTypes()), schema(&table.schema),
      column_index_map(std::move(column_index_map_p)), bound_defaults(std::move(bound_defaults)),
      bound_constraints(std::move(bound_constraints_p)), set_expressions(std::move(set_expressions)),
      set_columns(std::move(set_columns)), action_type(action_type), set_types(std::move(set_types)),
      on_conflict_condition(std::move(on_conflict_condition_p)), do_update_condition(std::move(do_update_condition_p)),
      conflict_target(std::move(conflict_target_p)) {
}

SparkInsert::SparkInsert(PhysicalPlan &physical_plan, LogicalOperator &op, SchemaCatalogEntry &schema,
                         unique_ptr<BoundCreateTableInfo> create_info, idx_t estimated_cardinality)
    : PhysicalOperator(physical_plan, PhysicalOperatorType::CREATE_TABLE_AS, op.types, estimated_cardinality),
      info(std::move(create_info)), insert_table(nullptr), schema(&schema) {
	PhysicalInsert::GetInsertInfo(*info, insert_types);
}

static pair<vector<string>, vector<LogicalType>> GetInsertColumns(const SparkInsert &insert, SparkTableEntry &entry) {
	vector<string> column_names;
	vector<LogicalType> column_types;
	auto &columns = entry.GetColumns();
	for (auto &col : columns.Logical()) {
		column_types.push_back(col.GetType());
		column_names.push_back(col.GetName());
	}
	return make_pair(column_names, column_types);
}

unique_ptr<GlobalSinkState> SparkInsert::GetGlobalSinkState(ClientContext &context) const {
	optional_ptr<SparkTableEntry> table;
	if (info) {                  // CTAS
		D_ASSERT(!insert_table); // then insert_table should be null
		// get mutable schema entry
		const auto &schema_ref = schema.get_mutable();
		// create table from CTAS info
		table = &schema_ref->CreateTable(schema_ref->GetCatalogTransaction(context), *info)->Cast<SparkTableEntry>();
	} else {                    // INSERT INTO
		D_ASSERT(insert_table); // then insert_table should not be null
		// get mutable table entry
		table = &insert_table.get_mutable()->Cast<SparkTableEntry>();
	}
	// ensure table is not null
	D_ASSERT(table != nullptr);

	// intialize global state
	auto insert_global_state = make_uniq<SparkInsertGlobalState>(context, *table, GetTypes());

	// FIXME: so if the user doesn't specify the column list
	// it means that the send_names/send_types is empty.
	auto [send_names, send_types] = GetInsertColumns(*this, *table);
	D_ASSERT(send_names.size() == send_types.size());
	D_ASSERT(send_names.size() > 0);
	D_ASSERT(send_types.size() > 0);

	// build ArrowSchema in C Interface
	ArrowSchema send_schema;
	auto client_properties = context.GetClientProperties();
	ArrowConverter::ToArrowSchema(&send_schema, send_types, send_names, client_properties);

	// convert ArrowSchema to Arrow::Schema in C++ interface
	insert_global_state->insert_schema = arrow::ImportSchema((ArrowSchema *)&send_schema).ValueOrDie();
	return insert_global_state;
}
unique_ptr<LocalSinkState> SparkInsert::GetLocalSinkState(ExecutionContext &context) const {
	auto state = make_uniq<SparkInsertLocalState>(context.client, insert_types, bound_defaults, bound_constraints);
	return state;
}

SourceResultType SparkInsert::GetData(ExecutionContext &context, DataChunk &chunk, OperatorSourceInput &input) const {
	auto &gstate = sink_state->Cast<SparkInsertGlobalState>();
	chunk.SetCapacity(1);
	chunk.SetValue(0, 0, Value::BIGINT(NumericCast<int64_t>(gstate.changed_count)));
	return SourceResultType::FINISHED;
}

SinkResultType SparkInsert::Sink(ExecutionContext &context, DataChunk &chunk, OperatorSinkInput &input) const {
	auto &gstate = input.global_state.Cast<SparkInsertGlobalState>();
	auto &lstate = input.local_state.Cast<SparkInsertLocalState>();

	// append chunk to local state's appender
	lstate.appender->Append(chunk, 0, chunk.size(), chunk.size());
	gstate.changed_count += chunk.size();
	return SinkResultType::NEED_MORE_INPUT;
}

SinkCombineResultType SparkInsert::Combine(ExecutionContext &context, OperatorSinkCombineInput &input) const {
	auto &gstate = input.global_state.Cast<SparkInsertGlobalState>();
	auto &lstate = input.local_state.Cast<SparkInsertLocalState>();

	// return underlying arrow array
	ArrowArray arr = lstate.appender->Finalize();

	// convert into RecordBatch
	auto record_batch = arrow::ImportRecordBatch(&arr, gstate.insert_schema).ValueOrDie();

	// write to global state buffer stream
	auto writer = arrow::ipc::MakeStreamWriter(gstate.buffer_stream, gstate.insert_schema).ValueOrDie();
	auto write_status = writer->WriteRecordBatch(*record_batch);
	auto close_status = writer->Close();
	return SinkCombineResultType::FINISHED;
}

SinkFinalizeType SparkInsert::Finalize(Pipeline &pipeline, Event &event, ClientContext &context,
                                       OperatorSinkFinalizeInput &input) const {
	auto &gstate = input.global_state.Cast<SparkInsertGlobalState>();
	auto spark_client = schema->catalog.Cast<SparkCatalog>().spark_client;

	// close the stream and return buffer
	auto buffer = gstate.buffer_stream->Finish().ValueOrDie();
	auto data = reinterpret_cast<const char *>(buffer->data());
	auto data_size = buffer->size();

	// build Spark Connect Plan
	auto plan = spark_client->PlanWriteOperationV2(
	    schema->name, gstate.table.name, ::spark::connect::WriteOperationV2::Mode::WriteOperationV2_Mode_MODE_APPEND,
	    data, data_size);

	auto status = spark_client->GetStatus(plan);
	if (!status.ok()) {
		throw ExecutorException("Fail to insert into table `%s` in schema `%s` of catalog `%s`. Error: `%s`.",
		                        gstate.table.name, schema->name, schema->catalog.GetName(), status.error_message());
	}

	return SinkFinalizeType::READY;
}

string SparkInsert::GetName() const {
	return info ? "SPARK_INSERT" : "SPARK_CREATE_TABLE_AS";
}

InsertionOrderPreservingMap<string> SparkInsert::ParamsToString() const {
	InsertionOrderPreservingMap<string> result;
	result["Table Name"] = !info ? insert_table->name : info->Base().table;
	return result;
}

PhysicalOperator &SparkCatalog::PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner, LogicalInsert &op,
                                           optional_ptr<PhysicalOperator> plan) {
	if (op.return_chunk) {
		throw BinderException("RETURNING clause not yet supported for insertion into Spark table.");
	}
	if (op.on_conflict_info.action_type != OnConflictAction::THROW) {
		throw BinderException("ON CONFLICT clause yet supported for insertion into Spark table.");
	}
	auto &insert = planner.Make<SparkInsert>(
	    op.types, op.table, std::move(op.bound_constraints), std::move(op.expressions),
	    std::move(op.on_conflict_info.set_columns), std::move(op.on_conflict_info.set_types), op.column_index_map,
	    op.estimated_cardinality, op.on_conflict_info.action_type, std::move(op.on_conflict_info.on_conflict_condition),
	    std::move(op.on_conflict_info.do_update_condition), std::move(op.on_conflict_info.on_conflict_filter),
	    std::move(op.bound_defaults));
	if (plan) {
		insert.children.push_back(*plan);
	}
	return insert;
}

PhysicalOperator &SparkCatalog::PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner,
                                                  LogicalCreateTable &op, PhysicalOperator &plan) {
	auto &insert = planner.Make<SparkInsert>(op, op.schema, std::move(op.info), op.estimated_cardinality);
	insert.children.push_back(plan);
	return insert;
}

} // namespace spark
} // namespace duckdb
