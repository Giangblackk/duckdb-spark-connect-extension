#include "spark_schema_entry.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_transaction.hpp"

namespace duckdb {
namespace spark {
SparkSchemaEntry::SparkSchemaEntry(Catalog &catalog, CreateSchemaInfo &info) : SchemaCatalogEntry(catalog, info) {
}
SparkSchemaEntry::~SparkSchemaEntry() {
}

void SparkSchemaEntry::Scan(ClientContext &context, CatalogType type,
                            const std::function<void(CatalogEntry &)> &callback) {};
void SparkSchemaEntry::Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) {};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateIndex(CatalogTransaction transaction, CreateIndexInfo &info,
                                                         TableCatalogEntry &table) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateFunction(CatalogTransaction transaction, CreateFunctionInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateTable(CatalogTransaction transaction, BoundCreateTableInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateView(CatalogTransaction transaction, CreateViewInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateTableFunction(CatalogTransaction transaction,
                                                                 CreateTableFunctionInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateSequence(CatalogTransaction transaction, CreateSequenceInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateCopyFunction(CatalogTransaction transaction,
                                                                CreateCopyFunctionInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreatePragmaFunction(CatalogTransaction transaction,
                                                                  CreatePragmaFunctionInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateCollation(CatalogTransaction transaction,
                                                             CreateCollationInfo &info) {
	return nullptr;
};
optional_ptr<CatalogEntry> SparkSchemaEntry::CreateType(CatalogTransaction transaction, CreateTypeInfo &info) {
	return nullptr;
};
void SparkSchemaEntry::DropEntry(ClientContext &context, DropInfo &info) {};

optional_ptr<CatalogEntry> SparkSchemaEntry::LookupEntry(CatalogTransaction transaction,
                                                         const EntryLookupInfo &lookup_info) {
	return nullptr;
};
void SparkSchemaEntry::Alter(CatalogTransaction transaction, AlterInfo &info) {};

} // namespace spark
} // namespace duckdb
