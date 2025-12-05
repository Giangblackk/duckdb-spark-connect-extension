cmake_minimum_required(VERSION 3.5)

# set cmake standard to 17 due to protobuf
set(CMAKE_CXX_STANDARD 17)

# Set extension name here
set(TARGET_NAME quack)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

find_package(gRPC CONFIG REQUIRED)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES src/quack_extension.cpp)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Link OpenSSL in both the static library as the loadable extension
target_link_libraries(${EXTENSION_NAME}
  gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
