# A user should never have to build the server_communication binary.

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 $(shell pkg-config --cflags protobuf grpc++ )
LDFLAGS = $(shell pkg-config --libs protobuf grpc++ )

# Protocol Buffers and gRPC
PROTOC = protoc
GRPC_CPP_PLUGIN = /opt/homebrew/bin/grpc_cpp_plugin

# Source files
PROTO_FILE = server_communication.proto
PROTO_SRCS = server_communication.pb.cc
PROTO_HDRS = server_communication.pb.h
GRPC_SRCS = server_communication.grpc.pb.cc
GRPC_HDRS = server_communication.grpc.pb.h

# Additional source files
USER_CREDENTIALS_SRCS = user_credentials.cpp
SERVER_SRCS = server.cpp
MODEL_SRCS = model.cpp
DISTRIBUTED_MODEL_SRCS = distributed_model.cpp
CRITERION_SRCS = criterion.cpp

# Targets
all: $(PROTO_SRCS) $(GRPC_SRCS) server_communication

# Generate Protocol Buffers code
$(PROTO_SRCS) $(PROTO_HDRS): $(PROTO_FILE)
	$(PROTOC) --cpp_out=. $(PROTO_FILE)

# Generate gRPC code
$(GRPC_SRCS) $(GRPC_HDRS): $(PROTO_FILE)
	$(PROTOC) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) $(PROTO_FILE)

# Build server_communication binary
server_communication: $(PROTO_SRCS) $(GRPC_SRCS) $(USER_CREDENTIALS_SRCS) $(SERVER_SRCS) $(MODEL_SRCS) $(DISTRIBUTED_MODEL_SRCS) $(CRITERION_SRCS) server_communication.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean
clean:
	rm -f $(PROTO_SRCS) $(PROTO_HDRS) $(GRPC_SRCS) $(GRPC_HDRS) server_communication

.PHONY: all clean 