#pragma once

#include "myproto/sample.grpc.pb.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/status.h>
#include <memory>

namespace duckdb {

class SamplegRPCClient {
public:
	SamplegRPCClient();
	std::string SendRequest(const std::string &message);
	~SamplegRPCClient() {};

private:
	std::shared_ptr<grpc::Channel> channel;
	std::unique_ptr<::spark::SampleService::Stub> stub_;
};

} // namespace duckdb
