cmake_minimum_required(VERSION 3.15)
project(aether_vlm_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Find Protobuf and gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Include generated gRPC headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pb)

# Add executable
add_executable(vlm_engine_server
    src/server.cpp
    src/kv_cache.cpp
    src/model_loader.cpp
    src/pb/inference.pb.cc
    src/pb/inference.grpc.pb.cc
)

target_link_libraries(vlm_engine_server
    protobuf::libprotobuf
    gRPC::grpc++
    pthread
)
