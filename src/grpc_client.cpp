#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/status.h>
#include <string>
#include "myproto/sample.grpc.pb.h"
#include "myproto/sample.pb.h"

#include "include/grpc_client.hpp"

namespace duckdb {

SamplegRPCClient::SamplegRPCClient()
    : channel(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials())),
      stub_(spark::SampleService::NewStub(channel)) {
}
std::string SamplegRPCClient::SendRequest(const std::string &message) {
	spark::SampleRequest request;
	spark::SampleResponse response;
	request.set_request_sample_field(message);
	grpc::ClientContext context;
	grpc::Status status = stub_->SampleMethod(&context, request, &response);
	if (!status.ok()) {
		return "SampleMethod rpc failed";
	} else if (response.response_sample_field().empty()) {
		return "<empty>";
	} else {
		return response.response_sample_field();
	}
}

} // namespace duckdb
