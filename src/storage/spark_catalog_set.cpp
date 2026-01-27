#include "spark_catalog_set.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/main/client_context.hpp"

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

void SparkCatalogSet::DropEntry(ClientContext &context, DropInfo &info) {
}

} // namespace spark
} // namespace duckdb
