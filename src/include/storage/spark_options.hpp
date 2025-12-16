#pragma once

#include "duckdb/common/enums/access_mode.hpp"
namespace duckdb {
namespace spark {
struct SparkOptions {
	AccessMode access_mode = AccessMode::READ_WRITE;
};
} // namespace spark
} // namespace duckdb