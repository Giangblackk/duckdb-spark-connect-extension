#pragma once

#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/function/table/arrow/arrow_duck_schema.hpp"
#include "duckdb/parser/parsed_data/drop_info.hpp"
#include "spark/connect/types.pb.h"

#include <arrow/record_batch.h>
#include <arrow/type.h>
#include <memory>
#include <string>

namespace duckdb {
namespace spark {
struct SparkConfig {
public:
	SparkConfig(const std::string &host, int port);
	static SparkConfig FromURI(const std::string &connection_string);
	std::string GetEndpoint();
	std::string host;
	int port;
};

struct ColumnInfo {
	ColumnInfo(const std::string &name, const LogicalType &type) : name(name), type(type) {
	}
	std::string name;
	LogicalType type;
};

std::string generate_uuid();

duckdb::unique_ptr<duckdb::ArrowType> GetArrowType(duckdb::DBConfig &config, ArrowSchema &schema_item);

struct ArrowTableSchema {
public:
	void AddColumn(idx_t index, shared_ptr<ArrowType> type, const string &name) {
		D_ASSERT(arrow_convert_data.find(index) == arrow_convert_data.end());
		arrow_convert_data.emplace(std::make_pair(index, std::move(type)));
	}
	const arrow_column_map_t GetColumns() const {
		return arrow_convert_data;
	}

private:
	arrow_column_map_t arrow_convert_data;
};

void WriteRecordBatchToDataChunk(ClientContext &context, const std::shared_ptr<arrow::RecordBatch> &batch,
                                 DataChunk &output);

LogicalType ConvertSparkToDuckDBType(const ::spark::connect::DataType &dtype);

::spark::connect::DataType ConvertDuckDBToSparkType(const vector<LogicalType> &types, const vector<string> &names,
                                                    const vector<idx_t> &not_nulls);

std::shared_ptr<arrow::DataType> ConvertSparkToArrowType(const ::spark::connect::DataType &dtype);

::spark::connect::DataType SetSparkType(const LogicalType &type);

std::string CreateSchemaInfoToSQL(const CreateSchemaInfo &info);

std::string DropSchemaInfoToSQL(const DropInfo &info);

std::string DropTableInfoToSQL(const DropInfo &info);

} // namespace spark
} // namespace duckdb
