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
#include <grpcpp/grpcpp.h>
#include "server_test.pb.h"
#include "server_test.grpc.pb.h"

namespace py = pybind11;

class UserCredentials {
private:
    std::string username;
    std::string hostname;
    int port;
    std::string key_path;
    bool is_connected;
    int tunnel_pid;  // PID of SSH tunnel process

    bool setup_ssh_tunnel() {
        std::cout << "Setting up SSH tunnel for gRPC connection..." << std::endl;
        
        // Check if tunnel is already running
        std::string check_cmd = "lsof -Pi :50051 -sTCP:LISTEN -t 2>/dev/null";
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check_cmd.c_str(), "r"), pclose);
        
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }
        
        if (!result.empty()) {
            std::cout << "Port 50051 is already in use. Killing existing tunnel..." << std::endl;
            std::string kill_cmd = "pkill -f 'ssh.*50051:localhost'";
            std::system(kill_cmd.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        // Build SSH tunnel command
        std::string ssh_cmd = "ssh -L 50051:localhost:50051 ";
        if (!key_path.empty()) {
            ssh_cmd += "-i " + key_path + " ";
        }
        ssh_cmd += "-p " + std::to_string(port) + " ";
        ssh_cmd += username + "@" + hostname + " -N";
        
        std::cout << "Starting SSH tunnel with command: " << ssh_cmd << std::endl;
        
        // Start tunnel in background
        ssh_cmd += " &";
        int result_code = std::system(ssh_cmd.c_str());
        
        if (result_code != 0) {
            std::cerr << "Failed to start SSH tunnel" << std::endl;
            return false;
        }
        
        // Wait for tunnel to establish
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Verify tunnel is working
        std::string verify_cmd = "lsof -Pi :50051 -sTCP:LISTEN -t 2>/dev/null";
        result.clear();
        pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(verify_cmd.c_str(), "r"), pclose);
        
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
        }
        
        if (!result.empty()) {
            // Extract PID from result
            std::string pid_str = result;
            if (pid_str.find('\n') != std::string::npos) {
                pid_str = pid_str.substr(0, pid_str.find('\n'));
            }
            try {
                tunnel_pid = std::stoi(pid_str);
                std::cout << "SSH tunnel established successfully! PID: " << tunnel_pid << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse tunnel PID: " << e.what() << std::endl;
                return false;
            }
        } else {
            std::cerr << "SSH tunnel verification failed" << std::endl;
            return false;
        }
    }

    void cleanup_ssh_tunnel() {
        if (tunnel_pid > 0) {
            std::cout << "Cleaning up SSH tunnel (PID: " << tunnel_pid << ")..." << std::endl;
            std::string kill_cmd = "kill " + std::to_string(tunnel_pid);
            std::system(kill_cmd.c_str());
            tunnel_pid = 0;
        }
    }

    bool verify_ssh_connection() {
        std::cout << "Attempting SSH connection..." << std::endl;
        std::string cmd = "ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=no ";
        if (!key_path.empty()) {
            cmd += "-i " + key_path + " ";
        }
        cmd += "-p " + std::to_string(port) + " ";
        cmd += username + "@" + hostname + " 'echo 2>&1'";
        
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << "SSH connection failed for " << hostname << std::endl;
            return false;
        }
        std::cout << "SSH connection successful" << std::endl;
        return true;
    }

    bool verify_grpc_connection() {
        try {
            std::cout << "Starting gRPC connection verification..." << std::endl;
            std::cout << "Connecting to " << hostname << ":" << port << " as " << username << std::endl;
            
            // First verify SSH connection with verbose output
            std::string test_ssh_cmd = "ssh -v -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                test_ssh_cmd += "-i " + key_path + " ";
            }
            test_ssh_cmd += "-p " + std::to_string(port) + " ";
            test_ssh_cmd += username + "@" + hostname + " 'echo SSH connection test successful'";
            
            std::cout << "Testing SSH connection..." << std::endl;
            if (std::system(test_ssh_cmd.c_str()) != 0) {
                std::cerr << "SSH connection test failed. Please verify:" << std::endl;
                std::cerr << "1. SSH port " << port << " is correct" << std::endl;
                std::cerr << "2. Username " << username << " is correct" << std::endl;
                std::cerr << "3. SSH key is properly set up" << std::endl;
                return false;
            }
            std::cout << "SSH connection test successful" << std::endl;
            
            // Check if Docker is installed and install if needed
            std::string check_docker_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                check_docker_cmd += "-i " + key_path + " ";
            }
            check_docker_cmd += "-p " + std::to_string(port) + " ";
            check_docker_cmd += username + "@" + hostname + " 'which docker || echo \"DOCKER_NOT_FOUND\"'";
            
            std::cout << "Checking for Docker installation..." << std::endl;
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check_docker_cmd.c_str(), "r"), pclose);
            
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    result += buffer.data();
                }
            }
            
            if (result.find("DOCKER_NOT_FOUND") != std::string::npos) {
                std::cout << "Docker not found. Installing Docker..." << std::endl;
                
                // Install Docker with progress output
                std::string install_docker_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
                if (!key_path.empty()) {
                    install_docker_cmd += "-i " + key_path + " ";
                }
                install_docker_cmd += "-p " + std::to_string(port) + " ";
                install_docker_cmd += username + "@" + hostname + " '"
                    "echo \"Updating package lists...\" && "
                    "sudo apt-get update && "
                    "echo \"Installing prerequisites...\" && "
                    "sudo apt-get install -y ca-certificates curl gnupg && "
                    "echo \"Setting up Docker repository...\" && "
                    "sudo install -m 0755 -d /etc/apt/keyrings && "
                    "curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg && "
                    "sudo chmod a+r /etc/apt/keyrings/docker.gpg && "
                    "echo \"deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo $VERSION_CODENAME) stable\" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null && "
                    "echo \"Installing Docker...\" && "
                    "sudo apt-get update && "
                    "sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin && "
                    "echo \"Docker installation complete\"'";
                
                std::cout << "Running Docker installation command..." << std::endl;
                if (std::system(install_docker_cmd.c_str()) != 0) {
                    std::cerr << "Failed to install Docker on " << hostname << std::endl;
                    return false;
                }
                
                std::cout << "Docker installed successfully" << std::endl;
            } else {
                std::cout << "Docker is already installed" << std::endl;
            }

            // Check if Docker daemon is running and start it if needed
            std::cout << "Checking Docker daemon status..." << std::endl;
            std::string check_daemon_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                check_daemon_cmd += "-i " + key_path + " ";
            }
            check_daemon_cmd += "-p " + std::to_string(port) + " ";
            check_daemon_cmd += username + "@" + hostname + " 'sudo systemctl is-active docker || echo \"DOCKER_NOT_RUNNING\"'";
            
            result.clear();
            pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(check_daemon_cmd.c_str(), "r"), pclose);
            
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    result += buffer.data();
                }
            }
            
            if (result.find("DOCKER_NOT_RUNNING") != std::string::npos || result.find("inactive") != std::string::npos) {
                std::cout << "Docker daemon is not running. Starting Docker daemon..." << std::endl;
                
                // First ensure Docker socket directory exists and has correct permissions
                std::string setup_docker_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
                if (!key_path.empty()) {
                    setup_docker_cmd += "-i " + key_path + " ";
                }
                setup_docker_cmd += "-p " + std::to_string(port) + " ";
                setup_docker_cmd += username + "@" + hostname + " '"
                    "sudo mkdir -p /var/run && "
                    "sudo chmod 777 /var/run && "
                    "sudo mkdir -p /var/run/docker && "
                    "sudo chmod 777 /var/run/docker && "
                    "sudo service docker stop || true && "
                    "sudo rm -f /var/run/docker.sock && "
                    "sudo service docker start || "
                    "(sudo nohup dockerd > /var/log/docker.log 2>&1 &) && "
                    "sleep 5 && "  // Wait for daemon to fully start
                    "sudo docker info || echo \"DOCKER_START_FAILED\"'";
                
                std::cout << "Setting up Docker daemon..." << std::endl;
                if (std::system(setup_docker_cmd.c_str()) != 0) {
                    std::cerr << "Failed to set up Docker daemon on " << hostname << std::endl;
                    return false;
                }

                // Verify Docker daemon is running
                std::string verify_docker_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
                if (!key_path.empty()) {
                    verify_docker_cmd += "-i " + key_path + " ";
                }
                verify_docker_cmd += "-p " + std::to_string(port) + " ";
                verify_docker_cmd += username + "@" + hostname + " '"
                    "sudo docker info > /dev/null 2>&1 && echo \"DOCKER_RUNNING\" || echo \"DOCKER_NOT_RUNNING\"'";
                
                result.clear();
                pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(verify_docker_cmd.c_str(), "r"), pclose);
                
                if (pipe) {
                    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                        result += buffer.data();
                    }
                }
                
                if (result.find("DOCKER_RUNNING") == std::string::npos) {
                    std::cerr << "Docker daemon failed to start properly on " << hostname << std::endl;
                    std::cerr << "Attempting alternative startup method..." << std::endl;
                    
                    // Try alternative startup method
                    std::string alt_start_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
                    if (!key_path.empty()) {
                        alt_start_cmd += "-i " + key_path + " ";
                    }
                    alt_start_cmd += "-p " + std::to_string(port) + " ";
                    alt_start_cmd += username + "@" + hostname + " '"
                        "sudo pkill -f dockerd || true && "
                        "sudo rm -f /var/run/docker.sock && "
                        "sudo nohup dockerd > /var/log/docker.log 2>&1 & "
                        "sleep 5 && "  // Wait for daemon to fully start
                        "sudo docker info > /dev/null 2>&1 && echo \"DOCKER_RUNNING\" || echo \"DOCKER_NOT_RUNNING\"'";
                    
                    result.clear();
                    pipe = std::unique_ptr<FILE, decltype(&pclose)>(popen(alt_start_cmd.c_str(), "r"), pclose);
                    
                    if (pipe) {
                        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                            result += buffer.data();
                        }
                    }
                    
                    if (result.find("DOCKER_RUNNING") == std::string::npos) {
                        std::cerr << "Failed to start Docker daemon using alternative method on " << hostname << std::endl;
                        return false;
                    }
                }
                
                std::cout << "Docker daemon started successfully" << std::endl;
            } else {
                std::cout << "Docker daemon is already running" << std::endl;
            }
            
            // First, copy the Dockerfile and source files to the remote server
            std::cout << "Copying Docker files to remote server..." << std::endl;
            
            // Create build directory first
            std::string mkdir_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                mkdir_cmd += "-i " + key_path + " ";
            }
            mkdir_cmd += "-p " + std::to_string(port) + " ";
            mkdir_cmd += username + "@" + hostname + " 'rm -rf /tmp/leaf-build && mkdir -p /tmp/leaf-build && chmod 777 /tmp/leaf-build'";
            
            if (std::system(mkdir_cmd.c_str()) != 0) {
                std::cerr << "Failed to create build directory on " << hostname << std::endl;
                return false;
            }
            
            std::string scp_cmd = "scp -v -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                scp_cmd += "-i " + key_path + " ";
            }
            scp_cmd += "-P " + std::to_string(port) + " ";
            scp_cmd += "Dockerfile docker-run.sh src/leaf/server_test.cpp src/leaf/server_test.proto " + username + "@" + hostname + ":/tmp/leaf-build/";
            
            if (std::system(scp_cmd.c_str()) != 0) {
                std::cerr << "Failed to copy Docker files to " << hostname << std::endl;
                return false;
            }
            std::cout << "Docker files copied successfully" << std::endl;

            // Verify files were copied
            // DO BETTER VERIFICATION

            // Build and run the Docker container
            std::cout << "Starting Docker container..." << std::endl;
            std::string ssh_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                ssh_cmd += "-i " + key_path + " ";
            }
            ssh_cmd += "-p " + std::to_string(port) + " ";
            ssh_cmd += username + "@" + hostname + " 'cd /tmp/leaf-build && chmod +x docker-run.sh && ./docker-run.sh'";
            
            if (std::system(ssh_cmd.c_str()) != 0) {
                std::cerr << "Failed to start Docker container on " << hostname << std::endl;
                return false;
            }
            std::cout << "Docker container started successfully" << std::endl;

            // Wait for the server to start
            std::cout << "Waiting for server to start..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));

            // Verify container is running
            std::cout << "Verifying container status..." << std::endl;
            ssh_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
            if (!key_path.empty()) {
                ssh_cmd += "-i " + key_path + " ";
            }
            ssh_cmd += "-p " + std::to_string(port) + " ";
            ssh_cmd += username + "@" + hostname + " 'docker ps | grep leaf-grpc-server'";
            
            if (std::system(ssh_cmd.c_str()) != 0) {
                std::cerr << "Docker container is not running on " << hostname << std::endl;
                return false;
            }
            std::cout << "Container is running" << std::endl;

            // Set up SSH tunnel for gRPC connection
            if (!setup_ssh_tunnel()) {
                std::cerr << "Failed to set up SSH tunnel" << std::endl;
                return false;
            }

            // Create gRPC channel and stub
            std::cout << "Creating gRPC channel..." << std::endl;
            std::string target = "localhost:50051";  // Use localhost for SSH tunneling
            std::cout << "Connecting to gRPC target: " << target << std::endl;
            auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
            auto stub = leaftest::ServerTest::NewStub(channel);

            // Try to get server time
            std::cout << "Attempting to get server time..." << std::endl;
            grpc::ClientContext context;
            leaftest::TimeRequest request;
            leaftest::TimeResponse response;
            
            // Set a timeout for the RPC call (10 seconds for debugging)
            context.set_deadline(std::chrono::system_clock::now() + 
                               std::chrono::seconds(10));
            
            std::cout << "Making RPC call to GetServerTime..." << std::endl;
            auto status = stub->GetServerTime(&context, request, &response);
            
            if (status.ok()) {
                std::cout << "Successfully connected to " << hostname 
                          << " (Server time: " << response.server_time_ms() << " ms)" << std::endl;
                return true;
            } else {
                std::cerr << "Failed to get server time from " << hostname 
                          << ": " << status.error_message() << std::endl;
                std::cerr << "Error code: " << status.error_code() << std::endl;
                std::cerr << "Error details: " << status.error_details() << std::endl;
                return false;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error verifying gRPC connection: " << e.what() << std::endl;
            return false;
        }
    }

public:
    UserCredentials(const std::string& user, const std::string& host, 
                   int p = 22, const std::string& key = "")
        : username(user), hostname(host), port(p), key_path(key), is_connected(false), tunnel_pid(0) {}

    ~UserCredentials() {
        cleanup_ssh_tunnel();
    }

    bool verify_connection() {
        std::cout << "Starting connection verification..." << std::endl;
        // First verify basic SSH connection
        if (!verify_ssh_connection()) {
            std::cout << "SSH verification failed" << std::endl;
            is_connected = false;
            return false;
        }

        std::cout << "SSH verification successful, proceeding to gRPC verification" << std::endl;
        // Then verify gRPC connection
        is_connected = verify_grpc_connection();
        std::cout << "gRPC verification " << (is_connected ? "successful" : "failed") << std::endl;
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
    int get_tunnel_pid() const { return tunnel_pid; }
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