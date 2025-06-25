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
COPY server_communication.cpp .
COPY server_communication.h .
COPY server_communication.proto .

# Generate gRPC / protobuf sources
RUN protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin server_communication.proto

# Compile
RUN g++ -std=c++17 server_communication.cpp server_communication.pb.cc server_communication.grpc.pb.cc -lgrpc++ -lprotobuf -o server_communication

# ---------- Stage 2 : runtime ----------
FROM ubuntu:22.04

# Install runtime deps
RUN apt-get update && apt-get install -y \
    libgrpc++1 \
    libgrpc10 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/server_communication ./server_communication

RUN chmod +x server_communication

EXPOSE 50051

CMD ["./server_communication"] 