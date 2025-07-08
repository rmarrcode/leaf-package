#include "model.h"
#include <iostream>
#include <cstring>

namespace py = pybind11;

Model::Model(py::object model, LeafTrainer* trainer)
    : pytorch_model(model), leaf_trainer(trainer), computed_outputs(false), loss(py::none()) {}

bool Model::forward(py::object input) {
    try {
        // Call the original model's forward method
        py::object output = pytorch_model.attr("forward")(input);
        
        // Store the output
        stored_outputs.push_back(output);
        computed_outputs = true;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error in Model::forward: " << e.what() << std::endl;
        return false;
    }
}

bool Model::operator()(py::object input) {
    return forward(input);
}

py::object Model::get_pytorch_model() const {
    return pytorch_model;
}

LeafTrainer* Model::get_leaf_trainer() const {
    return leaf_trainer;
}

bool Model::has_computed_outputs() const {
    return computed_outputs;
}

const std::vector<py::object>& Model::get_stored_outputs() const {
    return stored_outputs;
}

void Model::clear_stored_outputs() {
    stored_outputs.clear();
    computed_outputs = false;
}

py::object Model::get_loss() const {
    return loss;
}

void Model::set_loss(py::object loss_value) {
    loss = loss_value;
}

void Model::clear_loss() {
    loss = py::none();
}

py::object Model::state_dict() {
    return pytorch_model.attr("state_dict")();
}

py::object Model::parameters() {
    return pytorch_model.attr("parameters")();
}

py::object Model::named_parameters() {
    return pytorch_model.attr("named_parameters")();
}

py::object Model::train() {
    return pytorch_model.attr("train")();
}

py::object Model::eval() {
    return pytorch_model.attr("eval")();
}

py::object Model::to(py::object device) {
    return pytorch_model.attr("to")(device);
}

py::object Model::cpu() {
    return pytorch_model.attr("cpu")();
}

py::object Model::cuda() {
    return pytorch_model.attr("cuda")();
}

py::object Model::getattr(const std::string& name) {
    return pytorch_model.attr(name.c_str());
}

void Model::setattr(const std::string& name, py::object value) {
    pytorch_model.attr(name.c_str()) = value;
}

bool Model::hasattr(const std::string& name) {
    return py::hasattr(pytorch_model, name.c_str());
}

std::vector<float> Model::serialize_state() const {
    std::vector<float> model_state;
    
    try {
        // Extract model state from PyTorch model
        py::object state_dict = pytorch_model.attr("state_dict")();
        
        // Convert state dict to flat vector
        py::object items = state_dict.attr("items")();
        
        for (auto item : items) {
            // Extract the tensor value from the key-value pair
            py::object tensor = item.attr("__getitem__")(1);  // Get the value (tensor) from the key-value pair
            
            // Check if tensor has cpu() method
            if (py::hasattr(tensor, "cpu")) {
                py::object cpu_tensor = tensor.attr("cpu")();
                if (py::hasattr(cpu_tensor, "flatten")) {
                    // Flatten the tensor to 1D first, then convert to numpy
                    py::object flattened_tensor = cpu_tensor.attr("flatten")();
                    if (py::hasattr(flattened_tensor, "numpy")) {
                        py::array_t<float> numpy_array = flattened_tensor.attr("numpy")();
                        auto buffer = numpy_array.unchecked<1>();
                        for (py::ssize_t i = 0; i < buffer.size(); ++i) {
                            model_state.push_back(buffer[i]);
                        }
                    } else {
                        std::cout << "Warning: Flattened tensor does not have numpy() method" << std::endl;
                    }
                } else {
                    std::cout << "Warning: CPU tensor does not have flatten() method" << std::endl;
                }
            } else {
                std::cout << "Warning: Tensor does not have cpu() method" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error during state dict serialization: " << e.what() << std::endl;
        throw;
    }
    
    return model_state;
}

void Model::deserialize_state(const std::vector<float>& state) {
    try {
        // Extract model state from PyTorch model
        py::object state_dict = pytorch_model.attr("state_dict")();
        
        // Convert state dict to flat vector
        py::object items = state_dict.attr("items")();
        size_t state_index = 0;
        
        for (auto item : items) {
            // Extract the tensor value from the key-value pair
            py::object tensor = item.attr("__getitem__")(1);  // Get the value (tensor) from the key-value pair
            
            // Check if tensor has cpu() method
            if (py::hasattr(tensor, "cpu")) {
                py::object cpu_tensor = tensor.attr("cpu")();
                if (py::hasattr(cpu_tensor, "flatten")) {
                    // Flatten the tensor to 1D first, then convert to numpy
                    py::object flattened_tensor = cpu_tensor.attr("flatten")();
                    if (py::hasattr(flattened_tensor, "numpy")) {
                        py::array_t<float> numpy_array = flattened_tensor.attr("numpy")();
                        auto buffer = numpy_array.mutable_unchecked<1>();
                        
                        // Copy state data back to tensor
                        for (py::ssize_t i = 0; i < buffer.size() && state_index < state.size(); ++i) {
                            buffer(i) = state[state_index++];
                        }
                        
                        // Update the original tensor with the new values
                        // This is a simplified approach - in practice, you might need to handle this differently
                        // depending on how PyTorch tensors are managed
                    } else {
                        std::cout << "Warning: Flattened tensor does not have numpy() method" << std::endl;
                    }
                } else {
                    std::cout << "Warning: CPU tensor does not have flatten() method" << std::endl;
                }
            } else {
                std::cout << "Warning: Tensor does not have cpu() method" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error during state dict deserialization: " << e.what() << std::endl;
        throw;
    }
} 