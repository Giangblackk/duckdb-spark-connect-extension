#include "spark_list_catalogs.hpp"
#include <arrow/api.h>
#include <arrow/c/bridge.h>
#include "duckdb/common/arrow/arrow.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/shared_ptr.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/client_context.hpp"
#include "spark_client.hpp"
#include <arrow/type_fwd.h>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
namespace duckdb {
namespace spark {

struct ListCatalogsGlobalFunctionState : public GlobalTableFunctionState {
	explicit ListCatalogsGlobalFunctionState() {
	}
	arrow::RecordBatchVector batches;
	mutex lock;
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

duckdb::unique_ptr<duckdb::ArrowType> GetArrowType(duckdb::DBConfig &config, ArrowSchema &schema_item) {
	auto arrow_type = ArrowType::GetArrowLogicalType(config, schema_item);

	if (schema_item.dictionary) {
		auto dictionary_type = ArrowType::GetArrowLogicalType(config, *schema_item.dictionary);
		arrow_type->SetDictionary(std::move(dictionary_type));
	}
	return arrow_type;
}

struct ArrowTableSchema {
public:
	void AddColumn(idx_t index, shared_ptr<ArrowType> type, const string &name) {
		D_ASSERT(arrow_convert_data.find(index) == arrow_convert_data.end());
		arrow_convert_data.emplace(std::make_pair(index, std::move(type)));
	}
	const arrow_column_map_t GetColumns() const {
		return arrow_convert_data;
	}

private:
	arrow_column_map_t arrow_convert_data;
};

static void SparkListCatalogsFunc(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->CastNoConst<ListCatalogsBindData>();
	auto &global_state = data_p.global_state->Cast<ListCatalogsGlobalFunctionState>();
	auto spark_client = bind_data.spark_client;
	auto &config = DBConfig::GetConfig(context);

	// lock for exclusivity in get next from stream/batches
	std::lock_guard<mutex> guard(global_state.lock);
	// if batches are empty, just return
	if (global_state.batches.empty()) {
		return;
	}
	// if not, reset output and convert batch to data chunk
	output.Reset();

	// pop back to get next batch
	auto next_batch = global_state.batches.back();
	global_state.batches.pop_back();

	// extract schema
	ArrowSchemaWrapper schema_root;
	auto schema_export_status = arrow::ExportSchema(*next_batch->schema(), &schema_root.arrow_schema);
	if (!schema_export_status.ok()) {
		throw ExecutorException("Failed to Export Arrow Schema");
	}

	// convert arrow types to duckdb types
	vector<LogicalType> all_types;
	std::unordered_map<std::string, idx_t> name_indexes;
	ArrowTableSchema arrow_table;

	for (auto col_index = 0; col_index < schema_root.arrow_schema.n_children; col_index++) {
		auto &schema_item = *schema_root.arrow_schema.children[col_index];

		auto arrow_type = GetArrowType(config, schema_item);

		all_types.push_back(arrow_type->GetDuckType());
		arrow_table.AddColumn(col_index, std::move(arrow_type), schema_item.name);
		name_indexes[schema_item.name] = col_index;
	}

	ArrowSchema c_schema;
	auto current_chunk = make_uniq<ArrowArrayWrapper>();

	auto batch_export_status = arrow::ExportRecordBatch(*next_batch, &current_chunk->arrow_array, &c_schema);
	if (!batch_export_status.ok()) {
		throw ExecutorException("Failed to Export Arrow RecordBatch");
	}

	output.SetCardinality(current_chunk->arrow_array.length);

	ArrowScanLocalState fake_local_state(std::move(current_chunk), context);

	ArrowTableFunction::ArrowToDuckDB(fake_local_state, arrow_table.GetColumns(), output, 0);

	output.Verify();
}

static unique_ptr<GlobalTableFunctionState> SparkListCatalogsInitGlobalState(ClientContext &context,
                                                                             TableFunctionInitInput &input) {
	auto state = make_uniq<ListCatalogsGlobalFunctionState>();
	auto &bind_data = input.bind_data->CastNoConst<ListCatalogsBindData>();
	auto spark_client = bind_data.spark_client;

	state->batches = spark_client->GetCatalogs(bind_data.params.pattern);
	// global state should keep the RecordBatchStreamReader or RecordBachVector
	// local state if needed, should be used to handle when a record batch is bigger than maximum data chunk size to
	// split into multiple data chunks if needed
	// schema should be setup in bind stage, to return column names and data types
	return std::move(state);
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
	return std::move(bind_data);
}

SparkListCatalogsFunction::SparkListCatalogsFunction()
    : TableFunction("spark_catalogs", {LogicalType::VARCHAR, LogicalType::VARCHAR}, SparkListCatalogsFunc,
                    SparkListCatalogsBind, SparkListCatalogsInitGlobalState) {
	named_parameters["uri"] = LogicalType::VARCHAR;
	named_parameters["pattern"] = LogicalType::VARCHAR;
};
} // namespace spark
} // namespace duckdb