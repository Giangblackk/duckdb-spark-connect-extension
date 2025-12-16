#include "duckdb/common/helper.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/common/optional_ptr.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/parser/parsed_data/attach_info.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/transaction/transaction_manager.hpp"

#include "spark_storage.hpp"
#include "storage/spark_catalog.hpp"
#include "storage/spark_transaction.hpp"

namespace duckdb {
namespace spark {

static unique_ptr<Catalog> SparkAttach(optional_ptr<StorageExtensionInfo> storage_info, ClientContext &context,
                                       AttachedDatabase &db, const string &name, AttachInfo &info,
                                       AttachOptions &attach_options) {
	SparkOptions options;
	options.access_mode = attach_options.access_mode;
	return make_uniq<SparkCatalog>(db, info.path, options);
}

static unique_ptr<TransactionManager> SparkCreateTransactionManager(optional_ptr<StorageExtensionInfo> storage_info,
                                                                    AttachedDatabase &db, Catalog &catalog) {
	auto &spark_catalog = catalog.Cast<SparkCatalog>();
	return make_uniq<SparkTransactionManager>(db, spark_catalog);
}

SparkStorageExtension::SparkStorageExtension() {
	attach = SparkAttach;
	create_transaction_manager = SparkCreateTransactionManager;
}
} // namespace spark
} // namespace duckdb