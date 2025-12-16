#pragma once

#include "duckdb/catalog/catalog.hpp"

#include "storage/spark_options.hpp"
#include "spark_utils.hpp"

namespace duckdb {
namespace spark {

class SparkCatalog : public Catalog {
public:
	explicit SparkCatalog(AttachedDatabase &db_p, const SparkConfig &config, SparkOptions options_p);
	explicit SparkCatalog(AttachedDatabase &db_p, const string &connection_str, SparkOptions options_p);
	~SparkCatalog() override = default;

	SparkConfig config;
	SparkOptions options;

public:
	string GetCatalogType() override {
		return "spark";
	}
	void Initialize(bool load_builtin) override;
	optional_ptr<CatalogEntry> CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) override;
	optional_ptr<SchemaCatalogEntry> LookupSchema(CatalogTransaction transaction, const EntryLookupInfo &schema_lookup,
	                                              OnEntryNotFound if_not_found) override;
	void ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) override;
	PhysicalOperator &PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner, LogicalCreateTable &op,
	                                    PhysicalOperator &plan) override;

	PhysicalOperator &PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner, LogicalInsert &op,
	                             optional_ptr<PhysicalOperator> plan) override;
	PhysicalOperator &PlanDelete(ClientContext &context, PhysicalPlanGenerator &planner, LogicalDelete &op,
	                             PhysicalOperator &plan) override;
	PhysicalOperator &PlanUpdate(ClientContext &context, PhysicalPlanGenerator &planner, LogicalUpdate &op,
	                             PhysicalOperator &plan) override;
	DatabaseSize GetDatabaseSize(ClientContext &context) override;
	bool InMemory() override {
		return false;
	};
	string GetDBPath() override {
		return "default";
	};
	void DropSchema(ClientContext &context, DropInfo &info) override {
		throw NotImplementedException("Spark does not support Drop Schema");
	};
};

} // namespace spark
} // namespace duckdb