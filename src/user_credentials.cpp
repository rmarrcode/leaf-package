#include "user_credentials.h"

// Static member definitions
std::set<int> UserCredentials::used_ports;
std::mutex UserCredentials::port_mutex;

int UserCredentials::find_available_port() {
    std::lock_guard<std::mutex> lock(port_mutex);
    int start_port = 50051;
    int max_port = 50100; // Allow up to 50 ports
    
    for (int port = start_port; port <= max_port; port++) {
        if (used_ports.find(port) == used_ports.end()) {
            // Check if port is actually available
            std::string check_cmd = "lsof -Pi :" + std::to_string(port) + " -sTCP:LISTEN -t 2>/dev/null";
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check_cmd.c_str(), "r"), pclose);
            
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    result += buffer.data();
                }
            }
            
            if (result.empty()) {
                used_ports.insert(port);
                return port;
            }
        }
    }
    throw std::runtime_error("No available ports for SSH tunnel");
}

void UserCredentials::release_port() {
    std::lock_guard<std::mutex> lock(port_mutex);
    if (tunnel_port > 0) {
        used_ports.erase(tunnel_port);
        tunnel_port = 0;
    }
}

bool UserCredentials::setup_ssh_tunnel() {
    // Find an available port for this tunnel
    try {
        tunnel_port = find_available_port();
    } catch (const std::exception& e) {
        std::cerr << "Failed to find available port: " << e.what() << std::endl;
        return false;
    }
    
    // Check if tunnel is already running on this port
    std::string check_cmd = "lsof -Pi :" + std::to_string(tunnel_port) + " -sTCP:LISTEN -t 2>/dev/null";
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check_cmd.c_str(), "r"), pclose);
    
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
    }
    
    if (!result.empty()) {
        std::cout << "Port " << tunnel_port << " is already in use. Killing existing tunnel..." << std::endl;
        std::string kill_cmd = "pkill -f 'ssh.*" + std::to_string(tunnel_port) + ":localhost'";
        std::system(kill_cmd.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Build SSH tunnel command with dynamic port
    std::string ssh_cmd = "ssh -L " + std::to_string(tunnel_port) + ":localhost:50051 ";
    if (!key_path.empty()) {
        ssh_cmd += "-i " + key_path + " ";
    }
    ssh_cmd += "-p " + std::to_string(port) + " ";
    ssh_cmd += username + "@" + hostname + " -N";
            
    // Start tunnel in background
    ssh_cmd += " &";
    int result_code = std::system(ssh_cmd.c_str());
    
    if (result_code != 0) {
        std::cerr << "Failed to start SSH tunnel on port " << tunnel_port << std::endl;
        release_port();
        return false;
    }
    
    // Wait for tunnel to establish
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Verify tunnel is working
    std::string verify_cmd = "lsof -Pi :" + std::to_string(tunnel_port) + " -sTCP:LISTEN -t 2>/dev/null";
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
            std::cout << "SSH tunnel established on port " << tunnel_port << " (PID: " << tunnel_pid << ")" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse tunnel PID: " << e.what() << std::endl;
            release_port();
            return false;
        }
    } else {
        std::cerr << "SSH tunnel verification failed on port " << tunnel_port << std::endl;
        release_port();
        return false;
    }
}

void UserCredentials::cleanup_ssh_tunnel() {
    if (tunnel_pid > 0) {
        std::cout << "Cleaning up SSH tunnel (PID: " << tunnel_pid << ") on port " << tunnel_port << "..." << std::endl;
        std::string kill_cmd = "kill " + std::to_string(tunnel_pid);
        std::system(kill_cmd.c_str());
        tunnel_pid = 0;
    }
    release_port();
}

bool UserCredentials::verify_ssh_connection() {
    std::string test_ssh_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
    if (!key_path.empty()) {
        test_ssh_cmd += "-i " + key_path + " ";
    }
    test_ssh_cmd += "-p " + std::to_string(port) + " ";
    test_ssh_cmd += username + "@" + hostname + " 'echo SSH connection test successful'";
    if (std::system(test_ssh_cmd.c_str()) != 0) {
        std::cerr << "SSH connection test failed. Please verify:" << std::endl;
        std::cerr << "1. SSH port " << port << " is correct" << std::endl;
        std::cerr << "2. Username " << username << " is correct" << std::endl;
        std::cerr << "3. SSH key is properly set up" << std::endl;
        return false;
    }
    return true;
}

bool UserCredentials::verify_remote_docker_installation() {
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
        return false;
    } else {
        return true;
    }
}

bool UserCredentials::install_remote_docker() {
    std::cout << "Docker not found. Installing Docker..." << std::endl;   
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
    return true;
}

std::string UserCredentials::verify_remote_docker_daemon_status() {
    std::string result;
    // Check if Docker daemon is running and start it if needed
    std::string check_daemon_cmd = "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
    if (!key_path.empty()) {
        check_daemon_cmd += "-i " + key_path + " ";
    }
    check_daemon_cmd += "-p " + std::to_string(port) + " ";
    check_daemon_cmd += username + "@" + hostname + " 'sudo systemctl is-active docker || echo \"DOCKER_NOT_RUNNING\"'";
    
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check_daemon_cmd.c_str(), "r"), pclose);
    
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
    }
    return result;
}

bool UserCredentials::start_remote_docker_daemon() {
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
    if (std::system(setup_docker_cmd.c_str()) != 0) {
        std::cerr << "Failed to set up Docker daemon on " << hostname << std::endl;
        return false;
    }
    return true;
}

bool UserCredentials::copy_docker_files_to_remote_server() {
    // Create build directory
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
    // Copy Docker files to build directory
    std::string scp_cmd = "scp -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no ";
    if (!key_path.empty()) {
        scp_cmd += "-i " + key_path + " ";
    }
    scp_cmd += "-P " + std::to_string(port) + " ";
    scp_cmd += "Dockerfile docker-run.sh src/server_communication.cpp src/server_communication.h src/server_communication.proto " + username + "@" + hostname + ":/tmp/leaf-build/";
    if (std::system(scp_cmd.c_str()) != 0) {
        std::cerr << "Failed to copy Docker files to " << hostname << std::endl;
        return false;
    }
    std::cout << "Docker files copied successfully" << std::endl;
    return true;
}

bool UserCredentials::build_run_docker_container() {
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

    // Wait for the server to start
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify container is running
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
    return true;
}

bool UserCredentials::test_grpc_connection() {
    std::string target = "localhost:" + std::to_string(tunnel_port);
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = leaftest::ServerCommunication::NewStub(channel);
    // Try to get server time
    grpc::ClientContext context;
    leaftest::TimeRequest request;
    leaftest::TimeResponse response;
    context.set_deadline(std::chrono::system_clock::now() + 
                        std::chrono::seconds(10));
    
    auto status = stub->GetServerTime(&context, request, &response);
    if (!status.ok()) {
        return false;
    } 
    return true;
}

bool UserCredentials::verify_grpc_connection() {
    try {         
        
        if (!verify_ssh_connection()) {
            return false;
        }
        std::cout << hostname << " : ssh connection successful" << std::endl;

        if (!verify_remote_docker_installation()) {
            if (!install_remote_docker()) {
                return false;
            }
        }
        std::cout << hostname << " :docker verification successful" << std::endl;

        std::string result = verify_remote_docker_daemon_status();
        if (result.find("DOCKER_NOT_RUNNING") != std::string::npos || result.find("inactive") != std::string::npos) {
            if (!start_remote_docker_daemon()) {
                return false;
            }
        }
        std::cout << hostname << " : docker daemon verification successful" << std::endl;

        if (!copy_docker_files_to_remote_server()) {
            return false;
        }
        std::cout << hostname << " : docker files copied successful" << std::endl;
        
        if (!build_run_docker_container()) {
            return false;
        }
        std::cout << hostname << " : docker container built successful" << std::endl;
        
        if (!setup_ssh_tunnel()) {
            return false;
        }
        std::cout << hostname << " : ssh tunnel setup successful" << std::endl;
        
        if (!test_grpc_connection()) {
            return false;
        }
        std::cout << hostname << " : grpc connection successful" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error verifying gRPC connection: " << e.what() << std::endl;
        return false;
    } 
}

// Constructor
UserCredentials::UserCredentials(const std::string& user, const std::string& host, 
                               int p, const std::string& key)
    : username(user), hostname(host), port(p), key_path(key), is_connected(false), 
      tunnel_pid(0), tunnel_port(0), tunnel_ref_count(std::make_shared<int>(1)) {}

// Copy constructor
UserCredentials::UserCredentials(const UserCredentials& other)
    : username(other.username), hostname(other.hostname), port(other.port), 
      key_path(other.key_path), is_connected(other.is_connected), 
      tunnel_pid(other.tunnel_pid), tunnel_port(other.tunnel_port), tunnel_ref_count(other.tunnel_ref_count) {
    // Increment reference count
    if (tunnel_ref_count) {
        (*tunnel_ref_count)++;
    }
}

// Copy assignment operator
UserCredentials& UserCredentials::operator=(const UserCredentials& other) {
    if (this != &other) {
        // Decrement old reference count
        if (tunnel_ref_count) {
            (*tunnel_ref_count)--;
        }
        
        username = other.username;
        hostname = other.hostname;
        port = other.port;
        key_path = other.key_path;
        is_connected = other.is_connected;
        tunnel_pid = other.tunnel_pid;
        tunnel_port = other.tunnel_port;
        tunnel_ref_count = other.tunnel_ref_count;
        
        // Increment new reference count
        if (tunnel_ref_count) {
            (*tunnel_ref_count)++;
        }
    }
    return *this;
}

// Destructor
UserCredentials::~UserCredentials() {
    // Decrement reference count and clean up if this is the last reference
    if (tunnel_ref_count) {
        (*tunnel_ref_count)--;
        if (*tunnel_ref_count == 0 && (tunnel_pid > 0 || tunnel_port > 0)) {
            cleanup_ssh_tunnel();
        }
    }
}

bool UserCredentials::verify_connection() {
    is_connected = verify_grpc_connection();
    std::cout << "gRPC verification " << (is_connected ? "successful" : "failed") << std::endl;
    return is_connected;
}

std::string UserCredentials::get_connection_string() const {
    return username + "@" + hostname + ":" + std::to_string(port);
} 