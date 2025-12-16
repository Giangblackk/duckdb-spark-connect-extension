#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {
namespace spark {
class SparkAttachFunction : public TableFunction {
public:
	SparkAttachFunction();
};
} // namespace spark
} // namespace duckdb