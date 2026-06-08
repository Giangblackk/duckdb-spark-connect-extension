#pragma once

#include "duckdb/common/shared_ptr.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/client_context_state.hpp"
#include "spark/connect/base.grpc.pb.h"
#include "spark/connect/base.pb.h"
#include "spark/connect/commands.pb.h"
#include "spark/connect/types.pb.h"
#include "spark_utils.hpp"

#include <arrow/record_batch.h>
#include <arrow/result.h>
#include <arrow/table.h>
#include <arrow/type.h>
#include <arrow/type_fwd.h>
#include <cstdint>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/sync_stream.h>
#include <memory>
#include <string>
#include <vector>

namespace duckdb {
namespace spark {

#define DEFAULT_TIMEOUT_SEC 5

// Stateful stream holder that maintains the gRPC stream across iterations
struct SparkStreamState {
	// keep the gRPC context and client reader alive until the end of the iterator batch reader
	std::unique_ptr<grpc::ClientContext> context;
	std::unique_ptr<grpc::ClientReader<::spark::connect::ExecutePlanResponse>> stream;
};

class SparkGRPCClient {
public:
	explicit SparkGRPCClient(const std::string &endpoint);
	grpc::Status SetConfigs(const std::map<std::string, std::string> &configs);
	::spark::connect::Plan PlanListCatalogs(const std::string &pattern);
	::spark::connect::Plan PlanListDatabases(const std::string &pattern);
	::spark::connect::Plan PlanListTables(const std::string &pattern, const std::string &db_name);
	::spark::connect::Plan PlanSetCurrentCatalog(const std::string &catalog_name);
	::spark::connect::Plan PlanReadTable(const std::string &table_name);
	::spark::connect::Plan PlanExecuteSQLQuery(const std::string &sql_string);
	::spark::connect::Plan PlanExecuteSQLCommand(const std::string &sql_string);
	::spark::connect::Plan PlanCreateTable(const std::string &schema_name, const std::string &table_name,
	                                       ::spark::connect::DataType &table_schema);
	::spark::connect::Plan PlanWriteOperationV2(const std::string &schema_name, const std::string &table_name,
	                                            const ::spark::connect::WriteOperationV2::Mode save_mode,
	                                            const char *data, const size_t data_size);
	arrow::RecordBatchVector GetRecordBatches(::spark::connect::Plan &plan);

	std::shared_ptr<SparkStreamState> GetSparkStreamState(::spark::connect::Plan &plan);
	static arrow::Result<std::shared_ptr<arrow::RecordBatch>>
	IterateSparkSteamState(const std::shared_ptr<SparkStreamState> &state);

	grpc::Status GetStatus(::spark::connect::Plan &plan);
	std::vector<ColumnInfo> AnalyzePlanToColumnInfo(::spark::connect::Plan &plan);
	std::shared_ptr<arrow::Schema> AnalyzePlanToArrowSchema(::spark::connect::Plan &plan);
	~SparkGRPCClient() {};
	static shared_ptr<SparkGRPCClient> GetOrCreateSparkClient(ClientContext &context, const std::string &endpoint);

private:
	std::shared_ptr<grpc::Channel> channel;
	std::unique_ptr<::spark::connect::SparkConnectService::Stub> stub_;
	std::string session_id;
	int64_t next_plan_id = 1;
};

class SparkClientState : public ClientContextState {
public:
	explicit SparkClientState(shared_ptr<SparkGRPCClient> &client) : spark_client(std::move(client)) {
	}
	explicit SparkClientState(const std::string &endpoint) : spark_client(std::make_shared<SparkGRPCClient>(endpoint)) {
	}

	shared_ptr<SparkGRPCClient> spark_client;
};

} // namespace spark
} // namespace duckdb
