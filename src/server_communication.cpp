#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"
#include "server_communication.h"

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
using leaftest::StoreModelWeightsRequest;
using leaftest::StoreModelWeightsResponse;

Status ServerCommunicationServiceImpl::GetServerTime(ServerContext* /*context*/, const TimeRequest* /*request*/, TimeResponse* response) {
    response->set_server_time_ms(123456789);  // fixed demo value
    return Status::OK;
}

Status ServerCommunicationServiceImpl::StoreModelWeights(ServerContext* /*context*/, const StoreModelWeightsRequest* request, StoreModelWeightsResponse* response) {
    try {
        std::string model_id = request->model_id();
        
        // Deserialize model state from bytes to vector<float>
        const std::string& model_state_bytes = request->model_state();
        size_t num_floats = model_state_bytes.size() / sizeof(float);
        std::vector<float> model_state(num_floats);
        
        if (model_state_bytes.size() > 0) {
            std::memcpy(model_state.data(), model_state_bytes.data(), model_state_bytes.size());
        }
        
        // Store the model weights
        stored_models[model_id] = model_state;
        
        std::cout << "Stored model weights for model ID: " << model_id 
                  << ", size: " << model_state.size() << " parameters" << std::endl;
        
        response->set_success(true);
        response->set_error_message("");
        response->set_model_id(model_id);
        
        return Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_error_message(e.what());
        response->set_model_id(request->model_id());
        return Status::OK;
    }
}

Status ServerCommunicationServiceImpl::ForwardPass(ServerContext* /*context*/, const ForwardPassRequest* request, ForwardPassResponse* response) {
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

Status ServerCommunicationServiceImpl::GetGradients(ServerContext* /*context*/, const GradientRequest* request, GradientResponse* response) {
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

int main(int /*argc*/, char** /*argv*/) {
    const std::string addr("0.0.0.0:50051");
    ServerCommunicationServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}

