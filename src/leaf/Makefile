# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17
INCLUDES = -I/usr/include \
           -I/usr/local/include

# Protocol Buffers and gRPC
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin

# Source files
PROTO_FILE = server_test.proto
PROTO_SRCS = server_test.pb.cc
PROTO_HDRS = server_test.pb.h
GRPC_SRCS = server_test.grpc.pb.cc
GRPC_HDRS = server_test.grpc.pb.h

# Libraries
GRPC_LIBS = -lgrpc++ -lgrpc
PROTO_LIBS = -lprotobuf

# Targets
all: $(PROTO_SRCS) $(GRPC_SRCS) server_test

# Generate Protocol Buffers code
$(PROTO_SRCS) $(PROTO_HDRS): $(PROTO_FILE)
	$(PROTOC) --cpp_out=. $(PROTO_FILE)

# Generate gRPC code
$(GRPC_SRCS) $(GRPC_HDRS): $(PROTO_FILE)
	$(PROTOC) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) $(PROTO_FILE)

# Build server_test
server_test: server_test.cpp $(PROTO_SRCS) $(GRPC_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(GRPC_LIBS) $(PROTO_LIBS)

# Clean
clean:
	rm -f $(PROTO_SRCS) $(PROTO_HDRS) $(GRPC_SRCS) $(GRPC_HDRS) server_test

.PHONY: all clean 