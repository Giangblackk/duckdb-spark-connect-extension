#pragma once

#include "duckdb/common/types.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/client_context_state.hpp"
#include "spark/connect/base.grpc.pb.h"
#include "spark/connect/base.pb.h"
#include "spark_utils.hpp"

#include <arrow/record_batch.h>
#include <arrow/table.h>
#include <arrow/type_fwd.h>
#include <cstdint>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/support/status.h>
#include <memory>
#include <string>
#include <vector>

namespace duckdb {
namespace spark {

#define DEFAULT_TIMEOUT_SEC 5

class SparkGRPCClient {
public:
	explicit SparkGRPCClient(const std::string &endpoint);
	::spark::connect::Plan PlanListCatalogs(const std::string &pattern);
	::spark::connect::Plan PlanListDatabases(const std::string &pattern);
	::spark::connect::Plan PlanListTables(const std::string &pattern, const std::string &db_name);
	::spark::connect::Plan PlanSetCurrentCatalog(const std::string &catalog_name);
	arrow::RecordBatchVector GetRecordBatches(::spark::connect::Plan &plan);

	grpc::Status GetStatus(::spark::connect::Plan &plan);
	std::vector<ColumnInfo> AnalyzePlanSchema(::spark::connect::Plan &plan);
	~SparkGRPCClient() {};
	static std::shared_ptr<SparkGRPCClient> GetOrCreateSparkClient(ClientContext &context, const std::string &endpoint);

private:
	std::shared_ptr<grpc::Channel> channel;
	std::unique_ptr<::spark::connect::SparkConnectService::Stub> stub_;
	std::string session_id;
	int64_t next_plan_id = 1;
};

class SparkClientState : public ClientContextState {
public:
	explicit SparkClientState(std::shared_ptr<SparkGRPCClient> &client) : spark_client(std::move(client)) {
	}
	explicit SparkClientState(const std::string &endpoint) : spark_client(std::make_shared<SparkGRPCClient>(endpoint)) {
	}

	std::shared_ptr<SparkGRPCClient> spark_client;
};

} // namespace spark
} // namespace duckdb
