#ifndef CORE_H
#define CORE_H

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
#include "model.h"
#include "criterion.h"

namespace py = pybind11;

// Forward declarations
class DistributedModel;
class Criterion;

class LeafConfig {
private:
    std::map<std::string, Server> servers;

    void discover_local_resources();
    void print_server_info(const Server& server) const;

public:
    LeafConfig();

    void add_server(const std::string& server_name,
                   const std::string& username,
                   const std::string& hostname,
                   int port = 22,
                   const std::string& key_path = "");

    std::vector<std::string> get_servers() const;
    py::dict get_server_info(const std::string& server_name) const;
    void remove_server(const std::string& server_name);
    void print_all_resources() const;
    std::pair<int, std::string> get_server_connection_info(const std::string& server_name) const;
};

class LeafTrainer {
private:
    LeafConfig config;
    std::map<std::string, std::shared_ptr<grpc::Channel>> server_channels;
    std::mutex channel_mutex;
    std::vector<std::shared_ptr<Model>> local_models;  // Track local models
    std::vector<std::shared_ptr<DistributedModel>> distributed_models; // Track distributed models
    std::vector<std::shared_ptr<Criterion>> criterions; // Track criterions
    mutable std::mutex models_mutex;  // Protect access to local_models
    std::unique_ptr<ServerCommunicationServiceImpl> local_service;  // Persistent local service instance

    std::shared_ptr<grpc::Channel> create_channel(const std::string& server_name);
    std::pair<std::vector<float>, float> get_gradients_from_server(
        const std::string& server_name,
        py::object inputs,
        py::object model,
        py::object criterion,
        py::object optimizer,
        bool is_local = false);
    std::vector<std::pair<std::string, std::vector<size_t>>> distribute_batch(
        const std::vector<std::string>& server_names,
        size_t batch_size);
    
    // Divide targets the same way inputs are divided
    py::object divide_targets(py::object targets, 
                             const std::vector<std::pair<std::string, std::vector<size_t>>>& distribution);

public:
    py::object forward_pass_on_server(
        const std::string& server_name,
        py::object inputs,
        uint32_t model_index,
        bool is_local = false);
    LeafTrainer(const LeafConfig& cfg);
    ~LeafTrainer();
    
    // Delete copy constructor and assignment operator due to mutex members
    LeafTrainer(const LeafTrainer&) = delete;
    LeafTrainer& operator=(const LeafTrainer&) = delete;
    
    // Move constructor and assignment are implicitly deleted due to mutex members

    void cleanup_models();
    size_t get_model_count() const;
    std::vector<std::string> get_server_names() const;
    py::dict get_server_info(const std::string& server_name) const;
    std::pair<bool, std::string> store_model_weights_on_server(
        const std::string& server_name,
        const std::vector<float>& model_state,
        const std::string& model_id);
    py::object register_model(py::object model);
    py::object register_criterion(py::object model, py::object criterion);
    py::dict train(py::object model, 
                   py::object optimizer, 
                   py::object train_loader, 
                   int epochs,
                   py::object criterion = py::none());
    py::dict test_with_hardcoded_values();
};

#endif // CORE_H 