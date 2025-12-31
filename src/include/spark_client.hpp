#pragma once

#include "spark/connect/base.grpc.pb.h"
#include <arrow/record_batch.h>
#include <arrow/table.h>
#include <arrow/type_fwd.h>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <memory>
#include <string>

namespace duckdb {
namespace spark {
class SparkGRPCClient {
public:
	explicit SparkGRPCClient(const std::string &uri);
	arrow::RecordBatchVector GetCatalogs(const std::string &pattern);
	~SparkGRPCClient() {};

private:
	std::shared_ptr<grpc::Channel> channel;
	std::unique_ptr<::spark::connect::SparkConnectService::Stub> stub_;
	std::string session_id;
};

} // namespace spark
} // namespace duckdb