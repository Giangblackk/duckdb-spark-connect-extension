#include "spark_table_set.hpp"

#include "duckdb.h"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/parser/column_definition.hpp"
#include "duckdb/parser/constraints/not_null_constraint.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "duckdb/planner/parsed_data/bound_create_table_info.hpp"
#include "spark_catalog.hpp"
#include "spark_catalog_set.hpp"
#include "spark_schema_entry.hpp"
#include "spark_table_entry.hpp"
#include "spark_transaction.hpp"
#include "spark_utils.hpp"

#include <arrow/array/array_binary.h>
namespace duckdb {
namespace spark {

optional_ptr<CatalogEntry> SparkTableSet::GetEntry(ClientContext &context, const EntryLookupInfo &lookup_info) {
	auto existing_entry = SparkCatalogSet::GetEntry(context, lookup_info);
	if (!existing_entry) {
		return nullptr;
	}

	auto &table_entry = existing_entry->Cast<SparkTableEntry>();
	auto &table_catalog = table_entry.catalog.Cast<SparkCatalog>();

	return table_entry;
}

optional_ptr<CatalogEntry> SparkTableSet::CreateTable(ClientContext &context, BoundCreateTableInfo &info) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto &spark_client = spark_catalog.spark_client;
	auto &base = info.base->Cast<CreateTableInfo>();
	vector<LogicalType> column_types;
	vector<string> column_names;
	vector<idx_t> not_nulls;

	// extract column logical types and names
	for (auto &col : base.columns.Logical()) {
		column_types.push_back(col.GetType());
		column_names.push_back(col.Name());
	}

	// extract constraints
	for (auto &c : base.constraints) {
		if (c->type == ConstraintType::NOT_NULL) {
			auto not_null_constraint = reinterpret_cast<NotNullConstraint *>(c.get());
			not_nulls.push_back(not_null_constraint->index.index);
		}
	}
	auto table_schema = ConvertDuckDBToSparkType(column_types, column_names, not_nulls);
	auto plan = spark_client->PlanCreateTable(base.schema, base.table, table_schema);

	auto status = spark_client->GetStatus(plan);

	if (!status.ok()) {
		throw CatalogException("Fail to create Spark table `%s` in schema `%s` of catalog `%s`. Error: `%s`.",
		                       base.table, base.schema, base.catalog, status.error_message());
	}

	auto table_entry = make_uniq<SparkTableEntry>(catalog, schema, base);
	return CreateEntry(std::move(table_entry));
}

optional_ptr<CatalogEntry> SparkTableSet::RefreshTable(ClientContext &context, const string &table_name) {
	return nullptr;
}
void SparkTableSet::AlterTable(ClientContext &context, AlterTableInfo &info) {
	throw NotImplementedException("Alter table is not implmented");
}

void SparkTableSet::LoadEntries(DatabaseInstance &db) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto spark_client = spark_catalog.spark_client;
	auto list_table_plan = spark_client->PlanListTables("*", schema.name);
	auto data = spark_client->GetRecordBatches(list_table_plan);
	auto table_infos = ParseRecordBatches(data);

	for (auto &table_info : table_infos) {
		AddColumnsToTableInfo(table_info, db, table_info.table);
		auto schema_entry = make_uniq<SparkTableEntry>(catalog, schema, table_info);
		CreateEntry(std::move(schema_entry));
	}
}

void SparkTableSet::AddColumnsToTableInfo(CreateTableInfo &info, DatabaseInstance &db, const std::string &table_name) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto spark_client = spark_catalog.spark_client;
	auto schema_name = schema.name;
	auto read_table_plan = spark_client->PlanReadTable(schema_name + "." + table_name);
	auto column_info = spark_client->AnalyzePlanToColumnInfo(read_table_plan);
	for (const auto &column : column_info) {
		auto column_def = ColumnDefinition(column.name, column.type);
		column_def.SetDefaultValue(make_uniq<ConstantExpression>(Value(nullptr)));
		info.columns.AddColumn(std::move(column_def));
	}
	info.columns.Finalize();
}

std::vector<CreateTableInfo> SparkTableSet::ParseRecordBatches(arrow::RecordBatchVector &batches) {
	// for each record batch, get schema, validate schema conversion compatibility before handling data
	// extract record batch data and cast to destination type
	std::vector<CreateTableInfo> tables;
	for (auto &batch : batches) {
		auto batch_schema = batch->schema();
		auto field_names = batch_schema->field_names();
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
			CreateTableInfo table_info;
			if (!name_array->IsNull(i)) {
				table_info.table = name_array->GetString(i);
				if (!description_array->IsNull(i)) {
					table_info.comment = description_array->GetString(i);
				}
				table_info.internal = false;
				table_info.temporary = false;
				tables.push_back(std::move(table_info));
			}
		}
	}
	return tables;
}
} // namespace spark
} // namespace duckdb
