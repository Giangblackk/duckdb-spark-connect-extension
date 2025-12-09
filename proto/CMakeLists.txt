# find Protobuf and gRPC libraries
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_FILES
    myproto/sample.proto
)

set(PROTO_TARGET_NAME squawk_proto)

# configure to compile/generate protobuf c++ code and headers
add_library(${PROTO_TARGET_NAME} STATIC ${PROTO_FILES})

target_link_libraries(${PROTO_TARGET_NAME} PUBLIC protobuf::libprotobuf gRPC::grpc gRPC::grpc++)

target_include_directories(${PROTO_TARGET_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

protobuf_generate(
    TARGET ${PROTO_TARGET_NAME}
    LANGUAGE cpp
)

protobuf_generate(
    TARGET ${PROTO_TARGET_NAME}
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

install(
  TARGETS ${PROTO_TARGET_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
