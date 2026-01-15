#pragma once

#include "duckdb/function/table_function.hpp"
namespace duckdb {
namespace spark {
class SparkListTablesFunction : public TableFunction {
public:
	SparkListTablesFunction();
};
} // namespace spark
} // namespace duckdb
