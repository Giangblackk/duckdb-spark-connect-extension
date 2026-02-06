#include "spark_client.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/types.hpp"
#include "spark/connect/base.pb.h"
#include "spark/connect/catalog.pb.h"
#include "spark/connect/relations.pb.h"
#include "spark/connect/types.pb.h"

#include <arrow/buffer.h>
#include <arrow/io/interfaces.h>
#include <arrow/io/memory.h>
#include <arrow/io/type_fwd.h>
#include <arrow/ipc/api.h>
#include <arrow/ipc/message.h>
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>
#include <arrow/record_batch.h>
#include <arrow/status.h>
#include <arrow/type_fwd.h>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/status.h>
#include <memory>
#include <string>
#include <vector>

namespace duckdb {
namespace spark {

SparkGRPCClient::SparkGRPCClient(const std::string &uri)
    : channel(grpc::CreateChannel(uri, grpc::InsecureChannelCredentials())),
      stub_(::spark::connect::SparkConnectService::NewStub(channel)), session_id(generate_uuid()) {
	// Check gRPC connectivity state
	// try to connect
	auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(DEFAULT_TIMEOUT_SEC);

	if (!channel->WaitForConnected(deadline)) {
		throw ConnectionException("Failed to connect to endpoint `%s` within timeout `%i` seconds", uri,
		                          DEFAULT_TIMEOUT_SEC);
	}
};

::spark::connect::Plan SparkGRPCClient::PlanListCatalogs(const std::string &pattern) {
	// setup ListCatalogs
	::spark::connect::ListCatalogs lc;
	lc.set_pattern(pattern);

	// setup Catalog
	::spark::connect::Catalog c;
	c.mutable_list_catalogs()->CopyFrom(lc);

	// setup Relation
	::spark::connect::Relation r;
	r.mutable_catalog()->CopyFrom(c);

	// setup RelationCommon
	::spark::connect::RelationCommon rc;
	rc.set_plan_id(next_plan_id++);
	rc.set_source_info("");
	r.mutable_common()->CopyFrom(rc);

	// setup Plan
	::spark::connect::Plan p;
	p.mutable_root()->CopyFrom(r);
	return p;
}

::spark::connect::Plan SparkGRPCClient::PlanListDatabases(const std::string &pattern) {
	// setup ListDatabases
	::spark::connect::ListDatabases ld;
	ld.set_pattern(pattern);

	// setup Catalog
	::spark::connect::Catalog c;
	c.mutable_list_databases()->CopyFrom(ld);

	// setup Relation
	::spark::connect::Relation r;
	r.mutable_catalog()->CopyFrom(c);

	// setup RelationCommon
	::spark::connect::RelationCommon rc;
	rc.set_plan_id(next_plan_id++);
	rc.set_source_info("");
	r.mutable_common()->CopyFrom(rc);

	// setup Plan
	::spark::connect::Plan p;
	p.mutable_root()->CopyFrom(r);
	return p;
}

::spark::connect::Plan SparkGRPCClient::PlanListTables(const std::string &pattern, const std::string &db_name) {
	// setup ListDatabases
	::spark::connect::ListTables lt;
	lt.set_pattern(pattern);
	lt.set_db_name(db_name);

	// setup Catalog
	::spark::connect::Catalog c;
	c.mutable_list_tables()->CopyFrom(lt);

	// setup Relation
	::spark::connect::Relation r;
	r.mutable_catalog()->CopyFrom(c);

	// setup RelationCommon
	::spark::connect::RelationCommon rc;
	rc.set_plan_id(next_plan_id++);
	rc.set_source_info("");
	r.mutable_common()->CopyFrom(rc);

	// setup Plan
	::spark::connect::Plan p;
	p.mutable_root()->CopyFrom(r);
	return p;
}

::spark::connect::Plan SparkGRPCClient::PlanSetCurrentCatalog(const std::string &catalog_name) {
	// setup ListDatabases
	::spark::connect::SetCurrentCatalog scc;
	scc.set_catalog_name(catalog_name);

	// setup Catalog
	::spark::connect::Catalog c;
	c.mutable_set_current_catalog()->CopyFrom(scc);

	// setup Relation
	::spark::connect::Relation r;
	r.mutable_catalog()->CopyFrom(c);

	// setup RelationCommon
	::spark::connect::RelationCommon rc;
	rc.set_plan_id(next_plan_id++);
	rc.set_source_info("");
	r.mutable_common()->CopyFrom(rc);

	// setup Plan
	::spark::connect::Plan p;
	p.mutable_root()->CopyFrom(r);
	return p;
}

::spark::connect::Plan SparkGRPCClient::PlanReadTable(const std::string &table_name) {
	// setup Table
	::spark::connect::Read_NamedTable nt;
	nt.set_unparsed_identifier(table_name);

	::spark::connect::Read rd;
	rd.mutable_named_table()->CopyFrom(nt);

	::spark::connect::Relation rel;
	rel.mutable_read()->CopyFrom(rd);

	// setup RelationCommon
	::spark::connect::RelationCommon rc;
	rc.set_plan_id(next_plan_id++);
	rc.set_source_info("");
	rel.mutable_common()->CopyFrom(rc);

	// setup Plan
	::spark::connect::Plan p;
	p.mutable_root()->CopyFrom(rel);
	return p;
}

arrow::RecordBatchVector SparkGRPCClient::GetRecordBatches(::spark::connect::Plan &plan) {
	// setup ExecutePlanRequest
	::spark::connect::ExecutePlanRequest request;
	auto operation_id = generate_uuid();
	request.set_session_id(session_id);
	request.set_operation_id(operation_id);
	request.set_client_type("duckdb");
	request.mutable_plan()->CopyFrom(plan);

	// setup UserContext
	::spark::connect::UserContext uc;
	uc.set_user_name("duckdb");
	request.mutable_user_context()->CopyFrom(uc);

	// execute plan
	grpc::ClientContext context;
	auto stream = stub_->ExecutePlan(&context, request);
	::spark::connect::ExecutePlanResponse msg;
	std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
	// process through stream of messages
	while (stream->Read(&msg)) {
		auto response_type = msg.response_type_case();

		// if message is Arrow BatchRecord, parse it
		if (response_type == ::spark::connect::ExecutePlanResponse::kArrowBatch) {
			// read data from response
			auto data = msg.arrow_batch().data();

			// convert to buffer
			auto buffer = arrow::Buffer::FromString(data);

			// conver to BufferReader
			auto input_stream = std::make_shared<arrow::io::BufferReader>(buffer);

			// create RecordBatchStreamReader to parse Buffer
			auto _result = arrow::ipc::RecordBatchStreamReader::Open(input_stream);
			// handle result of stream reader, if not ok, continue to next message in stream
			if (!_result.ok()) {
				continue;
			}
			auto reader = _result.ValueOrDie();

			// read record batches from reader
			std::shared_ptr<arrow::RecordBatch> batch;
			while (reader->ReadNext(&batch).ok() && batch) {
				// Process the deserialized RecordBatch
				batches.push_back(batch);
			}
		}
	}
	return batches;
}

std::vector<ColumnInfo> SparkGRPCClient::AnalyzePlanSchema(::spark::connect::Plan &plan) {
	::spark::connect::AnalyzePlanRequest analyze_plan_request;
	auto operation_id = generate_uuid();
	analyze_plan_request.set_session_id(session_id);
	analyze_plan_request.set_client_type("duckdb");

	::spark::connect::UserContext uc;
	uc.set_user_name("duckdb");
	analyze_plan_request.mutable_user_context()->CopyFrom(uc);

	::spark::connect::AnalyzePlanRequest::Schema s;
	s.mutable_plan()->CopyFrom(plan);

	analyze_plan_request.mutable_schema()->CopyFrom(s);

	// execute plan
	grpc::ClientContext context;
	::spark::connect::AnalyzePlanResponse resp;
	auto status = stub_->AnalyzePlan(&context, analyze_plan_request, &resp);
	if (!status.ok()) {
		throw ConnectionException(status.error_message());
	}
	auto schema = resp.schema().schema();
	auto schema_kind = schema.kind_case();
	std::vector<ColumnInfo> columns;
	if (schema_kind == ::spark::connect::DataType::KindCase::kStruct) {
		auto data_struct = static_cast<::spark::connect::DataType::Struct>(schema.struct_());
		for (const auto &f : data_struct.fields()) {
			const auto &field_name = f.name();
			auto field_kind = f.data_type().kind_case();
			columns.emplace_back(field_name, ConvertSparkToDuckDBType(f.data_type()));
		}
	}
	return columns;
}

grpc::Status SparkGRPCClient::GetStatus(::spark::connect::Plan &plan) {
	::spark::connect::ExecutePlanRequest request;
	auto operation_id = generate_uuid();
	request.set_session_id(session_id);
	request.set_operation_id(operation_id);
	request.set_client_type("duckdb");
	request.mutable_plan()->CopyFrom(plan);

	// setup UserContext
	::spark::connect::UserContext uc;
	uc.set_user_name("duckdb");
	request.mutable_user_context()->CopyFrom(uc);

	// execute plan
	grpc::ClientContext context;
	auto stream = stub_->ExecutePlan(&context, request);
	::spark::connect::ExecutePlanResponse msg;
	// iterate over response messages
	while (stream->Read(&msg)) {
	}
	// get status
	grpc::Status status = stream->Finish();
	return status;
}

shared_ptr<SparkGRPCClient> SparkGRPCClient::GetOrCreateSparkClient(ClientContext &context,
                                                                    const std::string &endpoint) {
	const std::string state_key = "spark.client." + endpoint;
	auto spark_client_state = context.registered_state->GetOrCreate<SparkClientState>(state_key, endpoint);
	return spark_client_state->spark_client;
}

} // namespace spark
} // namespace duckdb
