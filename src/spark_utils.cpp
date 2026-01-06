#include <arrow/api.h>
#include <arrow/c/bridge.h>
#include <string>
#include "spark_utils.hpp"
#include "duckdb/common/types/uuid.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/main/config.hpp"
namespace duckdb {
namespace spark {

SparkConfig SparkConfig::FromDSN(const std::string &connection_string) {
	SparkConfig spark_config = SparkConfig();
	return spark_config;
}

std::string generate_uuid() {
	return UUID::ToString(UUID::GenerateRandomUUID());
}

duckdb::unique_ptr<duckdb::ArrowType> GetArrowType(duckdb::DBConfig &config, ArrowSchema &schema_item) {
	auto arrow_type = ArrowType::GetArrowLogicalType(config, schema_item);

	if (schema_item.dictionary) {
		auto dictionary_type = ArrowType::GetArrowLogicalType(config, *schema_item.dictionary);
		arrow_type->SetDictionary(std::move(dictionary_type));
	}
	return arrow_type;
}

void WriteRecordBatchToDataChunk(ClientContext &context, const std::shared_ptr<arrow::RecordBatch> &batch,
                                 DataChunk &output) {
	// Get DBConfig
	auto &config = DBConfig::GetConfig(context);

	// Extract schema from next batch
	ArrowSchemaWrapper schema_root;
	auto schema_export_status = arrow::ExportSchema(*batch->schema(), &schema_root.arrow_schema);
	if (!schema_export_status.ok()) {
		throw ExecutorException("Failed to Export Arrow Schema");
	}

	// Convert arrow types to duckdb types
	ArrowTableSchema arrow_table;

	for (auto col_index = 0; col_index < schema_root.arrow_schema.n_children; col_index++) {
		auto &schema_item = *schema_root.arrow_schema.children[col_index];

		auto arrow_type = GetArrowType(config, schema_item);

		arrow_table.AddColumn(col_index, std::move(arrow_type), schema_item.name);
	}

	// Export Record Batches to Arrow Array
	ArrowSchema c_schema;
	auto current_chunk = make_uniq<ArrowArrayWrapper>();

	auto batch_export_status = arrow::ExportRecordBatch(*batch, &current_chunk->arrow_array, &c_schema);
	if (!batch_export_status.ok()) {
		throw ExecutorException("Failed to Export Arrow RecordBatch");
	}

	// Set Cardinality before write to DataChunk
	output.SetCardinality(current_chunk->arrow_array.length);

	// ArrowScanLocalState for converting
	ArrowScanLocalState fake_local_state(std::move(current_chunk), context);

	// Write data to output DataChunk
	ArrowTableFunction::ArrowToDuckDB(fake_local_state, arrow_table.GetColumns(), output, 0);

	// Verify output
	output.Verify();
}

} // namespace spark
} // namespace duckdb