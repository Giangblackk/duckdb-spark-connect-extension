#include "spark_utils.hpp"

#include "duckdb.h"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/exception/parser_exception.hpp"
#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/uuid.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "spark/connect/types.pb.h"

#include <arrow/api.h>
#include <arrow/c/bridge.h>
#include <string>

namespace duckdb {
namespace spark {
SparkConfig::SparkConfig(const std::string &host, int port) : host(host), port(port) {
}

SparkConfig SparkConfig::FromURI(const std::string &url) {

	// split base path and query
	size_t query_pos = url.find('?');
	std::string base = (query_pos == std::string::npos) ? url : url.substr(0, query_pos);
	std::string query = (query_pos == std::string::npos) ? "" : url.substr(query_pos + 1);

	// Extract scheme and authority
	size_t scheme_end = base.find("://");
	if (scheme_end == std::string::npos) {
		throw ParserException("Invalid URL: missing scheme");
	}
	// Validate scheme
	std::string scheme = base.substr(0, scheme_end);
	if (scheme != "sc") {
		throw ParserException("Invalid Scheme: `%s`. Database path must start with `sc://`", scheme);
	}
	// Extract host and port
	std::string path = base.substr(scheme_end + 3);
	size_t endpoint_end = path.find(':');
	std::string host = (endpoint_end == std::string::npos) ? path : path.substr(0, endpoint_end);
	std::string port_str = (endpoint_end == std::string::npos) ? "15002" : path.substr(endpoint_end + 1);
	auto port = std::stoi(port_str);

	return SparkConfig(host, port);
}

std::string SparkConfig::GetEndpoint() {
	return host + ":" + std::to_string(port);
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

::spark::connect::DataType ConvertDuckDBToSparkType(const vector<LogicalType> &types, const vector<string> &names,
                                                    const vector<idx_t> &not_nulls) {
	D_ASSERT(types.size() == names.size());
	D_ASSERT(not_nulls.size() <= types.size());
	const idx_t column_count = types.size();

	// Initialize DataType, Struct and vector of StrucFields
	::spark::connect::DataType spark_dtype;
	::spark::connect::DataType::Struct spark_struct;
	vector<::spark::connect::DataType::StructField> fields;

	// convert LogicalType to corresponding Spark DataType
	for (idx_t col_idx = 0; col_idx < column_count; col_idx++) {
		const auto &type = types[col_idx];
		const auto &name = names[col_idx];

		// not found in `not_nulls` -> nullable
		const bool nullable = std::find(not_nulls.begin(), not_nulls.end(), col_idx) == not_nulls.end();
		::spark::connect::DataType::StructField f;
		f.mutable_data_type()->CopyFrom(SetSparkType(type));
		f.set_name(name);
		f.set_nullable(nullable);
		fields.push_back(f);
	}

	// Add fields to struct
	for (auto &field : fields) {
		auto new_field = spark_struct.add_fields();
		new_field->CopyFrom(field);
	}
	// Add structs to DataType
	spark_dtype.mutable_struct_()->CopyFrom(spark_struct);
	return spark_dtype;
}

::spark::connect::DataType SetSparkType(const LogicalType &type) {
	switch (type.id()) {
	case LogicalTypeId::SQLNULL: {
		::spark::connect::DataType d;
		d.mutable_null()->CopyFrom(::spark::connect::DataType::NULL_());
		return d;
	}
	case LogicalTypeId::BOOLEAN: {
		::spark::connect::DataType d;
		d.mutable_boolean()->CopyFrom(::spark::connect::DataType::Boolean());
		return d;
	}
	case LogicalTypeId::TINYINT: {
		::spark::connect::DataType d;
		d.mutable_byte()->CopyFrom(::spark::connect::DataType::Byte());
		return d;
	}
	case LogicalTypeId::SMALLINT: {
		::spark::connect::DataType d;
		d.mutable_short_()->CopyFrom(::spark::connect::DataType::Short());
		return d;
	}
	case LogicalTypeId::INTEGER: {
		::spark::connect::DataType d;
		d.mutable_integer()->CopyFrom(::spark::connect::DataType::Integer());
		return d;
	}
	case LogicalTypeId::BIGINT: {
		::spark::connect::DataType d;
		d.mutable_long_()->CopyFrom(::spark::connect::DataType::Long());
		return d;
	}
	case LogicalTypeId::DATE: {
		::spark::connect::DataType d;
		d.mutable_date()->CopyFrom(::spark::connect::DataType::Date());
		return d;
	}
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_MS:
	case LogicalTypeId::TIMESTAMP_NS:
	case LogicalTypeId::TIMESTAMP: {
		::spark::connect::DataType d;
		d.mutable_timestamp_ntz()->CopyFrom(::spark::connect::DataType::TimestampNTZ());
		return d;
	}
	case LogicalTypeId::DECIMAL: {
		uint8_t width, scale;
		type.GetDecimalProperties(width, scale);
		::spark::connect::DataType d;
		::spark::connect::DataType::Decimal dec;
		dec.set_scale(scale);
		dec.set_precision(width);
		d.mutable_decimal()->CopyFrom(dec);
		return d;
	}

	case LogicalTypeId::FLOAT: {
		::spark::connect::DataType d;
		d.mutable_float_()->CopyFrom(::spark::connect::DataType::Float());
		return d;
	}
	case LogicalTypeId::DOUBLE: {
		::spark::connect::DataType d;
		d.mutable_double_()->CopyFrom(::spark::connect::DataType::Double());
		return d;
	}
	case LogicalTypeId::CHAR:
	case LogicalTypeId::VARCHAR: {
		::spark::connect::DataType d;
		d.mutable_string()->CopyFrom(::spark::connect::DataType::String());
		return d;
	}
	case LogicalTypeId::BLOB: {
		::spark::connect::DataType d;
		d.mutable_binary()->CopyFrom(::spark::connect::DataType::Binary());
		return d;
	}
	case LogicalTypeId::TIMESTAMP_TZ: {
		::spark::connect::DataType d;
		d.mutable_timestamp()->CopyFrom(::spark::connect::DataType::Timestamp());
		return d;
	}
	// ARRAY and LIST convert to Spark ArrayType
	case LogicalTypeId::ARRAY:
	case LogicalTypeId::LIST: {
		auto child_type = ListType::GetChildType(type);
		::spark::connect::DataType d;
		d.mutable_array()->CopyFrom(SetSparkType(child_type));
		return d;
	}
	case LogicalTypeId::STRUCT: {
		const child_list_t<LogicalType> &child_types = StructType::GetChildTypes(type);
		::spark::connect::DataType::Struct s;
		for (size_t type_idx = 0; type_idx < child_types.size(); type_idx++) {
			auto child = child_types[type_idx];
			auto child_name = child.first;
			auto child_type = child.second;
			::spark::connect::DataType::StructField child_field;
			child_field.mutable_data_type()->CopyFrom(SetSparkType(child_type));
			child_field.set_name(child_name);
			auto new_field = s.add_fields();
			new_field->CopyFrom(child_field);
		}
		::spark::connect::DataType d;
		d.mutable_struct_()->CopyFrom(s);
		return d;
	}
	case LogicalTypeId::MAP: {
		auto key_type = MapType::KeyType(type);
		auto value_type = MapType::ValueType(type);
		auto spark_key_type = SetSparkType(key_type);
		auto spark_value_type = SetSparkType(value_type);
		::spark::connect::DataType::Map m;
		m.mutable_key_type()->CopyFrom(spark_key_type);
		m.mutable_value_type()->CopyFrom(spark_value_type);
		::spark::connect::DataType d;
		d.mutable_map()->CopyFrom(m);
		return d;
	}

	default:
		throw NotImplementedException("Unsupported conversion from DuckDB data type `%s` to Spark data type",
		                              type.ToString());
		break;
	}
}
std::string CreateSchemaInfoToSQL(const CreateSchemaInfo &info) {
	duckdb::stringstream ss;

	ss << "CREATE DATABASE ";

	if (info.on_conflict == OnCreateConflict::IGNORE_ON_CONFLICT) {
		ss << "IF NOT EXISTS ";
	}

	ss << info.schema;
	return ss.str();
}

std::string DropSchemaInfoToSQL(const DropInfo &info) {
	duckdb::stringstream ss;

	ss << "DROP DATABASE ";

	if (info.if_not_found == OnEntryNotFound::RETURN_NULL) {
		ss << "IF EXISTS ";
	}
	ss << info.name;

	if (info.cascade) {
		ss << " CASCADE";
	}
	return ss.str();
}

std::string DropTableInfoToSQL(const DropInfo &info) {
	duckdb::stringstream ss;

	ss << "DROP TABLE ";

	if (info.if_not_found == OnEntryNotFound::RETURN_NULL) {
		ss << "IF EXISTS ";
	}
	ss << info.schema + "." + info.name;

	if (info.cascade) {
		ss << " CASCADE";
	}
	return ss.str();
}

} // namespace spark
} // namespace duckdb
