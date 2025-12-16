#include "duckdb.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/connection.hpp"
#include "spark_attach.hpp"

namespace duckdb {
namespace spark {

static void AttachFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {};

static unique_ptr<FunctionData> AttachBind(ClientContext &context, TableFunctionBindInput &input,
                                           vector<LogicalType> &return_types, vector<string> &names) {};

SparkAttachFunction::SparkAttachFunction()
    : TableFunction("spark_attach", {LogicalType::VARCHAR}, AttachFunction, AttachBind) {
	named_parameters["source_schema"] = LogicalType::VARCHAR;
}
} // namespace spark
} // namespace duckdb