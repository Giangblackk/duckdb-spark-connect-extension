#include "spark_list_catalogs.hpp"

#include "duckdb/common/helper.hpp"
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

// GlobalFunctionState to store arrow Reader or record batches
struct ListCatalogsGlobalFunctionState : public GlobalTableFunctionState {
	explicit ListCatalogsGlobalFunctionState() {
	}
	arrow::RecordBatchVector batches;
	mutex lock;
};

// Parameters for Table Function
struct ListCatalogsParams {
	std::string pattern;
};

// Table Function Data
struct ListCatalogsBindData : public TableFunctionData {
	explicit ListCatalogsBindData(std::shared_ptr<SparkGRPCClient> &spark_client, ListCatalogsParams &params)
	    : spark_client(spark_client), params(params) {
		// build Spark gRPC Plan
		list_catalogs_plan = spark_client->PlanListCatalogs(params.pattern);
	}
	std::shared_ptr<SparkGRPCClient> spark_client;
	::spark::connect::Plan list_catalogs_plan;
	ListCatalogsParams params;
};

static void SparkListCatalogsFunc(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->CastNoConst<ListCatalogsBindData>();
	auto &global_state = data_p.global_state->Cast<ListCatalogsGlobalFunctionState>();
	auto spark_client = bind_data.spark_client;

	// lock for exclusivity in get next from stream/batches
	std::lock_guard<mutex> guard(global_state.lock);
	// if batches are empty, just return
	if (global_state.batches.empty()) {
		return;
	}

	// if not, reset output and convert batch to data chunk
	output.Reset();

	// pop back to get next batch
	auto next_batch = global_state.batches.back();
	global_state.batches.pop_back();

	// copy RecordBatch to DataChunk
	WriteRecordBatchToDataChunk(context, next_batch, output);
}

static unique_ptr<GlobalTableFunctionState> SparkListCatalogsInitGlobalState(ClientContext &context,
                                                                             TableFunctionInitInput &input) {
	auto state = make_uniq<ListCatalogsGlobalFunctionState>();
	auto &bind_data = input.bind_data->CastNoConst<ListCatalogsBindData>();
	auto spark_client = bind_data.spark_client;

	state->batches = spark_client->GetRecordBatches(bind_data.list_catalogs_plan);
	// global state should keep the RecordBatchStreamReader or RecordBachVector
	// local state if needed, should be used to handle when a record batch is bigger than maximum data chunk size to
	// split into multiple data chunks if needed
	// schema should be setup in bind stage, to return column names and data types
	return std::move(state);
}

static unique_ptr<FunctionData> SparkListCatalogsBind(ClientContext &context, TableFunctionBindInput &input,
                                                      vector<LogicalType> &return_types, vector<string> &names) {
	// Get parameters
	auto endpoint = input.inputs[0].GetValue<string>();
	auto pattern = input.inputs[1].GetValue<string>();
	ListCatalogsParams params;
	params.pattern = pattern;

	// Get existing Spark gRPC Client or create new for this endpoint
	auto sparkClient = SparkGRPCClient::GetOrCreateSparkClient(context, endpoint);

	// Add Spark gRPC client to table function data
	unique_ptr<ListCatalogsBindData> bind_data = make_uniq<ListCatalogsBindData>(sparkClient, params);

	// Initialize the names and return types from analyze plan
	auto columns = bind_data->spark_client->AnalyzePlanSchema(bind_data->list_catalogs_plan);
	for (const auto &column : columns) {
		names.emplace_back(column.name);
		return_types.emplace_back(column.type);
	}
	return std::move(bind_data);
}

SparkListCatalogsFunction::SparkListCatalogsFunction()
    : TableFunction("spark_list_catalogs", {LogicalType::VARCHAR, LogicalType::VARCHAR}, SparkListCatalogsFunc,
                    SparkListCatalogsBind, SparkListCatalogsInitGlobalState) {
	named_parameters["endpoint"] = LogicalType::VARCHAR;
	named_parameters["pattern"] = LogicalType::VARCHAR;
};
} // namespace spark
} // namespace duckdb
