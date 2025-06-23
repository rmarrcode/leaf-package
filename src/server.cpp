#include "server.h"

// ComputeResource implementation
ComputeResource::ComputeResource(const std::string& n, const std::string& t)
    : name(n), type(t) {}

void ComputeResource::add_property(const std::string& key, const std::string& value) {
    properties[key] = value;
}

// Server implementation
Server::Server() : name(""), credentials("", "", 0, ""), is_connected(false), is_local(false) {}

Server::Server(const std::string& server_name, const UserCredentials& creds, bool local)
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

void Server::discover_resources() {
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