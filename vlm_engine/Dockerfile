FROM docker.io/library/debian:bookworm-slim AS builder

# Install build tools, cmake, Protobuf and gRPC development libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Copy CMake configuration, headers, sources, and generated proto stubs
COPY CMakeLists.txt CMakeLists.txt
COPY include include
COPY src src

# Build executable
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

FROM docker.io/library/debian:bookworm-slim

# Install runtime dependencies for gRPC and Protobuf
RUN apt-get update && apt-get install -y \
    libprotobuf-dev \
    libgrpc++-dev \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /workspace/build/vlm_engine_server .

# Mock weights file path (matching server.cpp)
RUN mkdir -p /models && touch /models/vlm-mini-8b.bin

EXPOSE 50052

CMD ["./vlm_engine_server"]
