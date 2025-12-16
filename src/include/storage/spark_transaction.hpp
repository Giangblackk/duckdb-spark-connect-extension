#pragma once

#include "duckdb/main/client_context.hpp"
#include "duckdb/transaction/transaction.hpp"
#include "duckdb/transaction/transaction_manager.hpp"

namespace duckdb {
namespace spark {
class SparkCatalog;
class SparkSchemaEntry;
class SparkTableEntry;

class SparkTransaction : public Transaction {
public:
	SparkTransaction(SparkCatalog &spark_catalog, TransactionManager &manager, ClientContext &context);
	~SparkTransaction() override;
	static SparkTransaction &Get(ClientContext &context, Catalog &catalog);
	AccessMode GetAccessMode() {
		return access_mode;
	}

private:
	SparkCatalog &spark_catalog;
	case_insensitive_map_t<unique_ptr<CatalogEntry>> catalog_entries;
	AccessMode access_mode;
};

class SparkTransactionManager : public TransactionManager {
public:
	SparkTransactionManager(AttachedDatabase &db_p, SparkCatalog &spark_catalog);
	~SparkTransactionManager() override {};

	Transaction &StartTransaction(ClientContext &context) override;
	ErrorData CommitTransaction(ClientContext &context, Transaction &transaction) override;
	void RollbackTransaction(Transaction &transaction) override;

	void Checkpoint(ClientContext &context, bool force = false) override;

private:
	SparkCatalog &spark_catalog;
	mutex transaction_lock;
	reference_map_t<Transaction, unique_ptr<SparkTransaction>> transactions;
};

} // namespace spark
} // namespace duckdb