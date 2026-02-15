#include "storage/spark_catalog.hpp"

#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/storage/database_size.hpp"
#include "spark_client.hpp"
#include "spark_schema_entry.hpp"
#include "spark_schema_set.hpp"
#include "spark_utils.hpp"

#include <utility>

namespace duckdb {
namespace spark {
SparkCatalog::SparkCatalog(AttachedDatabase &db_p, const string &connection_str, SparkAttachOptions options_p)
    : Catalog(db_p), db_path(connection_str), config(SparkConfig::FromURI(connection_str)),
      options(std::move(options_p)), schemas(*this) {
	spark_client = make_shared_ptr<SparkGRPCClient>(config.GetEndpoint());
	// if not default spark catalog, set current catalog for spark gRPC client
	if (options.catalog != SPARK_DEFAULT_CATALOG) {
		auto plan = spark_client->PlanSetCurrentCatalog(options.catalog);
		auto status = spark_client->GetStatus(plan);
		if (!status.ok()) {
			throw BinderException("Fail to set spark catalog: `%s`. Error: `%s`.", options.catalog,
			                      status.error_message());
		}
	}
}

void SparkCatalog::Initialize(bool load_builtin) {
}

optional_ptr<CatalogEntry> SparkCatalog::CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) {
	return schemas.CreateSchema(transaction.GetContext(), info);
}

void SparkCatalog::DropSchema(ClientContext &context, DropInfo &info) {
	return schemas.DropEntry(context, info);
};

optional_ptr<SchemaCatalogEntry> SparkCatalog::LookupSchema(CatalogTransaction transaction,
                                                            const EntryLookupInfo &schema_lookup,
                                                            OnEntryNotFound if_not_found) {
	auto &schema_name = schema_lookup.GetEntryName();
	if (schema_name == DEFAULT_SCHEMA) {
		if (if_not_found == OnEntryNotFound::RETURN_NULL) {
			return nullptr;
		}
		throw CatalogException(schema_lookup.GetErrorContext(), "Schema with name \"%s\" not found", schema_name);
	}
	auto entry = schemas.GetEntry(transaction.GetContext(), schema_lookup);
	if (!entry && if_not_found != OnEntryNotFound::RETURN_NULL) {
		throw CatalogException(schema_lookup.GetErrorContext(), "Schema with name \"%s\" not found", schema_name);
	}
	return reinterpret_cast<SchemaCatalogEntry *>(entry.get());
	return nullptr;
}

void SparkCatalog::ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) {
	schemas.Scan(context, [&](CatalogEntry &schema) { callback(schema.Cast<SparkSchemaEntry>()); });
}

PhysicalOperator &SparkCatalog::PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner, LogicalInsert &op,
                                           optional_ptr<PhysicalOperator> plan) {
	throw BinderException("Spark does not supported INSERT yet.");
}

PhysicalOperator &SparkCatalog::PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner,
                                                  LogicalCreateTable &op, PhysicalOperator &plan) {
	throw BinderException("Spark does not supported CTAS yet.");
}

PhysicalOperator &SparkCatalog::PlanDelete(ClientContext &context, PhysicalPlanGenerator &planner, LogicalDelete &op,
                                           PhysicalOperator &plan) {
	throw BinderException("Spark does not supported DELETE yet.");
}
PhysicalOperator &SparkCatalog::PlanUpdate(ClientContext &context, PhysicalPlanGenerator &planner, LogicalUpdate &op,
                                           PhysicalOperator &plan) {
	throw BinderException("Spark does not supported UPDATE yet.");
}

DatabaseSize SparkCatalog::GetDatabaseSize(ClientContext &context) {
	// Use the tables.list API method to list all tables in the dataset
	// for each table, get the "numBytes" property
	// Sum these up to get the total size of the dataset
	throw BinderException("Spark does not support getting database size");
}

} // namespace spark
} // namespace duckdb
