#include "duckdb/main/client_context.hpp"
#include "duckdb/transaction/transaction.hpp"
#include "storage/spark_catalog.hpp"
#include "storage/spark_transaction.hpp"

namespace duckdb {
namespace spark {
SparkTransaction::SparkTransaction(SparkCatalog &spark_catalog, TransactionManager &manager, ClientContext &context)
    : Transaction(manager, context), //
      spark_catalog(spark_catalog),  //
      access_mode(spark_catalog.options.access_mode) {
}

SparkTransaction::~SparkTransaction() {
}

SparkTransactionManager::SparkTransactionManager(AttachedDatabase &db_p, SparkCatalog &spark_catalog)
    : TransactionManager(db_p), spark_catalog(spark_catalog) {
}

Transaction &SparkTransactionManager::StartTransaction(ClientContext &context) {
	auto transaction = make_uniq<SparkTransaction>(spark_catalog, *this, context);
	auto &result = *transaction;
	lock_guard<mutex> lock(transaction_lock);
	transactions[result] = std::move(transaction);
	return result;
}

ErrorData SparkTransactionManager::CommitTransaction(ClientContext &context, Transaction &transaction) {
	lock_guard<mutex> lock(transaction_lock);
	transactions.erase(transaction);
	return ErrorData();
}

void SparkTransactionManager::RollbackTransaction(Transaction &transaction) {
	lock_guard<mutex> lock(transaction_lock);
	transactions.erase(transaction);
}

void SparkTransactionManager::Checkpoint(ClientContext &context, bool force) {
	throw NotImplementedException("Spark does not support checkpoints");
}

} // namespace spark
} // namespace duckdb