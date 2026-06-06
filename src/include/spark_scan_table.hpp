#pragma once

#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/common/shared_ptr.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/execution/execution_context.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "spark_arrow_reader.hpp"
#include "spark_client.hpp"

#include <arrow/c/bridge.h>
namespace duckdb {
namespace spark {

struct ScanTableParams {
	std::string table_name;
};

struct SparkScanTableBindData : public ArrowScanFunctionData {
	// default constructor
	explicit SparkScanTableBindData(shared_ptr<SparkGRPCClient> &spark_client, ScanTableParams &params)
	    : ArrowScanFunctionData(&SparkStreamFactory::Produce, 0, make_shared_ptr<FactoryDependency>(nullptr)),
	      spark_client(spark_client), params(params) {
		// build Spark gRPC Plan
		scan_table_plan = spark_client->PlanReadTable(params.table_name);
	}

	shared_ptr<FactoryDependency> GetFactoryDependency() {
		return dependency ? shared_ptr_cast<DependencyItem, FactoryDependency>(dependency) : nullptr;
	}

	SparkScanTableBindData(const SparkScanTableBindData &) = delete;
	SparkScanTableBindData &operator=(const SparkScanTableBindData &) = delete;

	shared_ptr<SparkGRPCClient> spark_client;
	::spark::connect::Plan scan_table_plan;
	ScanTableParams params;
};

struct SparkScanTableGlobalState : public ArrowScanGlobalState {
	mutex lock;
	atomic<idx_t> position = 0;
};

struct SparkScanTableLocalState : public ArrowScanLocalState {
	explicit SparkScanTableLocalState(unique_ptr<ArrowArrayWrapper> current_chunk, ClientContext &context)
	    : ArrowScanLocalState(std::move(current_chunk), context) {
	}
};

class SparkScanTableFunction : public TableFunction {
public:
	SparkScanTableFunction();

private:
	static unique_ptr<FunctionData> SparkScanTableBind(ClientContext &context, TableFunctionBindInput &input,
	                                                   vector<LogicalType> &return_types, vector<string> &names);
	static unique_ptr<GlobalTableFunctionState> SparkScanTableInitGlobal(ClientContext &context,
	                                                                     TableFunctionInitInput &input);
	static unique_ptr<LocalTableFunctionState> SparkScanTableInitLocal(ExecutionContext &context,
	                                                                   TableFunctionInitInput &input,
	                                                                   GlobalTableFunctionState *global_state_p);
	static void SparkScanTableExecute(ClientContext &ctx, TableFunctionInput &data_p, DataChunk &output);
};
} // namespace spark
} // namespace duckdb
