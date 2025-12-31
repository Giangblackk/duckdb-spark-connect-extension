#include <string>
#include "spark_utils.hpp"
#include "duckdb/common/types/uuid.hpp"
namespace duckdb {
namespace spark {

SparkConfig SparkConfig::FromDSN(const std::string &connection_string) {
	SparkConfig spark_config = SparkConfig();
	return spark_config;
}

std::string generate_uuid() {
	return UUID::ToString(UUID::GenerateRandomUUID());
}
} // namespace spark
} // namespace duckdb