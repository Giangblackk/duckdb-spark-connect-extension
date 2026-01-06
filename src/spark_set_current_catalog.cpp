#include "spark_set_current_catalog.hpp"

#include "duckdb/common/helper.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "spark/connect/base.pb.h"
#include "spark_client.hpp"

#include <arrow/type_fwd.h>

namespace duckdb {
namespace spark {

struct SetCurrentCatalogGlobalFunctionState : public GlobalTableFunctionState {
	explicit SetCurrentCatalogGlobalFunctionState() {
	}
	arrow::RecordBatchVector batches;
	mutex lock;
};

struct SetCurrentCatalogParams {
	std::string catalog_name;
};

struct SetCurrentCatalogBindData : public TableFunctionData {
	explicit SetCurrentCatalogBindData(std::shared_ptr<SparkGRPCClient> &spark_client, SetCurrentCatalogParams &params)
	    : spark_client(spark_client), params(params) {
		// build Spark gRPC Plan
		set_current_catalog_plan = spark_client->PlanSetCurrentCatalog(params.catalog_name);
	}
	std::shared_ptr<SparkGRPCClient> spark_client;
	::spark::connect::Plan set_current_catalog_plan;
	SetCurrentCatalogParams params;
};

static void SparkSetCurrentCatalogFunc(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->CastNoConst<SetCurrentCatalogBindData>();
	auto &global_state = data_p.global_state->Cast<SetCurrentCatalogGlobalFunctionState>();
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

static unique_ptr<GlobalTableFunctionState> SparkSetCurrentCatalogGlobalState(ClientContext &context,
                                                                              TableFunctionInitInput &input) {
	auto state = make_uniq<SetCurrentCatalogGlobalFunctionState>();
	auto &bind_data = input.bind_data->CastNoConst<SetCurrentCatalogBindData>();
	auto spark_client = bind_data.spark_client;

	state->batches = spark_client->GetRecordBatches(bind_data.set_current_catalog_plan);
	// global state should keep the RecordBatchStreamReader or RecordBachVector
	// local state if needed, should be used to handle when a record batch is bigger than maximum data chunk size to
	// split into multiple data chunks if needed
	// schema should be setup in bind stage, to return column names and data types
	return std::move(state);
}

static unique_ptr<FunctionData> SparkSetCurrentCatalogBind(ClientContext &context, TableFunctionBindInput &input,
                                                           vector<LogicalType> &return_types, vector<string> &names) {
	// Get parameters
	auto endpoint = input.inputs[0].GetValue<string>();
	auto catalog_name = input.inputs[1].GetValue<string>();
	SetCurrentCatalogParams params;
	params.catalog_name = catalog_name;

	// Get existing Spark gRPC Client or create new for this endpoint
	auto sparkClient = SparkGRPCClient::GetOrCreateSparkClient(context, endpoint);

	// Add Spark gRPC client to table function data
	unique_ptr<SetCurrentCatalogBindData> bind_data = make_uniq<SetCurrentCatalogBindData>(sparkClient, params);

	// Initialize the names and return types from analyze plan
	auto columns = bind_data->spark_client->AnalyzePlanSchema(bind_data->set_current_catalog_plan);
	for (const auto &column : columns) {
		names.emplace_back(column.name);
		return_types.emplace_back(column.type);
	}
	return std::move(bind_data);
}

SparkSetCurrentCatalogFunction::SparkSetCurrentCatalogFunction()
    : TableFunction("spark_set_catalog", {LogicalType::VARCHAR, LogicalType::VARCHAR}, SparkSetCurrentCatalogFunc,
                    SparkSetCurrentCatalogBind, SparkSetCurrentCatalogGlobalState) {
	named_parameters["endpoint"] = LogicalType::VARCHAR;
	named_parameters["catalog_name"] = LogicalType::VARCHAR;
}
} // namespace spark
} // namespace duckdb
