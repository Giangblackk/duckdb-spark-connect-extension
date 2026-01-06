#pragma once

#include "duckdb/common/types.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/function/table/arrow/arrow_duck_schema.hpp"
#include <arrow/record_batch.h>
#include <string>

namespace duckdb {
namespace spark {
struct SparkConfig {
public:
	SparkConfig() = default;
	static SparkConfig FromDSN(const std::string &connection_string);
};

struct ColumnInfo {
	ColumnInfo(const std::string &name, const LogicalTypeId type) : name(name), type(type) {
	}
	std::string name;
	LogicalTypeId type;
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

} // namespace spark
} // namespace duckdb
