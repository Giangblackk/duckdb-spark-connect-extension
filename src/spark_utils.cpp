#include <string>
#include "spark_utils.hpp"

namespace duckdb {
namespace spark {

SparkConfig SparkConfig::FromDSN(const std::string &connection_string) {
	SparkConfig spark_config = SparkConfig();
	return spark_config;
}
} // namespace spark
} // namespace duckdb