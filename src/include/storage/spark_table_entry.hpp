#pragma once

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"

namespace duckdb {
namespace spark {

class SparkTableEntry : public TableCatalogEntry {
public:
	explicit SparkTableEntry(Catalog &catalog, SchemaCatalogEntry &schema, CreateTableInfo &info);

public:
	virtual_column_map_t GetVirtualColumns() const override;
	unique_ptr<BaseStatistics> GetStatistics(ClientContext &context, column_t column_id) override;
	TableFunction GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) override;
	TableFunction GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data,
	                              const EntryLookupInfo &lookup) override;
	TableStorageInfo GetStorageInfo(ClientContext &context) override;
	unique_ptr<SparkTableEntry> AlterEntryDirect(ClientContext &context, AlterInfo &info);

	Catalog &GetCatalog() const {
		return catalog_;
	}

private:
	Catalog &catalog_;
};
} // namespace spark
} // namespace duckdb
