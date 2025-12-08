# find Protobuf and gRPC libraries
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# set(PROTO_FILES
#     myproto/sample.proto
# )

# set(PROTO_TARGET_NAME myproto)
# set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/proto")
# set(PROTO_BINARY_DIR "${CMAKE_BINARY_DIR}/proto/generated")

# configure to compile/generate protobuf c++ code and headers
add_library(squawk_myproto STATIC "myproto/sample.proto")

target_link_libraries(squawk_myproto PUBLIC protobuf::libprotobuf gRPC::grpc gRPC::grpc++)

target_include_directories(squawk_myproto PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

protobuf_generate(
    TARGET squawk_myproto
    LANGUAGE cpp
)

protobuf_generate(
    TARGET squawk_myproto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

install(
  TARGETS squawk_myproto
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
