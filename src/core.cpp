#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
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
        UserCredentials creds(username, hostname, port, key_path);
        servers[server_name] = Server(server_name, creds);
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
        const std::vector<float>& model_state,
        const std::vector<float>& input_data,
        const std::vector<float>& target_data,
        const std::string& model_type,
        const std::string& criterion_type) {
        
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

    void train(py::object model, 
        py::object optimizer, 
        py::object train_loader, 
        int epochs,
        py::object criterion = py::none()) {        
        
        std::cout << "=== Testing get_gradients_from_server function ===" << std::endl;
        
        // Get available servers
        auto server_names = config.get_servers();
        if (server_names.empty()) {
            std::cout << "No servers available for testing" << std::endl;
            return;
        }
        
        // Use the first available server for testing
        std::string test_server = server_names[0];
        std::cout << "Testing with server: " << test_server << std::endl;
        
        try {
            // Create dummy test data
            std::vector<float> dummy_model_state = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
            std::vector<float> dummy_input_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
            std::vector<float> dummy_target_data = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
            std::string model_type = "resnet50";
            std::string criterion_type = "CrossEntropyLoss";
            
            std::cout << "Sending test request to server..." << std::endl;
            std::cout << "Model state size: " << dummy_model_state.size() << " floats" << std::endl;
            std::cout << "Input data size: " << dummy_input_data.size() << " floats" << std::endl;
            std::cout << "Target data size: " << dummy_target_data.size() << " floats" << std::endl;
            std::cout << "Model type: " << model_type << std::endl;
            std::cout << "Criterion type: " << criterion_type << std::endl;
            
            // Call get_gradients_from_server
            auto result = get_gradients_from_server(
                test_server,
                dummy_model_state,
                dummy_input_data,
                dummy_target_data,
                model_type,
                criterion_type
            );
            
            std::vector<float>& gradients = result.first;
            float loss = result.second;
            
            std::cout << "=== Test Results ===" << std::endl;
            std::cout << "Success! Received response from server" << std::endl;
            std::cout << "Loss: " << loss << std::endl;
            std::cout << "Gradients size: " << gradients.size() << " floats" << std::endl;
            
            // Print first few gradients
            std::cout << "First 5 gradients: ";
            for (size_t i = 0; i < std::min(size_t(5), gradients.size()); ++i) {
                std::cout << gradients[i] << " ";
            }
            std::cout << std::endl;
            
            std::cout << "=== Test completed successfully ===" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "=== Test failed ===" << std::endl;
            std::cout << "Error: " << e.what() << std::endl;
            std::cout << "This might be expected if the gRPC server is not running" << std::endl;
        }
        
        std::cout << "=== End of get_gradients_from_server test ===" << std::endl;
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