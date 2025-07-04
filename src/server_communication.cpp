#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"
#include "server_communication.h"
#include "model.h"

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
        
        // Create a new Model object with the deserialized state
        // Note: We need to create a dummy PyTorch model here since we can't easily reconstruct it from just the weights
        // In a real implementation, you might want to store the model architecture information as well
        py::object dummy_model = py::none();  // Placeholder for now
        auto model = std::make_shared<Model>(dummy_model, nullptr);
        
        // Store the model
        stored_models[model_id] = model;
        
        std::cout << "Stored model for model ID: " << model_id 
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
        // 1. Deserialize input data from request->input_data()
        // 2. Use the stored model to perform forward pass
        // 3. Return the output
        
        // Dummy implementation
        response->set_gradients("dummy_output");
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
        // For now, we'll return a dummy response
        // In a real implementation, this would:
        // 1. Deserialize input data from request->input_data()
        // 2. Use the stored model to compute gradients
        // 3. Return the gradients and loss
        
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

// Helper methods for model management
bool ServerCommunicationServiceImpl::has_model(const std::string& model_id) const {
    return stored_models.find(model_id) != stored_models.end();
}

std::shared_ptr<Model> ServerCommunicationServiceImpl::get_model(const std::string& model_id) const {
    auto it = stored_models.find(model_id);
    if (it != stored_models.end()) {
        return it->second;
    }
    return nullptr;
}

void ServerCommunicationServiceImpl::store_model(const std::string& model_id, std::shared_ptr<Model> model) {
    stored_models[model_id] = model;
}

void ServerCommunicationServiceImpl::remove_model(const std::string& model_id) {
    stored_models.erase(model_id);
}

std::vector<std::string> ServerCommunicationServiceImpl::get_stored_model_ids() const {
    std::vector<std::string> ids;
    for (const auto& pair : stored_models) {
        ids.push_back(pair.first);
    }
    return ids;
}

int main(int /*argc*/, char** /*argv*/) {
    const std::string addr("0.0.0.0:50051");
    ServerCommunicationServiceImpl service;
    ServerBuilder builder;
    
    // Configure server with increased message size limits
    // Default is 4MB, we'll set it to 100MB to handle large model weights
    builder.AddChannelArgument(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, 100 * 1024 * 1024);  // 100MB
    builder.AddChannelArgument(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, 100 * 1024 * 1024);      // 100MB
    
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}

