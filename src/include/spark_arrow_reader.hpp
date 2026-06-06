#pragma once

#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/shared_ptr.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "spark_client.hpp"

#include <arrow/c/bridge.h>
#include <arrow/record_batch.h>
#include <arrow/type.h>
#include <memory>
#include <utility>

namespace duckdb {
namespace spark {

//  Spark Stream Factory – provides ArrowArrayStreams from Spark Read Streams
class SparkStreamFactory {
public:
	explicit SparkStreamFactory(shared_ptr<SparkGRPCClient> spark_client, ::spark::connect::Plan &plan)
	    : spark_client(std::move(spark_client)), plan(plan) {
	}

	//! DuckDB calls this via the function pointer in the bind object
	static unique_ptr<ArrowArrayStreamWrapper> Produce(uintptr_t factory_ptr, ArrowStreamParameters & /*params*/);

	//! Called once in the bind step to get the schema
	static void GetSchema(ArrowArrayStream *factory_ptr, ArrowSchema &schema);

private:
	shared_ptr<SparkGRPCClient> spark_client;
	::spark::connect::Plan plan;
};

struct FactoryDependency final : public DependencyItem {
	explicit FactoryDependency(shared_ptr<SparkStreamFactory> ptr) : DependencyItem(), factory(std::move(ptr)) {
	}
	shared_ptr<SparkStreamFactory> factory;
};

struct SparkStreamState {
	SparkStreamFactory *factory;
};

class IteratorBatchReader : public arrow::RecordBatchReader {
public:
	explicit IteratorBatchReader(arrow::Iterator<std::shared_ptr<arrow::RecordBatch>> iterator,
	                             std::shared_ptr<arrow::Schema> schema)
	    : iterator_(std::move(iterator)), schema_(std::move(schema)) {
	}

	std::shared_ptr<arrow::Schema> schema() const override {
		return schema_;
	}

	arrow::Status ReadNext(std::shared_ptr<arrow::RecordBatch> *out) override {
		ARROW_ASSIGN_OR_RAISE(auto batch, iterator_.Next()); // nullptr = EOF
		*out = std::move(batch);
		return arrow::Status::OK();
	}

private:
	arrow::Iterator<std::shared_ptr<arrow::RecordBatch>> iterator_;
	std::shared_ptr<arrow::Schema> schema_;
};
} // namespace spark
} // namespace duckdb
