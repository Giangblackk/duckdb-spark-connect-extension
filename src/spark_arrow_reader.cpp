#include "spark_arrow_reader.hpp"

#include "duckdb/common/shared_ptr.hpp"

#include <arrow/buffer.h>
#include <arrow/io/memory.h>
#include <arrow/ipc/api.h>
#include <arrow/util/iterator.h>

namespace duckdb {
namespace spark {

unique_ptr<ArrowArrayStreamWrapper> SparkStreamFactory::Produce(uintptr_t factory_ptr,
                                                                ArrowStreamParameters & /*params*/) {
	auto *factory = reinterpret_cast<SparkStreamFactory *>(factory_ptr);

	auto wrapper = make_uniq<ArrowArrayStreamWrapper>();

	auto st = std::make_shared<SparkStreamState>();
	st->factory = factory;

	// Create Arrow iterator
	auto iter = arrow::MakeFunctionIterator([st]() -> arrow::Result<std::shared_ptr<arrow::RecordBatch>> {
		return st->factory->spark_client->IterateRecordBatches(st->factory->plan);
	});
	// Convert Iterator => RecordBatchReader => ArrowArrayStream
	auto schema = factory->spark_client->AnalyzePlanToArrowSchema(factory->plan);
	auto rb_reader = std::make_shared<IteratorBatchReader>(std::move(iter), schema);

	auto export_status = arrow::ExportRecordBatchReader(rb_reader, &wrapper->arrow_array_stream);
	if (!export_status.ok()) {
		throw BinderException("Arrow export failed: " + export_status.ToString());
	}
	return wrapper;
}

void SparkStreamFactory::GetSchema(ArrowArrayStream *factory_ptr, ArrowSchema &schema) {
	auto *factory = reinterpret_cast<SparkStreamFactory *>(factory_ptr);
	auto arrow_schema = factory->spark_client->AnalyzePlanToArrowSchema(factory->plan);

	auto status = arrow::ExportSchema(*arrow_schema, &schema);
	if (!status.ok()) {
		throw BinderException("Arrow export failed: " + status.ToString());
	}
}

} // namespace spark
} // namespace duckdb
