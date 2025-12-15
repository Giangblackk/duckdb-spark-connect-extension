#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/execution/expression_executor_state.hpp"
#define DUCKDB_EXTENSION_MAIN

#include "spark_extension.hpp"
#include "grpc_client.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>

#define SPARK_EXTENSION_VERSION "0.0.1"

namespace duckdb {

inline void sparkScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "spark " + name.GetString() + " 🐥");
	});
}

inline void sparkgRPCVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "spark " + name.GetString() + ", my linked gRPC version is " +
		                                           grpc_version_string());
	});
}

inline void sparkgRPCSampleRequestScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		SamplegRPCClient gRPCClient;
		return StringVector::AddString(result, gRPCClient.SendRequest(name.GetString()));
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	// Register a scalar function
	auto spark_scalar_function =
	    ScalarFunction("spark", {LogicalType::VARCHAR}, LogicalType::VARCHAR, sparkScalarFun);
	loader.RegisterFunction(spark_scalar_function);

	// register another scala function
	auto spark_grpc_version_scalar_function =
	    ScalarFunction("spark_grpc_version", {LogicalType::VARCHAR}, LogicalType::VARCHAR, sparkgRPCVersionScalarFun);
	loader.RegisterFunction(spark_grpc_version_scalar_function);

	auto spark_grpc_sample_request_scalar_function = ScalarFunction(
	    "spark_grpc_sample_request", {LogicalType::VARCHAR}, LogicalType::VARCHAR, sparkgRPCSampleRequestScalarFun);
	loader.RegisterFunction(spark_grpc_sample_request_scalar_function);
}

void SparkExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string SparkExtension::Name() {
	return "spark";
}

std::string SparkExtension::Version() const {
	return SPARK_EXTENSION_VERSION;
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(spark, loader) {
	duckdb::LoadInternal(loader);
}
}
