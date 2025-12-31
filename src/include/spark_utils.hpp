#pragma once

#include <string>

namespace duckdb {
namespace spark {
struct SparkConfig {
public:
	SparkConfig() = default;
	static SparkConfig FromDSN(const std::string &connection_string);
};

std::string generate_uuid();
} // namespace spark
} // namespace duckdb