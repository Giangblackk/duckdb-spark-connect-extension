#include "spark_utils.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/uuid.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/main/config.hpp"

#include <arrow/api.h>
#include <arrow/c/bridge.h>

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
	auto current_chunk = make_uniq<ArrowArrayWrapper>();

	auto batch_export_status = arrow::ExportRecordBatch(*batch, &current_chunk->arrow_array);
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

LogicalType ConvertSparkToDuckDBType(const ::spark::connect::DataType &dtype) {
	const auto field_kind = dtype.kind_case();
	switch (field_kind) {
	case ::spark::connect::DataType::kNull:
		return LogicalType::SQLNULL;
	case ::spark::connect::DataType::kBinary:
		return LogicalType::BLOB;
	case ::spark::connect::DataType::kBoolean:
		return LogicalType::BOOLEAN;
	case ::spark::connect::DataType::kByte:
		return LogicalType::TINYINT;
	case ::spark::connect::DataType::kShort:
		return LogicalType::SMALLINT;
	case ::spark::connect::DataType::kInteger:
		return LogicalType::INTEGER;
	case ::spark::connect::DataType::kLong:
		return LogicalType::BIGINT;
	case ::spark::connect::DataType::kFloat:
		return LogicalType::FLOAT;
	case ::spark::connect::DataType::kDouble:
		return LogicalType::DOUBLE;
	case ::spark::connect::DataType::kDecimal: {
		const auto &decimal_dtype = dtype.decimal();
		auto scale = static_cast<uint8_t>(decimal_dtype.scale());
		auto precision = static_cast<uint8_t>(decimal_dtype.precision());
		return LogicalType::DECIMAL(precision, scale);
	}
	case ::spark::connect::DataType::kString:
		return LogicalType::VARCHAR;
	case ::spark::connect::DataType::kChar:
		return LogicalType::VARCHAR;
	case ::spark::connect::DataType::kVarChar:
		return LogicalType::VARCHAR;
	case ::spark::connect::DataType::kDate:
		return LogicalType::DATE;
	case ::spark::connect::DataType::kTimestamp:
		return LogicalType::TIMESTAMP_TZ;
	case ::spark::connect::DataType::kTimestampNtz:
		return LogicalType::TIMESTAMP;
	case ::spark::connect::DataType::kCalendarInterval:
		return LogicalType::INTERVAL;
	case ::spark::connect::DataType::kYearMonthInterval:
		return LogicalType::INTERVAL;
	case ::spark::connect::DataType::kDayTimeInterval:
		return LogicalType::INTERVAL;
	case ::spark::connect::DataType::kArray: {
		const auto &array_dtype = dtype.array();
		auto element_type = ConvertSparkToDuckDBType(array_dtype.element_type());
		return LogicalType::LIST(element_type);
	}
	case ::spark::connect::DataType::kStruct: {
		const auto &struct_dtype = dtype.struct_();
		child_list_t<LogicalType> children;
		for (const auto &field : struct_dtype.fields()) {
			children.push_back({field.name(), ConvertSparkToDuckDBType(field.data_type())});
		}
		return LogicalType::STRUCT(children);
	}
	case ::spark::connect::DataType::kMap: {
		const auto &map_dtype = dtype.map();
		auto key_dtype = ConvertSparkToDuckDBType(map_dtype.key_type());
		auto value_dtype = ConvertSparkToDuckDBType(map_dtype.value_type());
		return LogicalType::MAP(key_dtype, value_dtype);
	}
	case ::spark::connect::DataType::kUdt:
		throw InvalidTypeException("Spark UserDefinedType are currently not supported");
		break;
	case ::spark::connect::DataType::kUnparsed:
		return LogicalType::VARCHAR;
	case ::spark::connect::DataType::KIND_NOT_SET:
		throw InvalidTypeException("Spark Data type are not set");
	}
	// return default data type
	return LogicalType::SQLNULL;
}
} // namespace spark
} // namespace duckdb
