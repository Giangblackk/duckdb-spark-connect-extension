#pragma once

#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/transaction/transaction.hpp"

namespace duckdb {

namespace spark {

class SparkSchemaEntry;
class SparkTransaction;

class SparkCatalogSet {

public:
	explicit SparkCatalogSet(Catalog &catalog);
	virtual ~SparkCatalogSet() = default;

	virtual optional_ptr<CatalogEntry> GetEntry(ClientContext &context, const EntryLookupInfo &lookup_info);
	virtual void DropEntry(ClientContext &context, DropInfo &info);
	void Scan(ClientContext &context, const std::function<void(CatalogEntry &)> &callback);
	virtual optional_ptr<CatalogEntry> CreateEntry(unique_ptr<CatalogEntry> entry);
	void ClearEntries();
	void ReplaceEntry(const std::string &name, unique_ptr<CatalogEntry> entry);

protected:
	virtual void LoadEntries(DatabaseInstance &db) = 0;
	void EraseEntryInternal(const std::string &name);

protected:
	Catalog &catalog;
	mutex entry_lock;
	case_insensitive_map_t<unique_ptr<CatalogEntry>> entries;
	bool is_loaded;
};

class SparkInSchemaSet : public SparkCatalogSet {
public:
	explicit SparkInSchemaSet(SparkSchemaEntry &schema);
	~SparkInSchemaSet() override = default;

	optional_ptr<CatalogEntry> CreateEntry(unique_ptr<CatalogEntry> entry) override;

protected:
	SparkSchemaEntry &schema;
};

} // namespace spark
} // namespace duckdb
