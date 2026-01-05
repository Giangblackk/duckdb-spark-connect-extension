#pragma once

#include "duckdb/common/types.hpp"
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
} // namespace spark
} // namespace duckdb