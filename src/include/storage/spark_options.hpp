#pragma once

#include "duckdb/common/enums/access_mode.hpp"
namespace duckdb {
namespace spark {
struct SparkAttachOptions {
	AccessMode access_mode = AccessMode::READ_WRITE;
	std::string catalog = "spark_catalog";
};
} // namespace spark
} // namespace duckdb
