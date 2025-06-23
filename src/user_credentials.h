#ifndef USER_CREDENTIALS_H
#define USER_CREDENTIALS_H

#include <string>
#include <memory>
#include <set>
#include <mutex>
#include <array>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include "server_communication.pb.h"
#include "server_communication.grpc.pb.h"

class UserCredentials {
private:
    std::string username;
    std::string hostname;
    int port;
    std::string key_path;
    bool is_connected;
    int tunnel_pid;  // PID of SSH tunnel process
    int tunnel_port; // Local port for SSH tunnel
    std::shared_ptr<int> tunnel_ref_count; // Reference count for tunnel ownership

    // Static member to track used ports
    static std::set<int> used_ports;
    static std::mutex port_mutex;

    int find_available_port();
    void release_port();
    bool setup_ssh_tunnel();
    void cleanup_ssh_tunnel();
    bool verify_ssh_connection();
    bool verify_remote_docker_installation();
    bool install_remote_docker();
    std::string verify_remote_docker_daemon_status();
    bool start_remote_docker_daemon();
    bool copy_docker_files_to_remote_server();
    bool build_run_docker_container();
    bool test_grpc_connection();
    bool verify_grpc_connection();

public:
    UserCredentials(const std::string& user, const std::string& host, 
                   int p = 22, const std::string& key = "");
    
    // Copy constructor
    UserCredentials(const UserCredentials& other);
    
    // Copy assignment operator
    UserCredentials& operator=(const UserCredentials& other);
    
    // Destructor
    ~UserCredentials();

    bool verify_connection();
    std::string get_connection_string() const;
    bool get_connection_status() const { return is_connected; }
    std::string get_username() const { return username; }
    std::string get_hostname() const { return hostname; }
    int get_port() const { return port; }
    std::string get_key_path() const { return key_path; }
    int get_tunnel_pid() const { return tunnel_pid; }
    int get_tunnel_port() const { return tunnel_port; }
};

#endif // USER_CREDENTIALS_H 