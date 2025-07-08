# ---------- Stage 1 : build ----------
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    libprotobuf-dev \
    python3-dev \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install pybind11
RUN pip3 install pybind11

WORKDIR /src

# Copy sources
COPY server_communication.cpp .
COPY server_communication.h .
COPY server_communication.proto .
COPY model.h .
COPY model.cpp .

# Generate gRPC / protobuf sources
RUN protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin server_communication.proto

# Compile with pybind11 include paths
RUN g++ -std=c++17 -I/usr/include/python3.10 -I/usr/local/lib/python3.10/dist-packages/pybind11/include server_communication.cpp model.cpp server_communication.pb.cc server_communication.grpc.pb.cc -lgrpc++ -lprotobuf -lpython3.10 -o server_communication && echo "Compilation successful" && ls -la server_communication

# ---------- Stage 2 : runtime ----------
FROM ubuntu:22.04

# Install runtime deps
RUN apt-get update && apt-get install -y \
    libgrpc++1 \
    libgrpc10 \
    libprotobuf23 \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install pybind11 runtime
RUN pip3 install pybind11

WORKDIR /app

COPY --from=builder /src/server_communication ./server_communication

RUN chmod +x server_communication

EXPOSE 50051

CMD ["./server_communication"]