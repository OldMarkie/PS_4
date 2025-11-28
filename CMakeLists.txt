cmake_minimum_required(VERSION 3.25)
project(PS_4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# --------------------------------------------------
# vcpkg toolchain - MUST be before any find_package
# --------------------------------------------------
set(CMAKE_TOOLCHAIN_FILE "D:/Tools/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "vcpkg toolchain")

# --------------------------------------------------
# Qt (installed manually, NOT via vcpkg)
# --------------------------------------------------
# Append Qt root directory to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.9.2/msvc2022_64")

# --------------------------------------------------
# Required packages
# --------------------------------------------------
find_package(Protobuf REQUIRED CONFIG)
find_package(gRPC REQUIRED CONFIG)
find_package(OpenCV CONFIG REQUIRED)
find_package(Tesseract CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets Gui REQUIRED)

# Proto paths
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILE ${PROTO_SRC_DIR}/service.proto)

include_directories(${PROTO_GEN_DIR})

# Generate grpc + protobuf
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

grpc_generate_cpp(GRPC_SRCS GRPC_HDRS PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# Server
add_executable(ps4_server
    server.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(ps4_server
    PRIVATE
        gRPC::grpc++
        protobuf::libprotobuf
        Tesseract::libtesseract
        ${OpenCV_LIBS}
)

# Client
add_executable(ps4_client
    client.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(ps4_client
    PRIVATE
        gRPC::grpc++
        protobuf::libprotobuf
        Tesseract::libtesseract
        ${OpenCV_LIBS}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
