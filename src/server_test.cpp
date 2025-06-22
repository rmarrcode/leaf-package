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
    Status GetServerTime(ServerContext* /*context*/, const TimeRequest* /*request*/, TimeResponse* response) override {
        response->set_server_time_ms(123456789);  // fixed demo value
        return Status::OK;
    }
};

int main(int /*argc*/, char** /*argv*/) {
    const std::string addr("0.0.0.0:50051");
    ServerTestServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}