#include "spark_schema_entry.hpp"

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_transaction.hpp"
#include "duckdb/common/exception.hpp"
#include "spark_table_set.hpp"

namespace duckdb {
namespace spark {
SparkSchemaEntry::SparkSchemaEntry(Catalog &catalog, CreateSchemaInfo &info)
    : SchemaCatalogEntry(catalog, info), tables(*this) {
}
SparkSchemaEntry::~SparkSchemaEntry() {
}

bool IsSupportedCatalogType(CatalogType type) {
	// currently support only table entries
	// TODO support other types of entries
	switch (type) {
	case CatalogType::TABLE_ENTRY:
		return true;
	default:
		return false;
	}
}

void SparkSchemaEntry::Scan(ClientContext &context, CatalogType type,
                            const std::function<void(CatalogEntry &)> &callback) {
	if (!IsSupportedCatalogType(type)) {
		return;
	}
	GetCatalogSet(type).Scan(context, callback);
};
void SparkSchemaEntry::Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) {
	throw NotImplementedException("Scan without context is not implemented");
};
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
	if (!IsSupportedCatalogType(lookup_info.GetCatalogType())) {
		return nullptr;
	}
	return GetCatalogSet(lookup_info.GetCatalogType()).GetEntry(transaction.GetContext(), lookup_info);
};
void SparkSchemaEntry::Alter(CatalogTransaction transaction, AlterInfo &info) {};

SparkCatalogSet &SparkSchemaEntry::GetCatalogSet(CatalogType type) {
	switch (type) {
	case CatalogType::TABLE_ENTRY:
		return tables;
	default:
		string error_message = "Spark: Type not supported for GetCatalogSet: " + CatalogTypeToString(type);
		throw NotImplementedException(error_message);
	}
}

} // namespace spark
} // namespace duckdb
