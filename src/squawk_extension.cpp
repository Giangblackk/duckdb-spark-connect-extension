#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/execution/expression_executor_state.hpp"
#define DUCKDB_EXTENSION_MAIN

#include "squawk_extension.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>

namespace duckdb {

inline void squawkScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "squawk " + name.GetString() + " 🐥");
	});
}

inline void squawkgRPCVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "squawk " + name.GetString() + ", my linked gRPC version is " +
		                                           grpc_version_string());
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	// Register a scalar function
	auto squawk_scalar_function = ScalarFunction("squawk", {LogicalType::VARCHAR}, LogicalType::VARCHAR, squawkScalarFun);
	loader.RegisterFunction(squawk_scalar_function);

	// register another scala function
	auto squawk_grpc_version_scalar_function =
	    ScalarFunction("squawk_grpc_version", {LogicalType::VARCHAR}, LogicalType::VARCHAR, squawkgRPCVersionScalarFun);
	loader.RegisterFunction(squawk_grpc_version_scalar_function);
}

void SquawkExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string SquawkExtension::Name() {
	return "squawk";
}

std::string SquawkExtension::Version() const {
#ifdef EXT_VERSION_QUACK
	return EXT_VERSION_QUACK;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(squawk, loader) {
	duckdb::LoadInternal(loader);
}
}
