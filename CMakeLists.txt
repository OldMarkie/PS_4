cmake_minimum_required(VERSION 3.25)
project(PS_4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Required packages
find_package(Protobuf REQUIRED CONFIG)
find_package(gRPC REQUIRED CONFIG)

# Source & generated directories
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILE ${PROTO_SRC_DIR}/service.proto)

# Include generated headers
include_directories(${PROTO_GEN_DIR})

# Function to generate both protobuf and gRPC C++ files
function(grpc_generate_cpp GRPC_SRCS GRPC_HDRS PROTO_SRCS PROTO_HDRS PROTOFILE)
    get_filename_component(PROTO_WE ${PROTOFILE} NAME_WE)

    set(GENERATED_PB_SRC ${PROTO_GEN_DIR}/${PROTO_WE}.pb.cc)
    set(GENERATED_PB_HDR ${PROTO_GEN_DIR}/${PROTO_WE}.pb.h)
    set(GENERATED_GRPC_SRC ${PROTO_GEN_DIR}/${PROTO_WE}.grpc.pb.cc)
    set(GENERATED_GRPC_HDR ${PROTO_GEN_DIR}/${PROTO_WE}.grpc.pb.h)

    add_custom_command(
        OUTPUT ${GENERATED_PB_SRC} ${GENERATED_PB_HDR} ${GENERATED_GRPC_SRC} ${GENERATED_GRPC_HDR}
        COMMAND protobuf::protoc
        ARGS --cpp_out=${PROTO_GEN_DIR}
             --grpc_out=${PROTO_GEN_DIR}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             -I ${PROTO_SRC_DIR}
             ${PROTOFILE}
        DEPENDS ${PROTOFILE}
        VERBATIM
    )

    set(${PROTO_SRCS} ${GENERATED_PB_SRC} PARENT_SCOPE)
    set(${PROTO_HDRS} ${GENERATED_PB_HDR} PARENT_SCOPE)
    set(${GRPC_SRCS} ${GENERATED_GRPC_SRC} PARENT_SCOPE)
    set(${GRPC_HDRS} ${GENERATED_GRPC_HDR} PARENT_SCOPE)
endfunction()

# Generate C++ files from proto
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# Server executable
add_executable(ps4_server
    server.cpp
    ${PROTO_SRCS}       # protobuf messages
    ${GRPC_SRCS}        # grpc service stubs
)

target_link_libraries(ps4_server
    PRIVATE
        gRPC::grpc++
        protobuf::libprotobuf
)

# Client executable
add_executable(ps4_client
    client.cpp
    ${PROTO_SRCS}       # protobuf messages
    ${GRPC_SRCS}        # grpc service stubs
)

target_link_libraries(ps4_client
    PRIVATE
        gRPC::grpc++
        protobuf::libprotobuf
)

