#pragma once

#include <string>

namespace duckdb {
namespace spark {
struct SparkConfig {
public:
	SparkConfig() = default;
	static SparkConfig FromDSN(const std::string &connection_string);
};
} // namespace spark
} // namespace duckdb