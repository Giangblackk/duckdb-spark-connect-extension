#pragma once

#include "duckdb/storage/storage_extension.hpp"

namespace duckdb {
namespace spark {
class SparkStorageExtension : public StorageExtension {
public:
	SparkStorageExtension();
};
} // namespace spark
} // namespace duckdb