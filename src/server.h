#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <regex>
#include "user_credentials.h"

class ComputeResource {
private:
    std::string name;
    std::string type;
    std::map<std::string, std::string> properties;

public:
    ComputeResource(const std::string& n, const std::string& t);
    void add_property(const std::string& key, const std::string& value);
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

    void discover_resources();

public:
    // Default constructor required for std::map
    Server();
    
    // Main constructor
    Server(const std::string& server_name, const UserCredentials& creds, bool local = false);

    bool is_server_connected() const { return is_connected; }
    bool is_local_server() const { return is_local; }
    std::string get_name() const { return name; }
    UserCredentials get_credentials() const { return credentials; }
    std::vector<ComputeResource> get_resources() const { return resources; }
};

#endif // SERVER_H 