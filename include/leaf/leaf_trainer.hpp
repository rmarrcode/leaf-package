#pragma once

#include <torch/torch.h>
#include <vector>
#include <string>
#include <memory>
#include <mpi.h>
#include <map>

namespace leaf {

class LeafConfig {
public:
    // Training configuration
    bool use_cuda = false;
    bool use_mpi = false;
    int world_size = 1;
    int rank = 0;
    int batch_size_multiplier = 1;
    std::vector<int> gpu_ids;

    // Server configuration
    struct ServerInfo {
        std::string username;
        std::string hostname;
        int port = 22;
        std::string key_path;
        bool is_local = false;
        std::vector<std::string> gpu_ids;
    };
    std::map<std::string, ServerInfo> servers;
    
    LeafConfig() {
        // Initialize local server by default
        servers["localhost"] = ServerInfo{"", "", 0, "", true};
    }

    // Server management methods
    void add_server(const std::string& server_name,
                   const std::string& username,
                   const std::string& hostname,
                   int port = 22,
                   const std::string& key_path = "") {
        servers[server_name] = ServerInfo{username, hostname, port, key_path, false};
    }

    void remove_server(const std::string& server_name) {
        if (server_name != "localhost") {  // Prevent removal of localhost
            servers.erase(server_name);
        }
    }

    std::vector<std::string> get_servers() const {
        std::vector<std::string> server_names;
        for (const auto& pair : servers) {
            server_names.push_back(pair.first);
        }
        return server_names;
    }
};

class LeafTrainer {
private:
    LeafConfig config;
    std::vector<torch::Device> devices;
    bool mpi_initialized;
    
public:
    LeafTrainer(const LeafConfig& cfg);
    ~LeafTrainer();
    
    // Split input data across devices
    std::vector<torch::Tensor> split_input(const torch::Tensor& input);
    
    // Average gradients across all processes
    void average_gradients(torch::nn::Module& model);
    
    // Forward pass with distributed computation
    torch::Tensor forward(torch::nn::Module& model, const torch::Tensor& input);
    
    // Backward pass with gradient synchronization
    void backward(torch::nn::Module& model, const torch::Tensor& loss);
    
    // Generate MPI hostfile
    void generate_hostfile(const std::string& filename);
    
    // Run the training function
    void run(std::function<void()> training_func);
};

} // namespace leaf 