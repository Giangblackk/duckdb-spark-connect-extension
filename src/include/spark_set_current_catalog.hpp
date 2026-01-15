#pragma once

#include "duckdb/function/table_function.hpp"
namespace duckdb {
namespace spark {
class SparkSetCurrentCatalogFunction : public TableFunction {
public:
	SparkSetCurrentCatalogFunction();
};
} // namespace spark
} // namespace duckdb
