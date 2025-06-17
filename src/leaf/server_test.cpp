#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "server_test.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using leaftest::ServerTest;
using leaftest::TimeRequest;
using leaftest::TimeResponse;

class ServerTestServiceImpl final : public ServerTest::Service {
    Status GetServerTime(ServerContext* context, const TimeRequest* request, TimeResponse* response) override {
        // For demonstration, we simply return a fixed response
        response->set_server_time_ms(123456789);
        return Status::OK;
    }
};

void RunServer(int port) {
    std::string server_address("0.0.0.0:50051");
    ServerTestServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    int port = 50051; // Default port
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    RunServer(port);
    return 0;
} 