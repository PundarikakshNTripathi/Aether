cmake_minimum_required(VERSION 3.15)
project(aether_search_core CXX)

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
add_executable(search_core_server
    src/server.cpp
    src/bm25_index.cpp
    src/hnsw_graph.cpp
    src/pb/crawler.pb.cc
    src/pb/crawler.grpc.pb.cc
    src/pb/search.pb.cc
    src/pb/search.grpc.pb.cc
)

target_link_libraries(search_core_server
    protobuf::libprotobuf
    gRPC::grpc++
    pthread
)
