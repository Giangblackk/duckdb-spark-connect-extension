#pragma once

#include "duckdb/function/table_function.hpp"
namespace duckdb {
namespace spark {
class SparkListDatabasesFunction : public TableFunction {
public:
	SparkListDatabasesFunction();
};
} // namespace spark
} // namespace duckdb
