#pragma once

#ifndef GRPC_CLIENT_H
#define GRPC_CLIENT_H

#include <memory>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/status.h>

#include "myproto/sample.grpc.pb.h"

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

#endif