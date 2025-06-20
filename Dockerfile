# ---------- Stage 1 : build ----------
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy sources
COPY server_test.cpp .
COPY server_test.proto .

# Generate gRPC / protobuf sources
RUN protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin server_test.proto

# Compile
RUN g++ -std=c++17 server_test.cpp server_test.pb.cc server_test.grpc.pb.cc -lgrpc++ -lprotobuf -o server_test

# ---------- Stage 2 : runtime ----------
FROM ubuntu:22.04

# Install runtime deps
RUN apt-get update && apt-get install -y \
    libgrpc++1 \
    libgrpc10 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/server_test ./server_test

RUN chmod +x server_test

EXPOSE 50051

CMD ["./server_test"] 