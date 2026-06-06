#include "spark_table_entry.hpp"

#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/storage/table_storage_info.hpp"
#include "spark_catalog.hpp"
#include "spark_scan_table.hpp"

namespace duckdb {
namespace spark {
SparkTableEntry::SparkTableEntry(Catalog &catalog, SchemaCatalogEntry &schema, CreateTableInfo &info)
    : TableCatalogEntry(catalog, schema, info), catalog_(catalog) {
	this->internal = false;
}

virtual_column_map_t SparkTableEntry::GetVirtualColumns() const {
	virtual_column_map_t v_cols;
	return v_cols;
};
unique_ptr<BaseStatistics> SparkTableEntry::GetStatistics(ClientContext &context, column_t column_id) {
	return nullptr;
};
TableFunction SparkTableEntry::GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) {
	throw InternalException("SparkTableEntry::GetScanFunction called without entry lookup info");
};
TableFunction SparkTableEntry::GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data,
                                               const EntryLookupInfo &lookup) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto spark_client = spark_catalog.spark_client;
	auto params = ScanTableParams();
	params.table_name = schema.name + "." + lookup.GetEntryName();
	unique_ptr<SparkScanTableBindData> result = make_uniq<SparkScanTableBindData>(spark_client, params);

	// get arrow schema from analyzing plan, export and populate shema to attributes
	auto arrow_shema = result->spark_client->AnalyzePlanToArrowSchema(result->scan_table_plan);
	auto status = arrow::ExportSchema(*std::move(arrow_shema), &result->schema_root.arrow_schema);
	if (!status.ok()) {
		throw BinderException("Arrow schema export failed: " + status.ToString());
	}
	ArrowTableFunction::PopulateArrowTableSchema(duckdb::DBConfig::GetConfig(context), result->arrow_table,
	                                             result->schema_root.arrow_schema);

	bind_data = std::move(result);
	auto function = SparkScanTableFunction();
	return function;
};
TableStorageInfo SparkTableEntry::GetStorageInfo(ClientContext &context) {
	return TableStorageInfo();
};
unique_ptr<SparkTableEntry> SparkTableEntry::AlterEntryDirect(ClientContext &context, AlterInfo &info) {
	return nullptr;
}

} // namespace spark
} // namespace duckdb
