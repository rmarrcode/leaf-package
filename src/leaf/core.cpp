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

namespace py = pybind11;

class UserCredentials {
private:
    std::string username;
    std::string hostname;
    int port;
    std::string key_path;
    bool is_connected;

public:
    UserCredentials(const std::string& user, const std::string& host, 
                   int p = 22, const std::string& key = "")
        : username(user), hostname(host), port(p), key_path(key), is_connected(false) {}

    bool verify_connection() {
        std::string cmd = "ssh -o BatchMode=yes -o ConnectTimeout=5 ";
        if (!key_path.empty()) {
            cmd += "-i " + key_path + " ";
        }
        cmd += "-p " + std::to_string(port) + " ";
        cmd += username + "@" + hostname + " 'echo 2>&1'";
        
        int result = std::system(cmd.c_str());
        is_connected = (result == 0);
        return is_connected;
    }

    std::string get_connection_string() const {
        return username + "@" + hostname + ":" + std::to_string(port);
    }

    bool get_connection_status() const { return is_connected; }
    std::string get_username() const { return username; }
    std::string get_hostname() const { return hostname; }
    int get_port() const { return port; }
    std::string get_key_path() const { return key_path; }
};

class ComputeResource {
private:
    std::string name;
    std::string type;
    std::map<std::string, std::string> properties;

public:
    ComputeResource(const std::string& n, const std::string& t)
        : name(n), type(t) {}

    void add_property(const std::string& key, const std::string& value) {
        properties[key] = value;
    }

    std::string get_name() const { return name; }
    std::string get_type() const { return type; }
    std::map<std::string, std::string> get_properties() const { return properties; }
};

class Server {
private:
    std::string name;
    UserCredentials credentials;
    std::vector<ComputeResource> resources;
    bool is_connected;
    bool is_local;

    void discover_resources() {
        if (!is_connected && !is_local) return;

        // Discover CPU information
        std::string cpu_cmd;
        if (is_local) {
            cpu_cmd = "sysctl -n machdep.cpu.brand_string 2>/dev/null";
        } else {
            cpu_cmd = "ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=10 ";
            if (!credentials.get_key_path().empty()) {
                cpu_cmd += "-i " + credentials.get_key_path() + " ";
            }
            cpu_cmd += "-p " + std::to_string(credentials.get_port()) + " ";
            cpu_cmd += credentials.get_username() + "@" + credentials.get_hostname() + " 'sysctl -n machdep.cpu.brand_string 2>/dev/null'";
        }

        std::array<char, 128> buffer;
        std::string cpu_result;
        std::unique_ptr<FILE, decltype(&pclose)> cpu_pipe(popen(cpu_cmd.c_str(), "r"), pclose);
        
        if (cpu_pipe) {
            while (fgets(buffer.data(), buffer.size(), cpu_pipe.get()) != nullptr) {
                cpu_result += buffer.data();
            }
            // Remove trailing newline
            if (!cpu_result.empty() && cpu_result[cpu_result.length()-1] == '\n') {
                cpu_result.erase(cpu_result.length()-1);
            }
            if (!cpu_result.empty()) {
                ComputeResource cpu(cpu_result, "CPU");
                resources.push_back(cpu);
            }
        }

        // Discover MPS information (Metal Performance Shaders)
        std::string mps_cmd;
        if (is_local) {
            mps_cmd = "system_profiler SPDisplaysDataType 2>/dev/null | grep 'Metal:' 2>/dev/null";
        } else {
            mps_cmd = "ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=10 ";
            if (!credentials.get_key_path().empty()) {
                mps_cmd += "-i " + credentials.get_key_path() + " ";
            }
            mps_cmd += "-p " + std::to_string(credentials.get_port()) + " ";
            mps_cmd += credentials.get_username() + "@" + credentials.get_hostname() + " 'system_profiler SPDisplaysDataType 2>/dev/null | grep \"Metal:\" 2>/dev/null'";
        }

        std::string mps_result;
        std::unique_ptr<FILE, decltype(&pclose)> mps_pipe(popen(mps_cmd.c_str(), "r"), pclose);
        
        if (mps_pipe) {
            while (fgets(buffer.data(), buffer.size(), mps_pipe.get()) != nullptr) {
                mps_result += buffer.data();
            }
            if (!mps_result.empty()) {
                ComputeResource mps("Metal Performance Shaders", "MPS");
                mps.add_property("status", "Available");
                resources.push_back(mps);
            }
        }

        // Discover GPU information
        std::string gpu_cmd;
        if (is_local) {
            gpu_cmd = "nvidia-smi --query-gpu=name,memory.total,memory.free --format=csv,noheader 2>/dev/null";
        } else {
            // For remote servers, we need to ensure the command is properly escaped and executed
            gpu_cmd = "ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=10 ";
            if (!credentials.get_key_path().empty()) {
                gpu_cmd += "-i " + credentials.get_key_path() + " ";
            }
            gpu_cmd += "-p " + std::to_string(credentials.get_port()) + " ";
            gpu_cmd += credentials.get_username() + "@" + credentials.get_hostname() + " 'nvidia-smi --query-gpu=name,memory.total,memory.free --format=csv,noheader 2>/dev/null'";
        }

        std::string gpu_result;
        std::unique_ptr<FILE, decltype(&pclose)> gpu_pipe(popen(gpu_cmd.c_str(), "r"), pclose);
        
        if (gpu_pipe) {
            while (fgets(buffer.data(), buffer.size(), gpu_pipe.get()) != nullptr) {
                gpu_result += buffer.data();
            }

            // Parse GPU information
            if (!gpu_result.empty()) {
                std::regex pattern("([^,]+),\\s*([^,]+),\\s*([^,]+)");
                std::smatch matches;
                std::string::const_iterator searchStart(gpu_result.cbegin());
                
                while (std::regex_search(searchStart, gpu_result.cend(), matches, pattern)) {
                    ComputeResource gpu(matches[1], "GPU");
                    gpu.add_property("total_memory", matches[2]);
                    gpu.add_property("free_memory", matches[3]);
                    resources.push_back(gpu);
                    searchStart = matches.suffix().first;
                }
            }
        }

        // If no resources were found, try a simpler GPU detection
        if (resources.empty()) {
            std::string simple_gpu_cmd;
            if (is_local) {
                simple_gpu_cmd = "nvidia-smi 2>/dev/null | grep 'NVIDIA'";
            } else {
                simple_gpu_cmd = "ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=10 ";
                if (!credentials.get_key_path().empty()) {
                    simple_gpu_cmd += "-i " + credentials.get_key_path() + " ";
                }
                simple_gpu_cmd += "-p " + std::to_string(credentials.get_port()) + " ";
                simple_gpu_cmd += credentials.get_username() + "@" + credentials.get_hostname() + " 'nvidia-smi 2>/dev/null | grep \"NVIDIA\"'";
            }

            std::string simple_gpu_result;
            std::unique_ptr<FILE, decltype(&pclose)> simple_gpu_pipe(popen(simple_gpu_cmd.c_str(), "r"), pclose);
            
            if (simple_gpu_pipe) {
                while (fgets(buffer.data(), buffer.size(), simple_gpu_pipe.get()) != nullptr) {
                    simple_gpu_result += buffer.data();
                }
                if (!simple_gpu_result.empty()) {
                    ComputeResource gpu("NVIDIA GPU", "GPU");
                    gpu.add_property("status", "Available");
                    resources.push_back(gpu);
                }
            }
        }
    }

public:
    // Default constructor required for std::map
    Server() : name(""), credentials("", "", 0, ""), is_connected(false), is_local(false) {}

    // Main constructor
    Server(const std::string& server_name, const UserCredentials& creds, bool local = false)
        : name(server_name), credentials(creds), is_connected(false), is_local(local) {
        if (local) {
            is_connected = true;
        } else {
            is_connected = credentials.verify_connection();
        }
        if (is_connected) {
            discover_resources();
        }
    }

    bool is_server_connected() const { return is_connected; }
    bool is_local_server() const { return is_local; }
    std::string get_name() const { return name; }
    UserCredentials get_credentials() const { return credentials; }
    std::vector<ComputeResource> get_resources() const { return resources; }
};

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
};

class LeafTrainer {
private:
    LeafConfig config;
    int world_size;
    int world_rank;
    bool is_initialized;
    std::vector<std::string> available_hosts;

    void initialize_mpi() {
        if (!is_initialized) {
            // Get all available servers from config
            auto servers = config.get_servers();
            available_hosts.clear();
            
            // Create hostfile content
            std::string hostfile_path = "/tmp/leaf_mpi_hostfile";
            std::ofstream hostfile(hostfile_path);
            
            for (const auto& server_name : servers) {
                auto server_info = config.get_server_info(server_name);
                if (server_info["connected"].cast<bool>()) {
                    std::string hostname = server_info["hostname"].cast<std::string>();
                    available_hosts.push_back(hostname);
                    
                    // Count all compute resources for this server
                    int gpu_count = 0;
                    int mps_count = 0;
                    int cpu_count = 0;
                    
                    auto resources = server_info["resources"].cast<py::list>();
                    for (const auto& resource : resources) {
                        auto res_dict = resource.cast<py::dict>();
                        std::string type = res_dict["type"].cast<std::string>();
                        
                        if (type == "GPU") {
                            gpu_count++;
                        } else if (type == "MPS") {
                            mps_count++;
                        } else if (type == "CPU") {
                            // For CPU, we'll use the number of CPU cores if available
                            auto props = res_dict["properties"].cast<py::dict>();
                            if (props.contains("cores")) {
                                cpu_count = props["cores"].cast<int>();
                            } else {
                                // Default to 1 if cores info not available
                                cpu_count = 1;
                            }
                        }
                    }
                    
                    // Calculate total slots as sum of all compute resources
                    int total_slots = gpu_count + mps_count + cpu_count;
                    
                    // Add to hostfile with total slots
                    hostfile << hostname << " slots=" << total_slots << "\n";
                    
                    if (world_rank == 0) {
                        std::cout << "Server " << hostname << " resources: "
                                 << gpu_count << " GPUs, "
                                 << mps_count << " MPS, "
                                 << cpu_count << " CPU cores"
                                 << " (total slots: " << total_slots << ")" << std::endl;
                    }
                }
            }
            hostfile.close();

            // Set MPI environment variables
            setenv("OMPI_MCA_btl_tcp_if_include", "eth0", 1);  // Use eth0 interface
            setenv("OMPI_MCA_btl", "^openib", 1);  // Disable InfiniBand
            
            // Initialize MPI with hostfile
            char* argv[] = {const_cast<char*>("leaf_trainer"), nullptr};
            int argc = 1;
            MPI_Init(&argc, &argv);
            
            // Get world size and rank
            MPI_Comm_size(MPI_COMM_WORLD, &world_size);
            MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
            
            if (world_rank == 0) {
                std::cout << "Initialized MPI with " << world_size << " processes across " 
                         << available_hosts.size() << " hosts" << std::endl;
            }
            
            is_initialized = true;
        }
    }

    void finalize_mpi() {
        if (is_initialized) {
            MPI_Finalize();
            is_initialized = false;
            
            // Clean up hostfile
            std::remove("/tmp/leaf_mpi_hostfile");
        }
    }

public:
    LeafTrainer(const LeafConfig& cfg) 
        : config(cfg), world_size(0), world_rank(0), is_initialized(false) {
        initialize_mpi();
    }

    ~LeafTrainer() {
        finalize_mpi();
    }

    void train(py::object model, py::object optimizer, py::object train_loader, 
              int epochs, py::object criterion = py::none()) {
        if (!is_initialized) {
            throw std::runtime_error("MPI not initialized");
        }

        // Get total number of batches
        int total_batches = train_loader.attr("__len__")().cast<int>();

        // Get server info for this rank to determine device
        auto servers = config.get_servers();
        std::string device = "cpu";  // default device
        
        // Find which server this rank belongs to
        int current_rank = 0;
        for (const auto& server_name : servers) {
            auto server_info = config.get_server_info(server_name);
            if (server_info["connected"].cast<bool>()) {
                auto resources = server_info["resources"].cast<py::list>();
                int gpu_count = 0;
                int mps_count = 0;
                
                // Count available resources
                for (const auto& resource : resources) {
                    auto res_dict = resource.cast<py::dict>();
                    std::string type = res_dict["type"].cast<std::string>();
                    if (type == "GPU") gpu_count++;
                    else if (type == "MPS") mps_count++;
                }
                
                // Calculate how many ranks this server handles
                int server_ranks = gpu_count + mps_count + 1;  // +1 for CPU
                
                // Check if this rank belongs to this server
                if (world_rank >= current_rank && world_rank < current_rank + server_ranks) {
                    int local_rank = world_rank - current_rank;
                    
                    // Assign device based on local rank
                    if (local_rank < gpu_count) {
                        device = "cuda:" + std::to_string(local_rank);
                    } else if (local_rank < gpu_count + mps_count) {
                        device = "mps";
                    } else {
                        device = "cpu";
                    }
                    break;
                }
                
                current_rank += server_ranks;
            }
        }

        // Broadcast initial model parameters to all processes
        auto parameters = model.attr("parameters")();
        std::vector<std::vector<float>> param_buffers;
        
        if (world_rank == 0) {
            // Master process collects parameters
            for (auto param : parameters) {
                auto tensor = param.attr("data").cast<py::object>();
                auto numpy_array = tensor.attr("cpu")().attr("numpy")();
                auto array = numpy_array.cast<py::array_t<float>>();
                std::vector<float> param_vec(array.size());
                std::memcpy(param_vec.data(), array.data(), array.size() * sizeof(float));
                param_buffers.push_back(param_vec);
            }
        }

        // Broadcast parameter sizes
        std::vector<int> param_sizes;
        if (world_rank == 0) {
            for (const auto& buffer : param_buffers) {
                param_sizes.push_back(buffer.size());
            }
        }
        int num_params = param_sizes.size();
        MPI_Bcast(&num_params, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        if (world_rank != 0) {
            param_sizes.resize(num_params);
        }
        MPI_Bcast(param_sizes.data(), num_params, MPI_INT, 0, MPI_COMM_WORLD);

        // Broadcast parameters
        if (world_rank != 0) {
            param_buffers.resize(num_params);
            for (int i = 0; i < num_params; ++i) {
                param_buffers[i].resize(param_sizes[i]);
            }
        }
        
        for (int i = 0; i < num_params; ++i) {
            MPI_Bcast(param_buffers[i].data(), param_sizes[i], MPI_FLOAT, 0, MPI_COMM_WORLD);
        }

        // Move model to appropriate device and update parameters
        model.attr("to")(device);
        int param_idx = 0;
        for (auto param : parameters) {
            auto tensor = param.attr("data").cast<py::object>();
            auto numpy_array = tensor.attr("cpu")().attr("numpy")();
            auto array = numpy_array.cast<py::array_t<float>>();
            std::memcpy(array.mutable_data(), param_buffers[param_idx].data(), 
                       param_sizes[param_idx] * sizeof(float));
            param_idx++;
        }
        
        // Training loop
        for (int epoch = 0; epoch < epochs; ++epoch) {
            float epoch_loss = 0.0f;
            int processed_batches = 0;

            // Reset the data loader for this epoch
            train_loader.attr("reset")();

            // Process batches in parallel across all processes
            for (int batch_idx = world_rank; batch_idx < total_batches; batch_idx += world_size) {
                // Get the batch for this process
                auto batch = train_loader.attr("__getitem__")(batch_idx);
                auto inputs = batch[0].attr("to")(device);
                auto targets = batch[1].attr("to")(device);

                // Forward pass
                auto outputs = model.attr("__call__")(inputs);
                auto loss = criterion.is_none() ? 
                    model.attr("loss")(outputs, targets) : 
                    criterion(outputs, targets);

                // Backward pass
                optimizer.attr("zero_grad")();
                loss.attr("backward")();

                // Collect gradients from this process
                std::vector<std::vector<float>> all_gradients;
                for (auto param : parameters) {
                    if (param.attr("grad").is_none()) continue;
                    auto grad = param.attr("grad").attr("cpu")().attr("numpy")();
                    auto array = grad.cast<py::array_t<float>>();
                    std::vector<float> grad_vec(array.size());
                    std::memcpy(grad_vec.data(), array.data(), array.size() * sizeof(float));
                    all_gradients.push_back(grad_vec);
                }

                // Prepare buffer for all-reduce
                std::vector<float> flat_gradients;
                for (const auto& grad : all_gradients) {
                    flat_gradients.insert(flat_gradients.end(), grad.begin(), grad.end());
                }

                // All-reduce gradients across all processes
                std::vector<float> reduced_gradients(flat_gradients.size());
                MPI_Allreduce(flat_gradients.data(), reduced_gradients.data(), 
                            flat_gradients.size(), MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD);

                // Average the gradients
                for (float& grad : reduced_gradients) {
                    grad /= world_size;
                }

                // Update model parameters with averaged gradients
                int grad_idx = 0;
                for (auto param : parameters) {
                    if (param.attr("grad").is_none()) continue;
                    auto grad = param.attr("grad");
                    auto numpy_array = grad.attr("cpu")().attr("numpy")();
                    auto array = numpy_array.cast<py::array_t<float>>();
                    auto size = array.size();
                    std::memcpy(array.mutable_data(), 
                              reduced_gradients.data() + grad_idx,
                              size * sizeof(float));
                    grad_idx += size;
                }

                // Update parameters
                optimizer.attr("step")();

                // Broadcast updated parameters to all processes
                param_buffers.clear();
                for (auto param : parameters) {
                    auto tensor = param.attr("data").cast<py::object>();
                    auto numpy_array = tensor.attr("cpu")().attr("numpy")();
                    auto array = numpy_array.cast<py::array_t<float>>();
                    std::vector<float> param_vec(array.size());
                    std::memcpy(param_vec.data(), array.data(), array.size() * sizeof(float));
                    param_buffers.push_back(param_vec);
                }

                // Broadcast new parameters
                for (int i = 0; i < num_params; ++i) {
                    MPI_Bcast(param_buffers[i].data(), param_sizes[i], MPI_FLOAT, 0, MPI_COMM_WORLD);
                }

                // Update local model parameters
                param_idx = 0;
                for (auto param : parameters) {
                    auto tensor = param.attr("data").cast<py::object>();
                    auto numpy_array = tensor.attr("cpu")().attr("numpy")();
                    auto array = numpy_array.cast<py::array_t<float>>();
                    std::memcpy(array.mutable_data(), param_buffers[param_idx].data(), 
                              param_sizes[param_idx] * sizeof(float));
                    param_idx++;
                }

                epoch_loss += loss.attr("item")().cast<float>();
                processed_batches++;

                // Synchronize all processes after each batch
                MPI_Barrier(MPI_COMM_WORLD);
            }

            // Average loss across all processes
            float total_loss;
            MPI_Allreduce(&epoch_loss, &total_loss, 1, MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD);
            total_loss /= (world_size * processed_batches);

            if (world_rank == 0) {
                std::cout << "Epoch " << epoch + 1 << "/" << epochs 
                         << " Loss: " << total_loss 
                         << " (processed " << processed_batches << " batches per process)" 
                         << std::endl;
            }
        }
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
        .def("print_all_resources", &LeafConfig::print_all_resources);

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