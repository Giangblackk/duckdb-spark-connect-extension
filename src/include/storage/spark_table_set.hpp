#pragma once

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry.hpp"
#include "duckdb/catalog/entry_lookup_info.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/parser/parsed_data/alter_table_info.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "spark_catalog_set.hpp"

#include <arrow/type_fwd.h>

namespace duckdb {
namespace spark {
class SparkSchemaEntry;

class SparkTableSet : public SparkInSchemaSet {
public:
	explicit SparkTableSet(SparkSchemaEntry &schema) : SparkInSchemaSet(schema) {};
	~SparkTableSet() override {};

public:
	optional_ptr<CatalogEntry> GetEntry(ClientContext &context, const EntryLookupInfo &lookup_info) override;
	optional_ptr<CatalogEntry> CreateTable(ClientContext &context, BoundCreateTableInfo &info);
	optional_ptr<CatalogEntry> RefreshTable(ClientContext &context, const string &table_name);
	void AlterTable(ClientContext &context, AlterTableInfo &info);

protected:
	void LoadEntries(DatabaseInstance &db) override;

private: // methods
	static std::vector<CreateTableInfo> ParseRecordBatches(arrow::RecordBatchVector &batches);
	void AddColumnsToTableInfo(CreateTableInfo &info, DatabaseInstance &db, const std::string &table_name);
};
} // namespace spark
} // namespace duckdb
