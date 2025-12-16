#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/storage/database_size.hpp"
#include "spark_utils.hpp"
#include "storage/spark_catalog.hpp"

namespace duckdb {
namespace spark {
SparkCatalog::SparkCatalog(AttachedDatabase &db_p, const SparkConfig &config, SparkOptions options_p)
    : Catalog(db_p), config(config), options(options_p) {
}

SparkCatalog::SparkCatalog(AttachedDatabase &db_p, const string &dsn, SparkOptions options_p)
    : SparkCatalog(db_p, SparkConfig::FromDSN(dsn), options_p) {
}

void SparkCatalog::Initialize(bool load_builtin) {
}

optional_ptr<CatalogEntry> SparkCatalog::CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) {
	return nullptr;
}

optional_ptr<SchemaCatalogEntry> SparkCatalog::LookupSchema(CatalogTransaction transaction,
                                                            const EntryLookupInfo &schema_lookup,
                                                            OnEntryNotFound if_not_found) {
	return nullptr;
}

void SparkCatalog::ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) {
	throw NotImplementedException("Spark does not support Scan Schemas yet");
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