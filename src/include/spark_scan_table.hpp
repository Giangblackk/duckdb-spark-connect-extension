#pragma once

#include "duckdb/function/table_function.hpp"
#include "spark_client.hpp"
namespace duckdb {
namespace spark {

struct ScanTableParams {
	std::string table_name;
};

struct ScanTableBindData : public TableFunctionData {
	explicit ScanTableBindData(shared_ptr<SparkGRPCClient> &spark_client, ScanTableParams &params)
	    : spark_client(spark_client), params(params) {
		// build Spark gRPC Plan
		scan_table_plan = spark_client->PlanReadTable(params.table_name);
	}
	shared_ptr<SparkGRPCClient> spark_client;
	::spark::connect::Plan scan_table_plan;
	ScanTableParams params;
	ScanTableBindData(const ScanTableBindData &) = delete;
	ScanTableBindData &operator=(const ScanTableBindData &) = delete;
};

struct ScanTableGlobalFunctionState : public GlobalTableFunctionState {
	explicit ScanTableGlobalFunctionState() {
	}
	arrow::RecordBatchVector batches;
	mutex lock;
};

class SparkScanTableFunction : public TableFunction {
public:
	SparkScanTableFunction();

private:
	static unique_ptr<FunctionData> SparkScanTableBind(ClientContext &context, TableFunctionBindInput &input,
	                                                   vector<LogicalType> &return_types, vector<string> &names);
	static unique_ptr<GlobalTableFunctionState> SparkScanTableInitGlobal(ClientContext &context,
	                                                                     TableFunctionInitInput &input);
	static void SparkScanTableExecute(ClientContext &ctx, TableFunctionInput &data_p, DataChunk &output);
};
} // namespace spark
} // namespace duckdb
