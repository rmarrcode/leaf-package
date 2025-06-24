#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using leaftest::ServerCommunication;
using leaftest::TimeRequest;
using leaftest::TimeResponse;
using leaftest::ForwardPassRequest;
using leaftest::ForwardPassResponse;
using leaftest::GradientRequest;
using leaftest::GradientResponse;

class ServerCommunicationServiceImpl final : public ServerCommunication::Service {
    Status GetServerTime(ServerContext* /*context*/, const TimeRequest* /*request*/, TimeResponse* response) override {
        response->set_server_time_ms(123456789);  // fixed demo value
        return Status::OK;
    }

    Status ForwardPass(ServerContext* /*context*/, const ForwardPassRequest* request, ForwardPassResponse* response) override {
        try {
            // For now, we'll return a dummy response
            // In a real implementation, this would:
            // 1. Deserialize the model state from request->model_state()
            // 2. Deserialize input data from request->input_data()
            // 3. Deserialize target data from request->target_data()
            // 4. Load the model based on request->model_type()
            // 5. Perform forward pass and calculate gradients
            // 6. Serialize gradients and return them
            
            // Dummy implementation
            response->set_gradients("dummy_gradients");
            response->set_loss(0.5f);
            response->set_success(true);
            response->set_error_message("");
            
            return Status::OK;
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_error_message(e.what());
            return Status::OK;
        }
    }

    Status GetGradients(ServerContext* /*context*/, const GradientRequest* request, GradientResponse* response) override {
        try {
            // This is essentially the same as ForwardPass for now
            // In a real implementation, this might be used for different purposes
            // or could be optimized differently
            
            // Dummy implementation
            response->set_gradients("dummy_gradients");
            response->set_loss(0.5f);
            response->set_success(true);
            response->set_error_message("");
            
            return Status::OK;
        } catch (const std::exception& e) {
            response->set_success(false);
            response->set_error_message(e.what());
            return Status::OK;
        }
    }
};

int main(int /*argc*/, char** /*argv*/) {
    const std::string addr("0.0.0.0:50051");
    ServerCommunicationServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}

