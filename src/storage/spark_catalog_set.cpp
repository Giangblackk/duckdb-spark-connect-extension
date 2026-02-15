#include "spark_catalog_set.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/enums/on_entry_not_found.hpp"
#include "duckdb/common/exception/catalog_exception.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/parser/parsed_data/drop_info.hpp"
#include "spark_catalog.hpp"
#include "spark_schema_entry.hpp"

#include <mutex>
namespace duckdb {
namespace spark {

SparkCatalogSet::SparkCatalogSet(Catalog &catalog) : catalog(catalog), is_loaded(false) {
}

void SparkCatalogSet::Scan(ClientContext &context, const std::function<void(CatalogEntry &)> &callback) {
	// lock
	std::lock_guard<std::mutex> guard(entry_lock);
	// load entries if not loaded
	if (!is_loaded) {
		is_loaded = true;
		LoadEntries(*context.db);
	}
	// run callback on entries
	for (auto &entry : entries) {
		callback(*entry.second);
	}
}

optional_ptr<CatalogEntry> SparkCatalogSet::GetEntry(ClientContext &context, const EntryLookupInfo &lookup_info) {
	std::lock_guard<std::mutex> guard(entry_lock);
	if (!is_loaded) {
		is_loaded = true;
		LoadEntries(*context.db);
	}
	auto entry = entries.find(lookup_info.GetEntryName());
	if (entry == entries.end()) {
		return nullptr;
	}
	return entry->second.get();
}

std::string SparkCatalogSet::DropSchemaInfoToSQL(const DropInfo &info) {
	duckdb::stringstream ss;

	ss << "DROP DATABASE ";

	if (info.if_not_found == OnEntryNotFound::RETURN_NULL) {
		ss << "IF EXISTS ";
	}
	ss << info.name;

	if (info.cascade) {
		ss << " CASCADE";
	}
	return ss.str();
}

void SparkCatalogSet::EraseEntryInternal(const string &name) {
	lock_guard<mutex> lock(entry_lock);
	entries.erase(name);
}

void SparkCatalogSet::DropEntry(ClientContext &context, DropInfo &info) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	auto spark_client = spark_catalog.spark_client;
	auto sql_string = DropSchemaInfoToSQL(info);
	auto plan = spark_client->PlanExecuteSQLCommand(sql_string);
	auto status = spark_client->GetStatus(plan);
	if (!status.ok()) {
		throw CatalogException("Fail to drop Spark schema `%s` in catalog `%s`. SQL command: `%s`. Error: `%s`.",
		                       info.schema, info.catalog, sql_string, status.error_message());
	}

	EraseEntryInternal(info.name);
}

SparkInSchemaSet::SparkInSchemaSet(SparkSchemaEntry &schema) : SparkCatalogSet(schema.ParentCatalog()), schema(schema) {
}

optional_ptr<CatalogEntry> SparkInSchemaSet::CreateEntry(unique_ptr<CatalogEntry> entry) {
	if (!entry->internal) {
		entry->internal = schema.internal;
	}
	return SparkCatalogSet::CreateEntry(std::move(entry));
}

} // namespace spark
} // namespace duckdb
