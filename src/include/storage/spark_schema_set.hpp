#pragma once

#include "spark_catalog_set.hpp"

#include <arrow/type_fwd.h>

namespace duckdb {

struct CreateSchemaInfo;

namespace spark {

class SparkSchemaSet : public SparkCatalogSet {
public:
	explicit SparkSchemaSet(Catalog &catalog);
	~SparkSchemaSet() override = default;

	optional_ptr<CatalogEntry> CreateSchema(ClientContext &context, CreateSchemaInfo &info);

	void LoadEntries(DatabaseInstance &db) override;

private: // methods
	static std::vector<CreateSchemaInfo> ParseRecordBatches(arrow::RecordBatchVector &batches);

private:
	bool populated_entire_set = false;
	bool called_load_entries = false;
};
} // namespace spark
} // namespace duckdb
