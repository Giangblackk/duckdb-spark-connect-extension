#include "spark_scan_table.hpp"

#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/common/constants.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/optimizer/column_lifetime_analyzer.hpp"
#include "duckdb/planner/bound_tokens.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "spark/connect/base.pb.h"
#include "spark/connect/expressions.pb.h"
#include "spark_arrow_reader.hpp"
#include "spark_expressions.hpp"

namespace duckdb {
namespace spark {

void SparkScanTableFunction::SparkScanTableExecute(ClientContext &context, TableFunctionInput &data_p,
                                                   DataChunk &output) {
	if (!data_p.local_state) {
		return;
	}

	auto &bind_data = data_p.bind_data->CastNoConst<SparkScanTableBindData>();
	auto &global_state = data_p.global_state->Cast<SparkScanTableGlobalState>();
	auto &local_state = data_p.local_state->Cast<SparkScanTableLocalState>();

	local_state.all_columns.Reset();
	if (local_state.chunk_offset >= static_cast<idx_t>(local_state.chunk->arrow_array.length)) {
		if (!ArrowTableFunction::ArrowScanParallelStateNext(context, data_p.bind_data.get(), local_state,
		                                                    global_state)) {
			return;
		}
	}

	auto output_size = MinValue<idx_t>(STANDARD_VECTOR_SIZE, NumericCast<idx_t>(local_state.chunk->arrow_array.length) -
	                                                             local_state.chunk_offset);
	output.SetCardinality(output_size);
	if (output_size > 0) {
		ArrowTableFunction::ArrowToDuckDB(local_state, bind_data.arrow_table.GetColumns(), output, 0,
		                                  COLUMN_IDENTIFIER_ROW_ID);
	}
	output.Verify();
	local_state.chunk_offset += output.size();
	lock_guard<mutex> glock(global_state.lock);
	global_state.position += output.size();
}

unique_ptr<ArrowArrayStreamWrapper> SparkProduceArrowScan(const ArrowScanFunctionData &function,
                                                          const vector<column_t> &column_ids, TableFilterSet *filters) {
	ArrowStreamParameters parameters;
	return function.scanner_producer(function.stream_factory_ptr, parameters);
}

unique_ptr<GlobalTableFunctionState> SparkScanTableFunction::SparkScanTableInitGlobal(ClientContext &context,
                                                                                      TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->CastNoConst<SparkScanTableBindData>();
	auto spark_client = bind_data.spark_client;

	// Get list of selected fields to build projection pushdown
	vector<string> selected_fields;
	selected_fields.reserve(input.column_ids.size());
	for (auto col_id : input.column_ids) {
		if (col_id == COLUMN_IDENTIFIER_ROW_ID || col_id < 0) {
			continue;
		}
		selected_fields.emplace_back(bind_data.names[col_id]);
	}

	auto read_table_rel = spark_client->CreateRelationReadTable(bind_data.params.table_name);
	auto projection_pushdown_rel = spark_client->AddColumnProjection(read_table_rel, selected_fields);

	auto &filter_expressions = bind_data.filter_expressions;

	::spark::connect::Plan scan_table_plan;
	if (!filter_expressions.empty()) {
		::spark::connect::Expression filter_expression;
		if (filter_expressions.size() == 1) {
			filter_expression = filter_expressions[0];
		} else {
			filter_expression = CombineExpressionWithAnd(filter_expressions);
		}

		auto filter_pushdown_rel = spark_client->AddFilter(projection_pushdown_rel, filter_expression);
		scan_table_plan = spark_client->PlanFromRelation(filter_pushdown_rel);
	} else {
		scan_table_plan = spark_client->PlanFromRelation(projection_pushdown_rel);
	}

	auto gstate = make_uniq<SparkScanTableGlobalState>();

	auto factory = make_shared_ptr<SparkStreamFactory>(bind_data.spark_client, scan_table_plan);
	auto factory_dependency = bind_data.GetFactoryDependency();
	if (!factory_dependency) {
		throw InternalException("Factory dependency not initialized");
	}

	factory_dependency->factory = factory;
	bind_data.stream_factory_ptr = reinterpret_cast<uintptr_t>(factory.get());

	gstate->stream = SparkProduceArrowScan(bind_data, input.column_ids, input.filters.get());

	return std::move(gstate);
}

unique_ptr<LocalTableFunctionState>
SparkScanTableFunction::SparkScanTableInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                GlobalTableFunctionState *global_state_p) {
	auto &client_context = context.client;
	auto &bind_data = input.bind_data->CastNoConst<SparkScanTableBindData>();
	auto &gstate = global_state_p->Cast<SparkScanTableGlobalState>();

	auto current_chunk = make_uniq<ArrowArrayWrapper>();
	auto lstate = make_uniq<SparkScanTableLocalState>(std::move(current_chunk), client_context);

	auto sorted_column_ids = input.column_ids;
	std::sort(sorted_column_ids.begin(), sorted_column_ids.end());

	lstate->column_ids = sorted_column_ids;
	lstate->filters = input.filters.get();
	if (!bind_data.projection_pushdown_enabled) {
		lstate->column_ids.clear();
	} else if (!input.projection_ids.empty() || !gstate.projection_ids.empty()) {
		auto &asgs = global_state_p->Cast<ArrowScanGlobalState>();
		lstate->all_columns.Initialize(client_context, asgs.scanned_types);
	}
	if (!ArrowTableFunction::ArrowScanParallelStateNext(client_context, input.bind_data.get(), *lstate, gstate)) {
		return nullptr;
	}

	return std::move(lstate);
}

unique_ptr<FunctionData> SparkScanTableFunction::SparkScanTableBind(ClientContext &context,
                                                                    TableFunctionBindInput &input,
                                                                    vector<LogicalType> &return_types,
                                                                    vector<string> &names) {
	// Get parameters
	auto endpoint = input.named_parameters["endpoint"].GetValue<std::string>();
	auto table_name = input.named_parameters["table_name"].GetValue<std::string>();
	ScanTableParams params;
	params.table_name = table_name;

	// Get existing Spark gRPC Client or create new for this endpoint
	auto sparkClient = SparkGRPCClient::GetOrCreateSparkClient(context, endpoint);

	// Add Spark gRPC client to table function data
	unique_ptr<SparkScanTableBindData> bind_data = make_uniq<SparkScanTableBindData>(sparkClient, params);

	// build Spark gRPC Plan to get table schema
	auto read_table_plan = bind_data->spark_client->PlanReadTable(params.table_name);

	// get arrow schema from analyzing plan, export and populate shema to attributes
	auto arrow_shema = bind_data->spark_client->AnalyzePlanToArrowSchema(read_table_plan);
	auto status = arrow::ExportSchema(*std::move(arrow_shema), &bind_data->schema_root.arrow_schema);
	if (!status.ok()) {
		throw BinderException("Arrow schema export failed: " + status.ToString());
	}
	ArrowTableFunction::PopulateArrowTableSchema(duckdb::DBConfig::GetConfig(context), bind_data->arrow_table,
	                                             bind_data->schema_root.arrow_schema);

	// Initialize the names and return types from analyze plan
	names = bind_data->arrow_table.GetNames();
	return_types = bind_data->arrow_table.GetTypes();

	bind_data->names = names;
	bind_data->all_types = return_types;

	return std::move(bind_data);
}

static bool TryConvertEpxression(ClientContext &context, const Expression &expr, LogicalGet &get,
                                 SparkScanTableBindData &bind_data) {
	bind_data.filter_expressions.push_back(ConvertExpression(expr));
	return false;
}

// Pushdown complex filter callback.
// Processes all filter expressions in a single pass and pushes recognized ones to Spark:
// - Comparison filters: =, !=, <, <=, >, >=
// - IS NULL / IS NOT NULL
// - IN expressions
// - LIKE/ILIKE patterns and prefix/suffix/contains
// - AND/OR conjunctions
//
// All recognized filters are pushed into get.table_filters and consumed from the filters vector.
// This approach handles everything in one pass, avoiding the issue where pushing to table_filters
// in pushdown_complex_filter causes DuckDB's optimizer to skip the FilterCombiner path,
// which would prevent standard comparison filters from being pushed down.
//
// This causes DuckDB's FilterCombiner (which runs after this callback) to see a non-empty
// table_filters and skip its own pushdown, preventing it from re-pushing the deferred filters.
void SparkPushdownComplexFilter(ClientContext &context, LogicalGet &get, FunctionData *bind_data_p,
                                vector<unique_ptr<Expression>> &filters) {
	auto &bind_data = bind_data_p->Cast<SparkScanTableBindData>();
	for (idx_t i = 0; i < filters.size(); i++) {
		auto &filter = filters[i];
		if (!filter) {
			continue;
		}
		vector<ColumnBinding> bindings;
		ColumnLifetimeAnalyzer::ExtractColumnBindings(*filter, bindings);

		if (bindings.empty()) {
			// no columns referenced at all (pure constant expression)
			continue;
		} else {
			if (TryConvertEpxression(context, *filter, get, bind_data)) {
				filters[i] = nullptr;
			}
			continue;
		}
	}
	// Remove processed filters.
	filters.erase(
	    std::remove_if(filters.begin(), filters.end(), [](const unique_ptr<Expression> &e) { return e == nullptr; }),
	    filters.end());
}

SparkScanTableFunction::SparkScanTableFunction()
    : TableFunction("spark_scan_table", {}, SparkScanTableFunction::SparkScanTableExecute,
                    SparkScanTableFunction::SparkScanTableBind, SparkScanTableFunction::SparkScanTableInitGlobal,
                    SparkScanTableInitLocal) {
	named_parameters["endpoint"] = LogicalType::VARCHAR;
	named_parameters["table_name"] = LogicalType::VARCHAR;

	projection_pushdown = true;
	// enable simple non-composite filters
	filter_pushdown = true;
	// prune out filter columns that are unused in the remainder of query plan
	filter_prune = true;
	// pushdown a set of arbitrary filter expressions
	pushdown_complex_filter = SparkPushdownComplexFilter;
}

} // namespace spark
} // namespace duckdb
