#include "storage/spark_schema_set.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/enums/on_create_conflict.hpp"
#include "duckdb/common/exception/catalog_exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "spark_catalog.hpp"
#include "spark_catalog_set.hpp"
#include "spark_schema_entry.hpp"

#include <arrow/array/array_binary.h>
#include <arrow/type.h>
#include <utility>
#include <vector>

namespace duckdb {
namespace spark {

SparkSchemaSet::SparkSchemaSet(Catalog &catalog) : SparkCatalogSet(catalog) {
}

std::string SparkSchemaSet::CreateSchemaInfoToSQL(const CreateSchemaInfo &info) {
	duckdb::stringstream ss;

	ss << "CREATE DATABASE ";

	if (info.on_conflict == OnCreateConflict::IGNORE_ON_CONFLICT) {
		ss << "IF NOT EXISTS ";
	}

	ss << info.schema;
	return ss.str();
}

optional_ptr<CatalogEntry> SparkSchemaSet::CreateSchema(ClientContext &context, CreateSchemaInfo &info) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto spark_client = spark_catalog.spark_client;
	auto sql_string = CreateSchemaInfoToSQL(info);
	auto plan = spark_client->PlanExecuteSQLCommand(sql_string);
	auto status = spark_client->GetStatus(plan);
	if (!status.ok()) {
		throw CatalogException("Fail to create Spark schema `%s` in catalog `%s`. SQL command: `%s`. Error: `%s`.",
		                       info.schema, info.catalog, sql_string, status.error_message());
	}
	auto schema_entry = make_uniq<SparkSchemaEntry>(catalog, info);
	return CreateEntry(std::move(schema_entry));
}

std::vector<CreateSchemaInfo> SparkSchemaSet::ParseRecordBatches(arrow::RecordBatchVector &batches) {
	// for each record batch, get schema, validate schema conversion compatibility before handling data
	// extract record batch data and cast to destination type
	std::vector<CreateSchemaInfo> schemas;
	for (auto &batch : batches) {
		auto schema = batch->schema();
		auto field_names = schema->field_names();
		// skip if batch doesn't have field `name` or `description`
		if (std::find(field_names.begin(), field_names.end(), "name") == field_names.end()) {
			continue;
		}
		if (std::find(field_names.begin(), field_names.end(), "description") == field_names.end()) {
			continue;
		}
		auto name_array = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("name"));
		auto description_array = std::static_pointer_cast<arrow::StringArray>(batch->GetColumnByName("description"));
		for (int64_t i = 0; i < batch->num_rows(); i++) {
			CreateSchemaInfo schema_info;
			if (!name_array->IsNull(i)) {
				schema_info.schema = name_array->GetString(i);
				if (!description_array->IsNull(i)) {
					schema_info.comment = description_array->GetString(i);
				}
				schema_info.internal = false;
				schema_info.temporary = false;
				schemas.push_back(schema_info);
			}
		}
	}
	return schemas;
}

void SparkSchemaSet::LoadEntries(DatabaseInstance &db) {
	if (called_load_entries) {
		return;
	}
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto spark_client = spark_catalog.spark_client;
	auto plan = spark_client->PlanListDatabases("*");
	auto data = spark_client->GetRecordBatches(plan);
	auto schemas = ParseRecordBatches(data);
	for (auto &schema_info : schemas) {
		auto schema_entry = make_uniq<SparkSchemaEntry>(catalog, schema_info);
		CreateEntry(std::move(schema_entry));
	}
	called_load_entries = true;
}
optional_ptr<CatalogEntry> SparkCatalogSet::CreateEntry(unique_ptr<CatalogEntry> entry) {
	auto result = entry.get();
	if (result->name.empty()) {
		throw CatalogException("SparkCatalogSet::CreateEntry called with empty name");
	}
	entries.insert(make_pair(result->name, std::move(entry)));
	return result;
}

} // namespace spark
} // namespace duckdb
