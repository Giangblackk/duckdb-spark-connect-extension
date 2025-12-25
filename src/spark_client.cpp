#include "spark_client.hpp"
#include "spark/connect/base.pb.h"

#include <arrow/io/interfaces.h>
#include <arrow/io/type_fwd.h>
#include <arrow/ipc/api.h>
#include <arrow/ipc/message.h>
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include <arrow/result.h>
#include <arrow/io/memory.h>
#include <arrow/buffer.h>
#include <arrow/result.h>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/status.h>

#include <memory>
#include <string>

namespace duckdb {
namespace spark {

SparkGRPCClient::SparkGRPCClient(const std::string &uri)
    : channel(grpc::CreateChannel(uri, grpc::InsecureChannelCredentials())),
      stub_(::spark::connect::SparkConnectService::NewStub(channel)) {};

std::string SparkGRPCClient::GetCatalogs(const std::string &pattern) {
	// setup ListCatalogs
	::spark::connect::ListCatalogs lc;
	lc.set_pattern(pattern);

	// setup Catalog
	::spark::connect::Catalog c;
	c.mutable_list_catalogs()->MergeFrom(lc);

	// setup Relation
	::spark::connect::Relation r;
	r.mutable_catalog()->MergeFrom(c);

	// setup RelationCommon
	::spark::connect::RelationCommon rc;
	rc.set_plan_id(1);
	rc.set_source_info("");
	r.mutable_common()->MergeFrom(rc);

	// setup Plan
	::spark::connect::Plan p;
	p.mutable_root()->MergeFrom(r);

	// setup ExecutePlanRequest
	::spark::connect::ExecutePlanRequest request;
	request.set_session_id("00112233-4455-6677-8899-aabbccddeeff");
	request.set_operation_id("00112233-4455-6677-8899-aabbccddeeff");
	request.set_client_type("duckdb");
	request.mutable_plan()->MergeFrom(p);

	// setup UserContext
	::spark::connect::UserContext uc;
	uc.set_user_id("00112233-4455-6677-8899-aabbccddeeff");
	uc.set_user_name("giangblackk");
	request.mutable_user_context()->MergeFrom(uc);

	// execute plan
	grpc::ClientContext context;
	auto stream = stub_->ExecutePlan(&context, request);
	::spark::connect::ExecutePlanResponse msg;
	std::string batch_str = "";
	// process through stream of messages
	while (stream->Read(&msg)) {
		auto response_type = msg.response_type_case();

		// if message is Arrow BatchRecord, parse it
		if (response_type == ::spark::connect::ExecutePlanResponse::kArrowBatch) {
			// read data from response
			auto data = msg.arrow_batch().data();

			// convert to buffer
			auto buffer = std::make_shared<arrow::Buffer>(reinterpret_cast<const uint8_t *>(data.data()), data.size());

			// conver to BufferReader
			auto input_stream = std::make_shared<arrow::io::BufferReader>(buffer);

			// create RecordBatchStreamReader to parse Buffer
			auto _result = arrow::ipc::RecordBatchStreamReader::Open(input_stream);
			// handle result of stream reader, if not ok, continue to next message in stream
			if (!_result.ok()) {
				continue;
			}
			auto reader = std::move(_result).ValueUnsafe();

			// read record batches from reader
			std::shared_ptr<arrow::RecordBatch> batch;
			while (reader->ReadNext(&batch).ok() && batch) {
				// Process the deserialized RecordBatch
				batch_str = batch->ToString();
			}
		}
	}
	grpc::Status status = stream->Finish();
	if (!status.ok()) {
		return "ExecutePlan rpc failed.";
	} else {
		if (batch_str.size() == 0) {
			return "<empty>";
		} else {
			return batch_str;
		}
	}
}

} // namespace spark
} // namespace duckdb