#include "spark_scan_table.hpp"

namespace duckdb {
namespace spark {

void SparkScanTableFunction::SparkScanTableExecute(ClientContext &context, TableFunctionInput &data_p,
                                                   DataChunk &output) {
	auto &bind_data = data_p.bind_data->CastNoConst<ScanTableBindData>();
	auto &global_state = data_p.global_state->Cast<ScanTableGlobalFunctionState>();
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

unique_ptr<GlobalTableFunctionState> SparkScanTableFunction::SparkScanTableInitGlobal(ClientContext &context,
                                                                                      TableFunctionInitInput &input) {
	auto state = make_uniq<ScanTableGlobalFunctionState>();
	auto &bind_data = input.bind_data->CastNoConst<ScanTableBindData>();
	auto spark_client = bind_data.spark_client;

	state->batches = spark_client->GetRecordBatches(bind_data.scan_table_plan);
	// global state should keep the RecordBatchStreamReader or RecordBachVector
	// local state if needed, should be used to handle when a record batch is bigger than maximum data chunk size to
	// split into multiple data chunks if needed
	// schema should be setup in bind stage, to return column names and data types
	return std::move(state);
	return nullptr;
}

unique_ptr<FunctionData> SparkScanTableFunction::SparkScanTableBind(ClientContext &context,
                                                                    TableFunctionBindInput &input,
                                                                    vector<LogicalType> &return_types,
                                                                    vector<string> &names) {
	// // Get parameters
	auto endpoint = input.named_parameters["endpoint"].GetValue<std::string>();
	auto table_name = input.named_parameters["table_name"].GetValue<std::string>();
	ScanTableParams params;
	params.table_name = table_name;

	// // Get existing Spark gRPC Client or create new for this endpoint
	auto sparkClient = SparkGRPCClient::GetOrCreateSparkClient(context, endpoint);

	// Add Spark gRPC client to table function data
	unique_ptr<ScanTableBindData> bind_data = make_uniq<ScanTableBindData>(sparkClient, params);

	// Initialize the names and return types from analyze plan
	auto columns = bind_data->spark_client->AnalyzePlanSchema(bind_data->scan_table_plan);
	for (const auto &column : columns) {
		names.push_back(column.name);
		return_types.push_back(column.type);
	}
	return std::move(bind_data);
}

SparkScanTableFunction::SparkScanTableFunction()
    : TableFunction("spark_scan_table", {}, SparkScanTableExecute, SparkScanTableBind, SparkScanTableInitGlobal) {
	named_parameters["endpoint"] = LogicalType::VARCHAR;
	named_parameters["table_name"] = LogicalType::VARCHAR;
}

} // namespace spark
} // namespace duckdb
