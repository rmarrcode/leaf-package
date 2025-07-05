#include "distributed_model.h"
#include "model.h"
#include "core.h"
#include <iostream>
#include <stdexcept>

namespace py = pybind11;

DistributedModel::DistributedModel(std::shared_ptr<Model> model, LeafTrainer* trainer)
    : model(model), leaf_trainer(trainer) {}

py::object DistributedModel::forward(py::object input) {
    // Get number of servers from trainer
    auto server_names = leaf_trainer->get_server_names();
    size_t num_servers = server_names.size();
    if (num_servers == 0) {
        throw std::runtime_error("No servers available for distributed forward");
    }
    // Assume input is a batch tensor, split along dim 0
    py::object torch = py::module_::import("torch");
    py::list input_chunks = torch.attr("chunk")(input, num_servers, 0);
    py::list outputs;
    for (size_t i = 0; i < num_servers; ++i) {
        // For now, call the local model for all servers for demonstration
        // In a real implementation, you would have a remote forward RPC
        outputs.append(model->forward(input_chunks[i]));
    }
    // Concatenate outputs along dim 0
    return torch.attr("cat")(outputs, 0);
}

py::object DistributedModel::operator()(py::object input) {
    return forward(input);
}

py::object DistributedModel::get_pytorch_model() const { 
    return model->get_pytorch_model(); 
}

LeafTrainer* DistributedModel::get_leaf_trainer() const { 
    return leaf_trainer; 
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