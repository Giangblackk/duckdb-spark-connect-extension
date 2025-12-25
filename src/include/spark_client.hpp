#pragma once

#include "spark/connect/base.grpc.pb.h"
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <string>

namespace duckdb {
namespace spark {
class SparkGRPCClient {
public:
	explicit SparkGRPCClient(const std::string &uri);
	std::string GetCatalogs(const std::string &pattern);
	~SparkGRPCClient() {};

private:
	std::shared_ptr<grpc::Channel> channel;
	std::unique_ptr<::spark::connect::SparkConnectService::Stub> stub_;
};

} // namespace spark
} // namespace duckdb