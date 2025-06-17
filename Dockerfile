# Use a multi-architecture base image
FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libgrpc++-dev \
    libgrpc-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Print current directory and list files
RUN pwd && ls -la

# Copy source files from leaf directory
COPY src/leaf/server_test.cpp .
COPY src/leaf/server_test.proto .
COPY src/leaf/Makefile .

# Print files after copy
RUN pwd && ls -la

# Build the server
RUN make clean && make

# Verify build artifacts
RUN ls -la

# Expose the port (default 50051, but can be overridden)
EXPOSE 50051

# Run the server
CMD ["./server_test"] 