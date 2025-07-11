#include "core.h"
#include "distributed_model.h"
#include "server_communication.h"
#include <algorithm>
#include <cstring>

namespace py = pybind11;

// LeafConfig implementation
LeafConfig::LeafConfig() {
    discover_local_resources();
}

void LeafConfig::add_server(const std::string& server_name,
                           const std::string& username,
                           const std::string& hostname,
                           int port,
                           const std::string& key_path) {
    UserCredentials creds(username, hostname, port, key_path);
    servers[server_name] = Server(server_name, creds, false);
}

std::vector<std::string> LeafConfig::get_servers() const {
    std::vector<std::string> server_names;
    for (const auto& [name, server] : servers) {
        server_names.push_back(name);
    }
    return server_names;
}

void LeafConfig::discover_local_resources() {
    // Create a dummy UserCredentials for localhost
    UserCredentials local_creds("", "", 0, "");
    servers["localhost"] = Server("localhost", local_creds, true);
}

void LeafConfig::print_server_info(const Server& server) const {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "Server: " << server.get_name() << "\n";
    std::cout << std::string(50, '=') << "\n";
    
    // Print server details
    std::cout << "Status: " << (server.is_server_connected() ? "Connected" : "Not connected") << "\n";
    if (server.is_local_server()) {
        std::cout << "Type: Local machine\n";
    } else {
        std::cout << "Type: Remote server\n";
        const UserCredentials& creds = server.get_credentials();
        std::cout << "Username: " << creds.get_username() << "\n";
        std::cout << "Hostname: " << creds.get_hostname() << "\n";
        std::cout << "Port: " << creds.get_port() << "\n";
        if (!creds.get_key_path().empty()) {
            std::cout << "SSH Key: " << creds.get_key_path() << "\n";
        }
    }
    
    // Print resources
    const auto& resources = server.get_resources();
    if (!resources.empty()) {
        std::cout << "\nAvailable Resources (" << resources.size() << "):\n";
        std::cout << std::string(50, '-') << "\n";
        
        for (size_t i = 0; i < resources.size(); ++i) {
            const auto& resource = resources[i];
            std::cout << "\nResource " << (i + 1) << ":\n";
            std::cout << "  Name: " << resource.get_name() << "\n";
            std::cout << "  Type: " << resource.get_type() << "\n";
            std::cout << "  Properties:\n";
            for (const auto& [key, value] : resource.get_properties()) {
                std::cout << "    " << key << ": " << value << "\n";
            }
        }
    } else {
        std::cout << "\nNo computational resources found\n";
    }
    
    std::cout << "\n" << std::string(50, '-') << "\n";
}

py::dict LeafConfig::get_server_info(const std::string& server_name) const {
    py::dict info;
    auto it = servers.find(server_name);
    if (it != servers.end()) {
        const Server& server = it->second;
        info["name"] = server.get_name();
        info["connected"] = server.is_server_connected();
        info["is_local"] = server.is_local_server();
        
        const UserCredentials& creds = server.get_credentials();
        std::cout << "Debug: Server " << server_name << " has hostname: " << creds.get_hostname() << " and port: " << creds.get_port() << std::endl;
        info["username"] = creds.get_username();
        info["hostname"] = creds.get_hostname();
        info["port"] = creds.get_port();
        info["key_path"] = creds.get_key_path();

        py::list resources;
        for (const auto& resource : server.get_resources()) {
            py::dict res_info;
            res_info["name"] = resource.get_name();
            res_info["type"] = resource.get_type();
            res_info["properties"] = resource.get_properties();
            resources.append(res_info);
        }
        info["resources"] = resources;
    }
    return info;
}

void LeafConfig::remove_server(const std::string& server_name) {
    if (server_name != "localhost") {  // Prevent removal of localhost
        servers.erase(server_name);
    }
}

void LeafConfig::print_all_resources() const {
    std::cout << "\n=== Available Servers and Resources ===\n";
    
    for (const auto& [name, server] : servers) {
        print_server_info(server);
    }
    
    std::cout << "\n=== End of Server List ===\n\n";
}

std::pair<int, std::string> LeafConfig::get_server_connection_info(const std::string& server_name) const {
    auto it = servers.find(server_name);
    if (it != servers.end()) {
        const Server& server = it->second;
        if (server.is_local_server()) {
            // For localhost, return -1 for PID and default gRPC port
            return {-1, "localhost:50051"};
        } else {
            // For remote servers, get tunnel info from credentials
            const UserCredentials& creds = server.get_credentials();
            int tunnel_pid = creds.get_tunnel_pid();
            int tunnel_port = creds.get_tunnel_port();
            
            std::string server_address;
            if (tunnel_port > 0) {
                server_address = "localhost:" + std::to_string(tunnel_port);
            } else {
                // Fallback to default port if tunnel port is not available
                server_address = "localhost:50051";
            }
            
            return {tunnel_pid, server_address};
        }
    }
    // Return default values if server not found
    return {-1, "localhost:50051"};
}

// LeafTrainer implementation
LeafTrainer::LeafTrainer(const LeafConfig& cfg) : config(cfg) {
    // gRPC is automatically initialized when needed
}

LeafTrainer::~LeafTrainer() {}

std::shared_ptr<grpc::Channel> LeafTrainer::create_channel(const std::string& server_name) {
    const auto& servers = config.get_servers();
    auto it = std::find(servers.begin(), servers.end(), server_name);
    if (it == servers.end()) {
        throw std::runtime_error("Server " + server_name + " not found in configuration");
    }

    // Get the server address using the tunnel port from user credentials
    std::string server_address = config.get_server_connection_info(server_name).second;
    std::cout << "Creating gRPC channel for server '" << server_name << "' at address: " << server_address << std::endl;
    
    // Configure channel with increased message size limits to handle large model weights
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(100 * 1024 * 1024);  // 100MB
    args.SetMaxSendMessageSize(100 * 1024 * 1024);     // 100MB
    
    auto channel = grpc::CreateCustomChannel(server_address, grpc::InsecureChannelCredentials(), args);
    return channel;
}

std::pair<std::vector<float>, float> LeafTrainer::get_gradients_from_server(
    const std::string& server_name,
    py::object inputs,
    py::object model,
    py::object criterion,
    py::object optimizer,
    bool is_local) {

    std::cout << "  inputs: " << py::str(inputs.get_type()) << std::endl;
    std::cout << "  inputs repr: " << py::str(inputs) << std::endl;

    // Extract model state from PyTorch model
    py::object state_dict = model.attr("state_dict")();
    std::vector<float> model_state;

    // Convert state dict to flat vector
    std::cout << "  state_dict type: " << py::str(state_dict.get_type()) << std::endl;
    std::cout << "  state_dict repr: " << py::str(state_dict) << std::endl;
    
    // Iterate over the state dict items properly
    py::object items = state_dict.attr("items")();
    std::cout << "  items type: " << py::str(items.get_type()) << std::endl;
    std::cout << "  items repr: " << py::str(items) << std::endl;
    
    std::cout << "before loop" << std::endl;
    try {
        for (auto item : items) {
            std::cout << "  Item type: " << py::str(item.get_type()) << std::endl;
            std::cout << "  Item repr: " << py::str(item) << std::endl;

            // Extract the tensor value from the key-value pair
            py::object tensor = item.attr("__getitem__")(1);  // Get the value (tensor) from the key-value pair
            std::cout << "  Tensor type: " << py::str(tensor.get_type()) << std::endl;
            
            // Check if tensor has cpu() method
            if (py::hasattr(tensor, "cpu")) {
                py::object cpu_tensor = tensor.attr("cpu")();
                if (py::hasattr(cpu_tensor, "numpy")) {
                    py::array_t<float> numpy_array = cpu_tensor.attr("numpy")();
                    auto buffer = numpy_array.unchecked<1>();
                    for (py::ssize_t i = 0; i < buffer.size(); ++i) {
                        model_state.push_back(buffer[i]);
                    }
                } else {
                    std::cout << "  Warning: CPU tensor does not have numpy() method" << std::endl;
                }
            } else {
                std::cout << "  Warning: Tensor does not have cpu() method" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "  Error during state dict iteration: " << e.what() << std::endl;
        throw;
    }
    
    // Extract input data
    std::cout << "  inputs type: " << py::str(inputs.get_type()) << std::endl;
    std::cout << "  inputs repr: " << py::str(inputs) << std::endl;
    py::array_t<float> inputs_array = inputs.attr("cpu")().attr("numpy")();
    auto inputs_buffer = inputs_array.unchecked<1>();
    std::vector<float> input_data;
    for (py::ssize_t i = 0; i < inputs_buffer.size(); ++i) {
        input_data.push_back(inputs_buffer[i]);
    }
    
    if (is_local) {
        // For local servers, directly use the GetGradients function from server_communication.cpp
        std::cout << "  Computing gradients locally (using GetGradients from server_communication)..." << std::endl;
        
        // Create an instance of ServerCommunicationServiceImpl to use its GetGradients method
        ServerCommunicationServiceImpl service;
        
        // Create the request and response objects
        leaftest::GradientRequest request;
        leaftest::GradientResponse response;
        
        // Set the request data
        request.set_model_state(model_state.data(), model_state.size() * sizeof(float));
        request.set_input_data(input_data.data(), input_data.size() * sizeof(float));
        
        // Call the GetGradients method directly
        grpc::ServerContext context;
        auto status = service.GetGradients(&context, &request, &response);
        
        if (!status.ok()) {
            throw std::runtime_error("Local GetGradients failed: " + status.error_message());
        }
        
        if (!response.success()) {
            throw std::runtime_error("Local GetGradients failed: " + response.error_message());
        }
        
        // Parse gradients from response
        std::vector<float> gradients;
        const std::string& gradients_data = response.gradients();
        size_t num_gradients = gradients_data.size() / sizeof(float);
        gradients.resize(num_gradients);
        std::memcpy(gradients.data(), gradients_data.data(), gradients_data.size());
        
        std::cout << "  Local computation completed" << std::endl;
        return {gradients, response.loss()};
    }
    
    std::lock_guard<std::mutex> lock(channel_mutex);
    
    // Get or create channel for this server
    if (server_channels.find(server_name) == server_channels.end()) {
        server_channels[server_name] = create_channel(server_name);
    }
    
    auto channel = server_channels[server_name];
    auto stub = leaftest::ServerCommunication::NewStub(channel);
    
    // Prepare request
    leaftest::GradientRequest request;
    request.set_model_state(model_state.data(), model_state.size() * sizeof(float));
    request.set_input_data(input_data.data(), input_data.size() * sizeof(float));
    
    // Make RPC call
    grpc::ClientContext context;
    leaftest::GradientResponse response;
    
    auto status = stub->GetGradients(&context, request, &response);
    
    if (!status.ok()) {
        throw std::runtime_error("RPC failed for server " + server_name + ": " + status.error_message());
    }
    
    if (!response.success()) {
        throw std::runtime_error("Server " + server_name + " failed: " + response.error_message());
    }
    
    // Parse gradients from response
    std::vector<float> gradients;
    const std::string& gradients_data = response.gradients();
    size_t num_gradients = gradients_data.size() / sizeof(float);
    gradients.resize(num_gradients);
    std::memcpy(gradients.data(), gradients_data.data(), gradients_data.size());
    
    return {gradients, response.loss()};
}

py::object LeafTrainer::forward_pass_on_server(
    const std::string& server_name,
    py::object inputs,
    uint32_t model_index,
    bool is_local) {
    
    try {
        if (is_local) {
            // For local server, use the local_models array directly
            std::lock_guard<std::mutex> lock(models_mutex);
            
            if (model_index >= local_models.size()) {
                throw std::runtime_error("Model with index " + std::to_string(model_index) + " not found locally");
            }
            
            auto model = local_models[model_index];
            if (!model) {
                throw std::runtime_error("Failed to retrieve local model with index " + std::to_string(model_index));
            }
            
            // Call the underlying PyTorch model's forward method directly
            py::object pytorch_model = model->get_pytorch_model();
            return pytorch_model.attr("forward")(inputs);
        }
        
        // For remote servers, make RPC call
        std::lock_guard<std::mutex> lock(channel_mutex);
        
        // Get or create channel for this server
        if (server_channels.find(server_name) == server_channels.end()) {
            server_channels[server_name] = create_channel(server_name);
        }
        
        auto channel = server_channels[server_name];
        auto stub = leaftest::ServerCommunication::NewStub(channel);
        
        // Serialize input data
        py::object torch = py::module_::import("torch");
        py::object numpy = py::module_::import("numpy");
        
        // Convert input to numpy array and then to bytes
        py::array_t<float> input_array;
        if (py::hasattr(inputs, "cpu")) {
            py::object cpu_input = inputs.attr("cpu")();
            if (py::hasattr(cpu_input, "numpy")) {
                input_array = cpu_input.attr("numpy")();
            } else {
                throw std::runtime_error("Input tensor does not have numpy() method");
            }
        } else {
            throw std::runtime_error("Input tensor does not have cpu() method");
        }
        
        // Prepare request
        leaftest::ForwardPassRequest request;
        request.set_input_data(input_array.data(), input_array.size() * sizeof(float));
        request.set_model_index(model_index);
        
        // Make RPC call
        grpc::ClientContext context;
        leaftest::ForwardPassResponse response;
        
        auto status = stub->ForwardPass(&context, request, &response);
        
        if (!status.ok()) {
            throw std::runtime_error("Forward pass RPC failed for server " + server_name + ": " + status.error_message());
        }
        
        if (!response.success()) {
            throw std::runtime_error("Server " + server_name + " forward pass failed: " + response.error_message());
        }
        
        // For now, return a dummy tensor since we're not properly serializing the output
        // In a real implementation, you'd deserialize the response.gradients() back to a tensor
        return torch.attr("randn")(1, 10);  // Placeholder output
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Forward pass failed: " + std::string(e.what()));
    }
}

std::vector<std::pair<std::string, std::vector<size_t>>> LeafTrainer::distribute_batch(
    const std::vector<std::string>& server_names,
    size_t batch_size) {
    
    std::vector<std::pair<std::string, std::vector<size_t>>> distribution;
    
    // Simple round-robin distribution
    for (size_t i = 0; i < batch_size; ++i) {
        std::string server = server_names[i % server_names.size()];
        
        // Find if this server is already in distribution
        auto it = std::find_if(distribution.begin(), distribution.end(),
            [&server](const auto& pair) { return pair.first == server; });
        
        if (it != distribution.end()) {
            it->second.push_back(i);
        } else {
            distribution.push_back({server, {i}});
        }
    }
    
    return distribution;
}

void LeafTrainer::cleanup_models() {
    std::lock_guard<std::mutex> lock(models_mutex);
    std::cout << "Cleaning up " << local_models.size() << " local models..." << std::endl;
    local_models.clear();
    distributed_models.clear();
    std::cout << "Model cleanup completed." << std::endl;
}

size_t LeafTrainer::get_model_count() const {
    std::lock_guard<std::mutex> lock(models_mutex);
    return local_models.size();
}

std::vector<std::string> LeafTrainer::get_server_names() const {
    return config.get_servers();
}

py::dict LeafTrainer::get_server_info(const std::string& server_name) const {
    return config.get_server_info(server_name);
}

std::pair<bool, std::string> LeafTrainer::store_model_weights_on_server(
    const std::string& server_name,
    const std::vector<float>& model_state,
    const std::string& model_id) {
    
    try {
        std::lock_guard<std::mutex> lock(channel_mutex);
        
        // Get or create channel for this server
        if (server_channels.find(server_name) == server_channels.end()) {
            server_channels[server_name] = create_channel(server_name);
        }
        
        auto channel = server_channels[server_name];
        auto stub = leaftest::ServerCommunication::NewStub(channel);
        
        // Prepare request
        leaftest::StoreModelWeightsRequest request;
        request.set_model_state(model_state.data(), model_state.size() * sizeof(float));
        request.set_model_id(model_id);
        
        // Make RPC call
        grpc::ClientContext context;
        leaftest::StoreModelWeightsResponse response;
        
        auto status = stub->StoreModelWeights(&context, request, &response);
        
        if (!status.ok()) {
            return {false, "RPC failed: " + status.error_message()};
        }
        
        if (!response.success()) {
            return {false, "Server failed: " + response.error_message()};
        }
        
        return {true, ""};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

py::object LeafTrainer::register_model(py::object model) {
    std::cout << "Registering model with LeafTrainer..." << std::endl;
    // Create a Model instance that wraps the PyTorch model using smart pointer
    auto leaf_model = std::make_shared<Model>(model, this);
    
    // Get the model index before adding to local_models
    size_t model_index;
    {
        std::lock_guard<std::mutex> lock(models_mutex);
        model_index = local_models.size();
        local_models.push_back(leaf_model);
    }
    
    // Extract model state from PyTorch model using the new serialize_state method
    std::vector<float> model_state = leaf_model->serialize_state();
    std::cout << "Model state extracted, size: " << model_state.size() << " parameters" << std::endl;
    
    // Distribute model to all servers
    auto server_names = config.get_servers();
    std::cout << "Distributing model to " << server_names.size() << " servers..." << std::endl;
    for (const auto& server_name : server_names) {
        try {
            py::dict server_info = config.get_server_info(server_name);
            bool is_local = server_info["is_local"].cast<bool>();
            bool is_connected = server_info["connected"].cast<bool>();
            if (!is_connected) {
                std::cout << "Skipping server " << server_name << " - not connected" << std::endl;
                continue;
            }
            std::cout << "Storing model on server: " << server_name << std::endl;
            if (is_local) {
                ServerCommunicationServiceImpl service;
                std::string model_key = "model_" + std::to_string(model_index);
                service.store_model(model_key, leaf_model);
                std::cout << "✓ Model stored locally successfully" << std::endl;
            } else {
                std::lock_guard<std::mutex> lock(channel_mutex);
                if (server_channels.find(server_name) == server_channels.end()) {
                    server_channels[server_name] = create_channel(server_name);
                }
                auto channel = server_channels[server_name];
                auto stub = leaftest::ServerCommunication::NewStub(channel);
                leaftest::StoreModelWeightsRequest request;
                request.set_model_state(model_state.data(), model_state.size() * sizeof(float));
                request.set_model_id("model_" + std::to_string(model_index));
                grpc::ClientContext context;
                leaftest::StoreModelWeightsResponse response;
                auto status = stub->StoreModelWeights(&context, request, &response);
                if (!status.ok()) {
                    std::cout << "Warning: RPC failed for server " << server_name << ": " << status.error_message() << std::endl;
                } else if (!response.success()) {
                    std::cout << "Warning: Server " << server_name << " failed: " << response.error_message() << std::endl;
                } else {
                    std::cout << "✓ Model stored on server " << server_name << " successfully" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Error storing model on server " << server_name << ": " << e.what() << std::endl;
        }
    }
    
    // Create a DistributedModel and store it
    std::shared_ptr<DistributedModel> dist_model;
    {
        std::lock_guard<std::mutex> lock(models_mutex);
        dist_model = std::make_shared<DistributedModel>(leaf_model, this, model_index);
        distributed_models.push_back(dist_model);
    }
    py::object model_obj = py::cast(dist_model);
    std::cout << "Model registered successfully! Total models: " << local_models.size() << std::endl;
    return model_obj;
}

py::dict LeafTrainer::train(py::object model, 
    py::object optimizer, 
    py::object train_loader, 
    int epochs,
    py::object criterion) {        
    
    std::cout << "=== Testing get_gradients_from_server function ===" << std::endl;
    
    // Get available servers
    auto server_names = config.get_servers();
    
    // Results dictionary to return
    py::dict results;
    py::list server_results;
    
    for (const auto& server_name : server_names) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "Testing server: " << server_name << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        py::dict server_result;
        server_result["server_name"] = server_name;
        
        try {
            // Get server info to check if it's local
            py::dict server_info = config.get_server_info(server_name);
            bool is_local = server_info["is_local"].cast<bool>();
            bool is_connected = server_info["connected"].cast<bool>();
            
            std::cout << "Server type: " << (is_local ? "Local" : "Remote") << std::endl;
            std::cout << "Connection status: " << (is_connected ? "Connected" : "Not connected") << std::endl;
            
            server_result["is_local"] = is_local;
            server_result["is_connected"] = is_connected;
            
            if (!is_connected) {
                std::cout << "Skipping server " << server_name << " - not connected" << std::endl;
                server_result["success"] = false;
                server_result["error"] = "Server not connected";
                server_results.append(server_result);
                continue;
            }
            
            std::cout << "Getting sample batch from train loader..." << std::endl;
            
            // Get a sample batch from the train loader for testing
            py::object train_iter = train_loader.attr("__iter__")();
            std::cout << "Created train iterator" << std::endl;
            
            py::tuple batch = train_iter.attr("__next__")();
            std::cout << "Got batch from iterator" << std::endl;
            
            py::object inputs = batch[0];
            py::object targets = batch[1];
            std::cout << "Extracted inputs and targets from batch" << std::endl;
            
            std::pair<std::vector<float>, float> result;
            
            std::cout << "Calling get_gradients_from_server..." << std::endl;
            result = get_gradients_from_server(server_name,
                inputs,
                model,
                criterion,
                optimizer,
                is_local);

            // Print results
            std::cout << "✓ Gradient computation successful!" << std::endl;
            std::cout << "  Loss: " << result.second << std::endl;
            std::cout << "  Gradients size: " << result.first.size() << " elements" << std::endl;
            
            // Print first few gradients as a sample
            std::cout << "  Sample gradients: ";
            for (size_t i = 0; i < std::min(size_t(5), result.first.size()); ++i) {
                std::cout << result.first[i];
                if (i < std::min(size_t(4), result.first.size() - 1)) {
                    std::cout << ", ";
                }
            }
            if (result.first.size() > 5) {
                std::cout << ", ...";
            }
            std::cout << std::endl;
            
            // Store results
            server_result["success"] = true;
            server_result["loss"] = result.second;
            server_result["gradients_size"] = result.first.size();
            
            // Convert gradients to Python list
            py::list gradients_list;
            for (const auto& grad : result.first) {
                gradients_list.append(grad);
            }
            server_result["gradients"] = gradients_list;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Error testing server " << server_name << ": " << e.what() << std::endl;
            server_result["success"] = false;
            server_result["error"] = e.what();
        }
        
        server_results.append(server_result);
    }
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Gradient testing completed!" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Return results
    results["server_results"] = server_results;
    results["total_servers"] = server_names.size();
    return results;
}

py::dict LeafTrainer::test_with_hardcoded_values() {
    std::cout << "=== Testing get_gradients_from_server with hardcoded values ===" << std::endl;
    
    // Get available servers
    auto server_names = config.get_servers();
    
    // Results dictionary to return
    py::dict results;
    py::list server_results;
    
    // Create hardcoded input data (simplified version of the tensor you provided)
    // This represents a 4D tensor with shape [batch_size, channels, height, width]
    // Using a smaller subset for testing
    std::vector<float> input_data = {
        // Batch 0, Channel 0, 2x2 patch
        -2.4291e+00, -2.4291e+00,
        -2.4291e+00, -2.4291e+00,
        
        // Batch 0, Channel 1, 2x2 patch  
        -2.4183e+00, -2.4183e+00,
        -2.4183e+00, -2.4183e+00,
        
        // Batch 0, Channel 2, 2x2 patch
        -2.2214e+00, -2.2214e+00,
        -2.2214e+00, -2.2214e+00
    };
    
    // Create hardcoded model state (simplified version of the state dict you provided)
    // This represents a few key layers from ResNet
    std::vector<float> model_state = {
        // conv1.weight (first conv layer) - 3x3x3 kernel
        -0.0156, -0.1879, -0.0307,
        -0.0445,  0.1709, -0.1334,
         0.1111, -0.1489, -0.1845,
        
        // bn1.weight (batch norm weights)
        2.3888e-01, 2.9136e-01, 3.1615e-01,
        
        // bn1.bias (batch norm bias)
        2.2484e-01, 6.0617e-01, 1.2483e-02,
        
        // fc.weight (final layer weights) - 10 classes
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
        
        // fc.bias (final layer bias)
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0
    };
    
    for (const auto& server_name : server_names) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "Testing server: " << server_name << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        py::dict server_result;
        server_result["server_name"] = server_name;
        
        try {
            // Get server info to check if it's local
            py::dict server_info = config.get_server_info(server_name);
            bool is_local = server_info["is_local"].cast<bool>();
            bool is_connected = server_info["connected"].cast<bool>();
            
            std::cout << "Server type: " << (is_local ? "Local" : "Remote") << std::endl;
            std::cout << "Connection status: " << (is_connected ? "Connected" : "Not connected") << std::endl;
            
            server_result["is_local"] = is_local;
            server_result["is_connected"] = is_connected;
            
            if (!is_connected) {
                std::cout << "Skipping server " << server_name << " - not connected" << std::endl;
                server_result["success"] = false;
                server_result["error"] = "Server not connected";
                server_results.append(server_result);
                continue;
            }
            
            std::pair<std::vector<float>, float> result;
            
            if (is_local) {
                // For local servers, directly use the GetGradients function from server_communication.cpp
                std::cout << "  Computing gradients locally (using GetGradients from server_communication)..." << std::endl;
                
                // Create an instance of ServerCommunicationServiceImpl to use its GetGradients method
                ServerCommunicationServiceImpl service;
                
                // Create the request and response objects
                leaftest::GradientRequest request;
                leaftest::GradientResponse response;
                
                // Set the request data
                request.set_model_state(model_state.data(), model_state.size() * sizeof(float));
                request.set_input_data(input_data.data(), input_data.size() * sizeof(float));
                
                // Call the GetGradients method directly
                grpc::ServerContext context;
                auto status = service.GetGradients(&context, &request, &response);
                
                if (!status.ok()) {
                    throw std::runtime_error("Local GetGradients failed: " + status.error_message());
                }
                
                if (!response.success()) {
                    throw std::runtime_error("Local GetGradients failed: " + response.error_message());
                }
                
                // Parse gradients from response
                std::vector<float> gradients;
                const std::string& gradients_data = response.gradients();
                size_t num_gradients = gradients_data.size() / sizeof(float);
                gradients.resize(num_gradients);
                std::memcpy(gradients.data(), gradients_data.data(), gradients_data.size());
                
                result = {gradients, response.loss()};
                std::cout << "  Local computation completed" << std::endl;
            } else {
                std::lock_guard<std::mutex> lock(channel_mutex);
                
                // Get or create channel for this server
                if (server_channels.find(server_name) == server_channels.end()) {
                    server_channels[server_name] = create_channel(server_name);
                }
                
                auto channel = server_channels[server_name];
                auto stub = leaftest::ServerCommunication::NewStub(channel);
                
                // Prepare request
                leaftest::GradientRequest request;
                request.set_model_state(model_state.data(), model_state.size() * sizeof(float));
                request.set_input_data(input_data.data(), input_data.size() * sizeof(float));
                
                // Make RPC call
                grpc::ClientContext context;
                leaftest::GradientResponse response;
                
                auto status = stub->GetGradients(&context, request, &response);
                
                if (!status.ok()) {
                    throw std::runtime_error("RPC failed for server " + server_name + ": " + status.error_message());
                }
                
                if (!response.success()) {
                    throw std::runtime_error("Server " + server_name + " failed: " + response.error_message());
                }
                
                // Parse gradients from response
                std::vector<float> gradients;
                const std::string& gradients_data = response.gradients();
                size_t num_gradients = gradients_data.size() / sizeof(float);
                gradients.resize(num_gradients);
                std::memcpy(gradients.data(), gradients_data.data(), gradients_data.size());
                
                result = {gradients, response.loss()};
            }

            // Print results
            std::cout << "✓ Gradient computation successful!" << std::endl;
            std::cout << "  Loss: " << result.second << std::endl;
            std::cout << "  Gradients size: " << result.first.size() << " elements" << std::endl;
            
            // Print first few gradients as a sample
            std::cout << "  Sample gradients: ";
            for (size_t i = 0; i < std::min(size_t(5), result.first.size()); ++i) {
                std::cout << result.first[i];
                if (i < std::min(size_t(4), result.first.size() - 1)) {
                    std::cout << ", ";
                }
            }
            if (result.first.size() > 5) {
                std::cout << ", ...";
            }
            std::cout << std::endl;
            
            // Store results
            server_result["success"] = true;
            server_result["loss"] = result.second;
            server_result["gradients_size"] = result.first.size();
            
            // Convert gradients to Python list
            py::list gradients_list;
            for (const auto& grad : result.first) {
                gradients_list.append(grad);
            }
            server_result["gradients"] = gradients_list;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Error testing server " << server_name << ": " << e.what() << std::endl;
            server_result["success"] = false;
            server_result["error"] = e.what();
        }
        
        server_results.append(server_result);
    }
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Gradient testing with hardcoded values completed!" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Return results
    results["server_results"] = server_results;
    results["total_servers"] = server_names.size();
    return results;
} 