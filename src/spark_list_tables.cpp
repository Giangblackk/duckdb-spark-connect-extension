#include "spark_list_tables.hpp"

#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "spark_client.hpp"

#include <arrow/type_fwd.h>

namespace duckdb {
namespace spark {

struct ListTablesGlobalFunctionState : public GlobalTableFunctionState {
	explicit ListTablesGlobalFunctionState() {
	}
	arrow::RecordBatchVector batches;
	mutex lock;
};

struct ListTablesParams {
	std::string pattern;
	std::string db_name;
};

struct ListTablesBindData : public TableFunctionData {
	explicit ListTablesBindData(std::shared_ptr<SparkGRPCClient> &spark_client, ListTablesParams &params)
	    : spark_client(spark_client), params(params) {
		// build Spark gRPC Plan
		list_tables_plan = spark_client->PlanListTables(params.pattern, params.db_name);
	}
	std::shared_ptr<SparkGRPCClient> spark_client;
	::spark::connect::Plan list_tables_plan;
	ListTablesParams params;
};

static void SparkListTablesFunc(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->CastNoConst<ListTablesBindData>();
	auto &global_state = data_p.global_state->Cast<ListTablesGlobalFunctionState>();
	auto spark_client = bind_data.spark_client;

	// lock for exclusivity in get next from stream/batches
	std::lock_guard<mutex> guard(global_state.lock);
	// if batches are empty, just return
	if (global_state.batches.empty()) {
		return;
	}

	// if not, reset output and convert batch to data chunk
	// output.Reset();

	// pop back to get next batch
	auto next_batch = global_state.batches.back();
	global_state.batches.pop_back();

	// copy RecordBatch to DataChunk
	WriteRecordBatchToDataChunk(context, next_batch, output);
}

static unique_ptr<GlobalTableFunctionState> SparkListTablesGlobalState(ClientContext &context,
                                                                       TableFunctionInitInput &input) {
	auto state = make_uniq<ListTablesGlobalFunctionState>();
	auto &bind_data = input.bind_data->CastNoConst<ListTablesBindData>();
	auto spark_client = bind_data.spark_client;

	state->batches = spark_client->GetRecordBatches(bind_data.list_tables_plan);
	// global state should keep the RecordBatchStreamReader or RecordBachVector
	// local state if needed, should be used to handle when a record batch is bigger than maximum data chunk size to
	// split into multiple data chunks if needed
	// schema should be setup in bind stage, to return column names and data types
	return std::move(state);
	return nullptr;
}

static unique_ptr<FunctionData> SparkListTablesBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
	// // Get parameters
	auto endpoint = input.inputs[0].GetValue<std::string>();
	auto pattern = input.inputs[1].GetValue<std::string>();
	auto db_name = input.inputs[2].GetValue<std::string>();
	ListTablesParams params;
	params.pattern = pattern;
	params.db_name = db_name;

	// // Get existing Spark gRPC Client or create new for this endpoint
	auto sparkClient = SparkGRPCClient::GetOrCreateSparkClient(context, endpoint);

	// Add Spark gRPC client to table function data
	unique_ptr<ListTablesBindData> bind_data = make_uniq<ListTablesBindData>(sparkClient, params);

	// Initialize the names and return types from analyze plan
	auto columns = bind_data->spark_client->AnalyzePlanSchema(bind_data->list_tables_plan);
	for (const auto &column : columns) {
		names.push_back(column.name);
		return_types.push_back(column.type);
	}
	return std::move(bind_data);
}

SparkListTablesFunction::SparkListTablesFunction()
    : TableFunction("spark_list_tables", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR},
                    SparkListTablesFunc, SparkListTablesBind, SparkListTablesGlobalState) {
	named_parameters["endpoint"] = LogicalType::VARCHAR;
	named_parameters["pattern"] = LogicalType::VARCHAR;
	named_parameters["db_name"] = LogicalType::VARCHAR;
}
} // namespace spark
} // namespace duckdb
