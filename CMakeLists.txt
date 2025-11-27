cmake_minimum_required(VERSION 3.25)
project(DistributedOCR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------
# 1. PATHS — exactly your current vcpkg (November 2025)
# -------------------------------------------------
set(CMAKE_PREFIX_PATH "D:/Qt/6.9.2/msvc2022_64")

set(gRPC_DIR       "D:/Tools/vcpkg/installed/x64-windows/share/grpc"       CACHE PATH "")
set(Protobuf_DIR   "D:/Tools/vcpkg/installed/x64-windows/share/protobuf"   CACHE PATH "")
set(Tesseract_DIR  "D:/Tools/vcpkg/installed/x64-windows/share/tesseract"  CACHE PATH "")
set(Leptonica_DIR  "D:/Tools/vcpkg/installed/x64-windows/share/leptonica")
set(CURL_DIR       "D:/Tools/vcpkg/installed/x64-windows/share/curl"       CACHE PATH "")
set(OpenJPEG_DIR   "D:/Tools/vcpkg/installed/x64-windows/share/openjpeg"   CACHE PATH "")
set(re2_DIR        "D:/Tools/vcpkg/installed/x64-windows/share/re2"        CACHE PATH "")
set(c-ares_DIR     "D:/Tools/vcpkg/installed/x64-windows/share/c-ares"     CACHE PATH "")
set(absl_DIR       "D:/Tools/vcpkg/installed/x64-windows/share/absl"       CACHE PATH "")
set(utf8_range_DIR "D:/Tools/vcpkg/installed/x64-windows/share/utf8_range" CACHE PATH "")
set(WebP_DIR       "D:/Tools/vcpkg/installed/x64-windows/share/WebP"       CACHE PATH "")

# LibArchive uses old module → manual hints
set(LibArchive_INCLUDE_DIR "D:/Tools/vcpkg/installed/x64-windows/include" CACHE PATH "")
set(LibArchive_LIBRARY    "D:/Tools/vcpkg/installed/x64-windows/lib/libarchive.lib" CACHE PATH "")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -------------------------------------------------
# 2. FIND PACKAGES — this order is critical
# -------------------------------------------------
find_package(Qt6       REQUIRED COMPONENTS Core Widgets)
find_package(Protobuf REQUIRED)
find_package(gRPC      CONFIG REQUIRED)
find_package(Tesseract CONFIG REQUIRED)
find_package(Leptonica CONFIG REQUIRED
    PATHS "D:/Tools/vcpkg/installed/x64-windows/share/leptonica"
    NO_DEFAULT_PATH
)
find_package(CURL      CONFIG REQUIRED)
find_package(OpenJPEG  CONFIG REQUIRED)
find_package(re2       CONFIG REQUIRED)
find_package(c-ares    CONFIG REQUIRED)
find_package(absl      CONFIG REQUIRED)
find_package(WebP      CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

# -------------------------------------------------
# 3. Proto generation (modern vcpkg workflow)
# -------------------------------------------------

set(PROTO_FILE ${CMAKE_SOURCE_DIR}/proto/ocr_service.proto)
set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

# Load official protobuf generator
include("${Protobuf_DIR}/protobuf-generate.cmake")

# 3.1 Generate Protobuf messages (.pb.cc / .pb.h)
protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    ${PROTO_FILE}
)

# 3.2 Generate gRPC service stubs (.grpc.pb.cc / .grpc.pb.h)
protobuf_generate(
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    OUT_VAR GRPC_SRCS
    PROTOS ${PROTO_FILE}
    PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

set(GRPC_HDRS "")


# -------------------------------------------------
# 5. Server & Client
# -------------------------------------------------
add_executable(ocr_server
    src/server.cpp src/ocr_worker.cpp
    ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
)
target_include_directories(ocr_server PRIVATE ${GEN_DIR})
target_link_libraries(ocr_server PRIVATE
    gRPC::grpc++ gRPC::grpc++_reflection protobuf::libprotobuf
    Tesseract::libtesseract Leptonica::Leptonica
    CURL::libcurl OpenJPEG::OpenJPEG re2::re2 c-ares::cares absl::absl
    WebP::webp WebP::webpdemux WebP::webpdecoder WebP::webpmux
    LibArchive::LibArchive
)

add_executable(ocr_client WIN32
    src/client.cpp gui/main_window.cpp gui/main_window.h gui/main_window.ui
    ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
)
target_include_directories(ocr_client PRIVATE ${GEN_DIR})
target_link_libraries(ocr_client PRIVATE
    Qt6::Core Qt6::Widgets gRPC::grpc++ protobuf::libprotobuf
)
qt_finalize_executable(ocr_client)