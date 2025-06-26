#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <array>
#include <memory>
#include <stdexcept>
#include <regex>
#include <iomanip>
#include <mpi.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <set>
#include <mutex>
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include "server_communication.pb.h"
#include "server_communication.grpc.pb.h"
#include "server_communication.h"
#include "user_credentials.h"
#include "server.h"

namespace py = pybind11;

class LeafConfig {
private:
    std::map<std::string, Server> servers;

    void discover_local_resources() {
        // Create a dummy UserCredentials for localhost
        UserCredentials local_creds("", "", 0, "");
        servers["localhost"] = Server("localhost", local_creds, true);
    }

    void print_server_info(const Server& server) const {
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

public:
    LeafConfig() {
        discover_local_resources();
    }

    void add_server(const std::string& server_name,
                   const std::string& username,
                   const std::string& hostname,
                   int port = 22,
                   const std::string& key_path = "") {
        std::cout << "Adding server: " << server_name << " with hostname: " << hostname << " and port: " << port << std::endl;
        UserCredentials creds(username, hostname, port, key_path);
        servers[server_name] = Server(server_name, creds);
        std::cout << "Server " << server_name << " added successfully" << std::endl;
    }

    std::vector<std::string> get_servers() const {
        std::vector<std::string> server_names;
        for (const auto& pair : servers) {
            server_names.push_back(pair.first);
        }
        return server_names;
    }

    py::dict get_server_info(const std::string& server_name) const {
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

    void remove_server(const std::string& server_name) {
        if (server_name != "localhost") {  // Prevent removal of localhost
            servers.erase(server_name);
        }
    }

    void print_all_resources() const {
        std::cout << "\n=== Available Servers and Resources ===\n";
        
        for (const auto& [name, server] : servers) {
            print_server_info(server);
        }
        
        std::cout << "\n=== End of Server List ===\n\n";
    }

    std::pair<int, std::string> get_server_connection_info(const std::string& server_name) const {
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
};

class LeafTrainer {
private:
    LeafConfig config;
    std::map<std::string, std::shared_ptr<grpc::Channel>> server_channels;
    std::mutex channel_mutex;

    // Helper function to create gRPC channel for a server
    std::shared_ptr<grpc::Channel> create_channel(const std::string& server_name) {
        const auto& servers = config.get_servers();
        auto it = std::find(servers.begin(), servers.end(), server_name);
        if (it == servers.end()) {
            throw std::runtime_error("Server " + server_name + " not found in configuration");
        }

        // Get the server address using the tunnel port from user credentials
        std::string server_address = config.get_server_connection_info(server_name).second;
        std::cout << "Creating gRPC channel for server '" << server_name << "' at address: " << server_address << std::endl;
        
        auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        return channel;
    }

    // Helper function to get gradients from a server
    std::pair<std::vector<float>, float> get_gradients_from_server(
        const std::string& server_name,
        py::object inputs,
        py::object model,
        py::object criterion,
        py::object optimizer,
        bool is_local = false) {
        
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
                        for (size_t i = 0; i < buffer.size(); ++i) {
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
        for (size_t i = 0; i < inputs_buffer.size(); ++i) {
            input_data.push_back(inputs_buffer[i]);
        }
        
        // Create dummy target data (this should be passed from the caller)
        // For now, using a simple one-hot encoding
        size_t num_classes = 10; // Default for CIFAR-10
        std::vector<float> target_data(num_classes, 0.0f);
        target_data[0] = 1.0f; // Dummy target
        
        // Get model type and criterion type
        std::string model_type = "resnet50"; // Default
        std::string criterion_type = "CrossEntropyLoss"; // Default
        
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
            request.set_target_data(target_data.data(), target_data.size() * sizeof(float));
            request.set_model_type(model_type);
            request.set_criterion_type(criterion_type);
            
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
        request.set_target_data(target_data.data(), target_data.size() * sizeof(float));
        request.set_model_type(model_type);
        request.set_criterion_type(criterion_type);
        
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

    // Helper function to divide batch among servers
    std::vector<std::pair<std::string, std::vector<size_t>>> distribute_batch(
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

public:
    LeafTrainer(const LeafConfig& cfg) : config(cfg) {
    }

    ~LeafTrainer() {
    }

    py::dict train(py::object model, 
        py::object optimizer, 
        py::object train_loader, 
        int epochs,
        py::object criterion = py::none()) {        
        
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

};

PYBIND11_MODULE(_core, m) {
    std::cout << "Initializing _core module..." << std::endl;
    
    py::class_<LeafConfig>(m, "LeafConfig")
        .def(py::init<>())
        .def("add_server", &LeafConfig::add_server,
             py::arg("server_name"),
             py::arg("username"),
             py::arg("hostname"),
             py::arg("port") = 22,
             py::arg("key_path") = "")
        .def("get_servers", &LeafConfig::get_servers)
        .def("get_server_info", &LeafConfig::get_server_info)
        .def("remove_server", &LeafConfig::remove_server)
        .def("print_all_resources", &LeafConfig::print_all_resources)
        .def("get_server_connection_info", &LeafConfig::get_server_connection_info);

    py::class_<LeafTrainer>(m, "LeafTrainer")
        .def(py::init<const LeafConfig&>())
        .def("train", &LeafTrainer::train,
             py::arg("model"),
             py::arg("optimizer"),
             py::arg("train_loader"),
             py::arg("epochs"),
             py::arg("criterion") = py::none());
        
    std::cout << "_core module initialization complete!" << std::endl;
} 