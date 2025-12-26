#include "spark_list_catalogs.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/shared_ptr.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "spark_client.hpp"
#include <string>
#include <vector>
namespace duckdb {
namespace spark {

struct ListCatalogsGlobalFunctionState : public GlobalTableFunctionState {
	explicit ListCatalogsGlobalFunctionState() {
	}
};

struct ListCatalogParams {
	std::string pattern;
};

struct ListCatalogsBindData : public TableFunctionData {
	explicit ListCatalogsBindData(shared_ptr<SparkGRPCClient> &spark_client, ListCatalogParams &params)
	    : spark_client(spark_client), params(params) {
	}
	shared_ptr<SparkGRPCClient> spark_client;
	ListCatalogParams params;
	bool finished = false;
};

void InitializeNamesAndReturnTypes(vector<LogicalType> &return_types, vector<string> &names) {
	struct ColumnInfo {
		ColumnInfo(const string &name, const LogicalTypeId type) : name(name), type(type) {
		}
		string name;
		LogicalTypeId type;
	};
	std::vector<ColumnInfo> columns = {
	    {"name", LogicalTypeId::VARCHAR},
	    {"description", LogicalTypeId::VARCHAR},
	};
	for (const auto &column : columns) {
		names.emplace_back(column.name);
		return_types.emplace_back(column.type);
	}
}
static void SparkListCatalogsFunc(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->CastNoConst<ListCatalogsBindData>();
	auto &global_state = data_p.global_state->Cast<ListCatalogsGlobalFunctionState>();
	auto spark_client = bind_data.spark_client;

	if (bind_data.finished) {
		return;
	}

	auto catalogs = spark_client->GetCatalogs(bind_data.params.pattern);
	output.SetValue(0, 0, Value("spark_catalog"));
	output.SetValue(1, 0, Value(catalogs));
	output.SetCardinality(1);

	bind_data.finished = true;
}

static unique_ptr<GlobalTableFunctionState> SparkListCatalogsInitGlobalState(ClientContext &context,
                                                                             TableFunctionInitInput &input) {
	auto state = make_uniq<ListCatalogsGlobalFunctionState>();
	return state;
}
static unique_ptr<FunctionData> SparkListCatalogsBind(ClientContext &context, TableFunctionBindInput &input,
                                                      vector<LogicalType> &return_types, vector<string> &names) {
	// Initialize the names and return types
	InitializeNamesAndReturnTypes(return_types, names);
	auto uri = input.inputs[0].GetValue<string>();
	auto pattern = input.inputs[1].GetValue<string>();
	ListCatalogParams params;
	params.pattern = pattern;
	auto sparkClient = make_shared_ptr<SparkGRPCClient>(uri);
	unique_ptr<ListCatalogsBindData> bind_data = make_uniq<ListCatalogsBindData>(sparkClient, params);
	return bind_data;
}

SparkListCatalogsFunction::SparkListCatalogsFunction()
    : TableFunction("spark_catalogs", {LogicalType::VARCHAR, LogicalType::VARCHAR}, SparkListCatalogsFunc,
                    SparkListCatalogsBind, SparkListCatalogsInitGlobalState) {
	named_parameters["uri"] = LogicalType::VARCHAR;
	named_parameters["pattern"] = LogicalType::VARCHAR;
};
} // namespace spark
} // namespace duckdb