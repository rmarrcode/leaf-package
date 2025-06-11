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
#include <grpc/grpc.h>
#include "leaftest.grpc.pb.h"

namespace py = pybind11;

class UserCredentials {
private:
    std::string username;
    std::string hostname;
    int port;
    std::string key_path;
    bool is_connected;

    bool verify_grpc_connection() {
        // First, copy the server test binary to the remote server
        std::string scp_cmd = "scp -o BatchMode=yes -o ConnectTimeout=5 ";
        if (!key_path.empty()) {
            scp_cmd += "-i " + key_path + " ";
        }
        scp_cmd += "-P " + std::to_string(port) + " ";
        scp_cmd += "server_test " + username + "@" + hostname + ":/tmp/";
        
        if (std::system(scp_cmd.c_str()) != 0) {
            std::cerr << "Failed to copy server test binary to " << hostname << std::endl;
            return false;
        }

        // Start the server test program in the background
        std::string ssh_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=5 ";
        if (!key_path.empty()) {
            ssh_cmd += "-i " + key_path + " ";
        }
        ssh_cmd += "-p " + std::to_string(port) + " ";
        ssh_cmd += username + "@" + hostname + " 'chmod +x /tmp/server_test && /tmp/server_test > /dev/null 2>&1 &'";
        
        if (std::system(ssh_cmd.c_str()) != 0) {
            std::cerr << "Failed to start server test on " << hostname << std::endl;
            return false;
        }

        // Wait a moment for the server to start
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Create gRPC channel and stub
        std::string target = hostname + ":50051";
        auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
        auto stub = leaftest::ServerTest::NewStub(channel);

        // Try to get server time
        grpc::ClientContext context;
        leaftest::TimeRequest request;
        leaftest::TimeResponse response;
        
        auto status = stub->GetServerTime(&context, request, &response);
        
        if (status.ok()) {
            std::cout << "Successfully connected to " << hostname 
                      << " (Server time: " << response.server_time_ms() << " ms)" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to get server time from " << hostname 
                      << ": " << status.error_message() << std::endl;
            return false;
        }
    }

public:
    UserCredentials(const std::string& user, const std::string& host, 
                   int p = 22, const std::string& key = "")
        : username(user), hostname(host), port(p), key_path(key), is_connected(false) {}

    bool verify_connection() {
        // First verify basic SSH connection
        std::string cmd = "ssh -o BatchMode=yes -o ConnectTimeout=5 ";
        if (!key_path.empty()) {
            cmd += "-i " + key_path + " ";
        }
        cmd += "-p " + std::to_string(port) + " ";
        cmd += username + "@" + hostname + " 'echo 2>&1'";
        
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "Basic SSH connection failed for " << hostname << std::endl;
            is_connected = false;
            return false;
        }

        // Then verify gRPC connection
        is_connected = verify_grpc_connection();
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
    std::vector<std::string> available_hosts;

    void initialize() {

    }

    void finalize() {
        
    }

public:
    LeafTrainer(const LeafConfig& cfg) : config(cfg) {
        initialize();
    }

    ~LeafTrainer() {
        finalize();
    }

    void train(py::object model, py::object optimizer, py::object train_loader, 
              int epochs, py::object criterion = py::none()) {

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