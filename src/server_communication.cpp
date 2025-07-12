#include <iostream>
#include <memory>
#include <string>
#include <cstdint>
#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"
#include "server_communication.h"
#include "model.h"
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

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

namespace py = pybind11;

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
        uint32_t model_index = request->model_index();
        std::cout << "ForwardPass: Starting for model index " << model_index << std::endl;
        
        // Get the model ID based on the index
        std::string model_id = "model_" + std::to_string(model_index);
        std::cout << "ForwardPass: Looking for model_id: " << model_id << std::endl;
        
        // Check if the model exists
        if (!has_model(model_id)) {
            std::cout << "ForwardPass: ERROR - Model " << model_id << " not found" << std::endl;
            response->set_success(false);
            response->set_error_message("Model with index " + std::to_string(model_index) + " not found");
            return Status::OK;
        }
        std::cout << "ForwardPass: Model " << model_id << " found" << std::endl;
        
        // Get the model
        auto model = get_model(model_id);
        if (!model) {
            std::cout << "ForwardPass: ERROR - Failed to retrieve model " << model_id << std::endl;
            response->set_success(false);
            response->set_error_message("Failed to retrieve model with index " + std::to_string(model_index));
            return Status::OK;
        }
        std::cout << "ForwardPass: Model " << model_id << " retrieved successfully" << std::endl;
        
        // Deserialize input data from request into a numpy array
        const std::string& input_bytes = request->input_data();
        if (input_bytes.empty()) {
            std::cout << "ForwardPass: ERROR - No input data provided" << std::endl;
            response->set_success(false);
            response->set_error_message("No input data provided");
            return Status::OK;
        }
        std::cout << "ForwardPass: Input data size: " << input_bytes.size() << " bytes" << std::endl;
        
        try {
            py::object numpy = py::module_::import("numpy");
            std::cout << "ForwardPass: NumPy imported successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "ForwardPass: ERROR - Failed to import numpy: " << e.what() << std::endl;
            response->set_success(false);
            response->set_error_message("Failed to import numpy: " + std::string(e.what()));
            return Status::OK;
        }
        
        try {
            py::object torch = py::module_::import("torch");
            std::cout << "ForwardPass: PyTorch imported successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "ForwardPass: ERROR - Failed to import torch: " << e.what() << std::endl;
            response->set_success(false);
            response->set_error_message("Failed to import torch: " + std::string(e.what()));
            return Status::OK;
        }
        
        py::object numpy = py::module_::import("numpy");
        py::object torch = py::module_::import("torch");
        
        py::object input_tensor;
        try {
            // Create numpy array from raw bytes (float32)
            py::object np_float32 = numpy.attr("dtype")("float32");
            py::object input_array = numpy.attr("frombuffer")(py::bytes(input_bytes), np_float32);
            std::cout << "ForwardPass: Input array created from bytes successfully" << std::endl;
            
            // Convert numpy array to torch tensor
            input_tensor = torch.attr("from_numpy")(input_array).attr("float")();
            std::cout << "ForwardPass: Input tensor created successfully" << std::endl;
            
            // Ensure batch dimension exists (if caller sent flat tensor we treat it as batch size 1)
            if (input_tensor.attr("dim")().cast<int>() == 1) {
                input_tensor = input_tensor.attr("unsqueeze")(0);
                std::cout << "ForwardPass: Added batch dimension to input tensor" << std::endl;
            }
            
            std::cout << "ForwardPass: Input tensor shape: " << py::str(input_tensor.attr("shape")).cast<std::string>() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "ForwardPass: ERROR - Failed to create input tensor: " << e.what() << std::endl;
            response->set_success(false);
            response->set_error_message("Failed to create input tensor: " + std::string(e.what()));
            return Status::OK;
        }
        
        try {
            // Run forward pass using the stored PyTorch model
            py::object pytorch_model = model->get_pytorch_model();
            std::cout << "ForwardPass: Retrieved PyTorch model successfully" << std::endl;
            
            py::object output_tensor = pytorch_model.attr("forward")(input_tensor);
            std::cout << "ForwardPass: Forward pass completed successfully" << std::endl;
            
            // Just return true if the forward pass worked
            response->set_success(true);
            response->set_error_message("");
            
            std::cout << "ForwardPass: Completed successfully for model index " << model_index << std::endl;
            
            return Status::OK;
        } catch (const std::exception& e) {
            std::cout << "ForwardPass: ERROR - Forward pass failed: " << e.what() << std::endl;
            response->set_success(false);
            response->set_error_message("Forward pass failed: " + std::string(e.what()));
            return Status::OK;
        }
    } catch (const std::exception& e) {
        std::cout << "ForwardPass: ERROR - Unexpected error: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Unexpected error: " + std::string(e.what()));
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


std::vector<float> ServerCommunicationServiceImpl::get_outputs(const std::string& model_id) const {
    auto it = model_outputs.find(model_id);
    if (it != model_outputs.end()) {
        return it->second;
    }
    return {};
}

int main(int /*argc*/, char** /*argv*/) {
    const std::string addr("0.0.0.0:50051");
    ServerCommunicationServiceImpl service;
    ServerBuilder builder;
    
    // Configure server with increased message size limits
    // Default is 4MB, we'll set it to 100MB to handle large model weights
    builder.AddChannelArgument(GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH, 100 * 1024 * 1024);  // 100MB
    builder.AddChannelArgument(GRPC_ARG_MAX_SEND_MESSAGE_LENGTH, 100 * 1024 * 1024);      // 100MB
    
    // Add keepalive settings for better connection stability
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 30000);  // 30 seconds
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);  // 10 seconds
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
    builder.AddChannelArgument(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 5000);
    
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}

