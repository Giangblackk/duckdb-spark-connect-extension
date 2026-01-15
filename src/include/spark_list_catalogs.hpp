#pragma once

#include "duckdb/function/table_function.hpp"
namespace duckdb {
namespace spark {
class SparkListCatalogsFunction : public TableFunction {
public:
	SparkListCatalogsFunction();
};
} // namespace spark
} // namespace duckdb
