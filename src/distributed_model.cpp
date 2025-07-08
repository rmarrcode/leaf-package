#include "distributed_model.h"
#include "model.h"
#include "core.h"
#include <iostream>
#include <stdexcept>

namespace py = pybind11;

DistributedModel::DistributedModel(std::shared_ptr<Model> model, LeafTrainer* trainer, size_t index)
    : model(model), leaf_trainer(trainer), index(index) {}

bool DistributedModel::forward(py::object input) {
    // Get number of servers from trainer
    auto server_names = leaf_trainer->get_server_names();
    size_t num_servers = server_names.size();
    if (num_servers == 0) {
        std::cout << "Error: No servers available for distributed forward" << std::endl;
        return false;
    }
    
    // Track if all connected servers successfully processed the forward pass
    bool all_success = true;
    size_t connected_servers = 0;
    size_t successful_servers = 0;
    
    // Get server info to determine which servers are local vs remote
    for (size_t i = 0; i < num_servers; ++i) {
        const std::string& server_name = server_names[i];
        py::dict server_info = leaf_trainer->get_server_info(server_name);
        bool is_local = server_info["is_local"].cast<bool>();
        bool is_connected = server_info["connected"].cast<bool>();
        
        if (!is_connected) {
            std::cout << "Warning: Server " << server_name << " is not connected, skipping" << std::endl;
            continue;
        }
        
        connected_servers++;
        
        try {
            // Use the model index to perform forward pass on the specific server
            py::object output = leaf_trainer->forward_pass_on_server(server_name, input, static_cast<uint32_t>(index), is_local);
            successful_servers++;
        } catch (const std::exception& e) {
            std::cout << "Error on server " << server_name << ": " << e.what() << std::endl;
            all_success = false;
        }
    }
    
    if (connected_servers == 0) {
        std::cout << "Error: No connected servers available for distributed forward" << std::endl;
        return false;
    }
    
    std::cout << "Distributed forward completed: " << successful_servers << "/" << connected_servers << " servers successful" << std::endl;
    return all_success;
}

bool DistributedModel::operator()(py::object input) {
    return forward(input);
}

py::object DistributedModel::get_pytorch_model() const { 
    return model->get_pytorch_model(); 
}

LeafTrainer* DistributedModel::get_leaf_trainer() const { 
    return leaf_trainer; 
}

size_t DistributedModel::get_index() const { 
    return index; 
}

py::object DistributedModel::state_dict() { 
    return model->state_dict(); 
}

py::object DistributedModel::parameters() { 
    return model->parameters(); 
}

py::object DistributedModel::named_parameters() { 
    return model->named_parameters(); 
}

py::object DistributedModel::train() { 
    return model->train(); 
}

py::object DistributedModel::eval() { 
    return model->eval(); 
}

py::object DistributedModel::to(py::object device) { 
    return model->to(device); 
}

py::object DistributedModel::cpu() { 
    return model->cpu(); 
}

py::object DistributedModel::cuda() { 
    return model->cuda(); 
}

py::object DistributedModel::getattr(const std::string& name) { 
    return model->getattr(name); 
}

void DistributedModel::setattr(const std::string& name, py::object value) { 
    model->setattr(name, value); 
}

bool DistributedModel::hasattr(const std::string& name) { 
    return model->hasattr(name); 
}

std::vector<float> DistributedModel::serialize_state() const { 
    return model->serialize_state(); 
}

void DistributedModel::deserialize_state(const std::vector<float>& state) { 
    model->deserialize_state(state); 
}