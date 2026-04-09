#pragma once

#include "duckdb/catalog/catalog.hpp"
#include "spark_client.hpp"
#include "spark_utils.hpp"
#include "storage/spark_options.hpp"
#include "storage/spark_schema_set.hpp"

namespace duckdb {
namespace spark {

#define SPARK_DEFAULT_CATALOG "spark_catalog"
class SparkCatalog : public Catalog {
public:
	explicit SparkCatalog(AttachedDatabase &db_p, const string &connection_str, SparkAttachOptions options_p);
	~SparkCatalog() override = default;

	string GetCatalogType() override {
		return "spark";
	}
	void Initialize(bool load_builtin) override;
	//! Creates a schema in the catalog.
	optional_ptr<CatalogEntry> CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) override;
	optional_ptr<SchemaCatalogEntry> LookupSchema(CatalogTransaction transaction, const EntryLookupInfo &schema_lookup,
	                                              OnEntryNotFound if_not_found) override;
	//! Scans all the schemas in the system one-by-one, invoking the callback for each entry
	void ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) override;
	PhysicalOperator &PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner, LogicalCreateTable &op,
	                                    PhysicalOperator &plan) override;

	PhysicalOperator &PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner, LogicalInsert &op,
	                             optional_ptr<PhysicalOperator> plan) override;
	PhysicalOperator &PlanDelete(ClientContext &context, PhysicalPlanGenerator &planner, LogicalDelete &op,
	                             PhysicalOperator &plan) override;
	PhysicalOperator &PlanUpdate(ClientContext &context, PhysicalPlanGenerator &planner, LogicalUpdate &op,
	                             PhysicalOperator &plan) override;
	PhysicalOperator &PlanMergeInto(ClientContext &context, PhysicalPlanGenerator &planner, LogicalMergeInto &op,
	                                PhysicalOperator &plan) override;
	DatabaseSize GetDatabaseSize(ClientContext &context) override;
	bool InMemory() override {
		return false;
	};
	string GetDBPath() override {
		return db_path;
	};

public:
	SparkConfig config;
	SparkAttachOptions options;
	shared_ptr<SparkGRPCClient> spark_client;

private:
	void DropSchema(ClientContext &context, DropInfo &info) override;

private:
	SparkSchemaSet schemas;
	std::string db_path;
};

} // namespace spark
} // namespace duckdb
