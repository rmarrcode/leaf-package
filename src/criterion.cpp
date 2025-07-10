#include "criterion.h"
#include <iostream>
#include <cstring>

namespace py = pybind11;

Criterion::Criterion(py::object criterion, LeafTrainer* trainer)
    : pytorch_criterion(criterion), leaf_trainer(trainer), loss(0.0f) {}

bool Criterion::forward(py::object outputs, py::object targets) {
    try {
        // Store outputs and targets for later use
        stored_outputs.push_back(outputs);
        stored_targets.push_back(targets);
        
        // Call the original criterion's forward method
        py::object loss_tensor = pytorch_criterion.attr("__call__")(outputs, targets);
        
        // Convert loss tensor to float
        if (py::hasattr(loss_tensor, "item")) {
            loss = loss_tensor.attr("item")().cast<float>();
        } else if (py::hasattr(loss_tensor, "cpu")) {
            py::object cpu_tensor = loss_tensor.attr("cpu")();
            if (py::hasattr(cpu_tensor, "numpy")) {
                py::array_t<float> numpy_array = cpu_tensor.attr("numpy")();
                auto buffer = numpy_array.unchecked<1>();
                if (buffer.size() > 0) {
                    loss = buffer[0];
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error in Criterion::forward: " << e.what() << std::endl;
        return false;
    }
}

bool Criterion::operator()(py::object outputs, py::object targets) {
    return forward(outputs, targets);
}

bool Criterion::forward_distributed(py::object outputs, py::object divided_targets) {
    try {
        // Store outputs and divided targets for later use
        stored_outputs.push_back(outputs);
        stored_targets.push_back(divided_targets);
        
        // For distributed computation, we need to handle the divided targets
        // This is a simplified implementation - in practice, you might need to
        // concatenate the divided targets or handle them differently based on your needs
        
        // For now, we'll just use the first target from the divided targets
        // In a real implementation, you might want to concatenate all targets
        py::object targets = divided_targets.attr("__getitem__")(0);
        
        // Call the original criterion's forward method
        py::object loss_tensor = pytorch_criterion.attr("__call__")(outputs, targets);
        
        // Convert loss tensor to float
        if (py::hasattr(loss_tensor, "item")) {
            loss = loss_tensor.attr("item")().cast<float>();
        } else if (py::hasattr(loss_tensor, "cpu")) {
            py::object cpu_tensor = loss_tensor.attr("cpu")();
            if (py::hasattr(cpu_tensor, "numpy")) {
                py::array_t<float> numpy_array = cpu_tensor.attr("numpy")();
                auto buffer = numpy_array.unchecked<1>();
                if (buffer.size() > 0) {
                    loss = buffer[0];
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "Error in Criterion::forward_distributed: " << e.what() << std::endl;
        return false;
    }
}

py::object Criterion::get_pytorch_criterion() const {
    return pytorch_criterion;
}

LeafTrainer* Criterion::get_leaf_trainer() const {
    return leaf_trainer;
}

float Criterion::get_loss() const {
    return loss;
}

void Criterion::set_loss(float loss_value) {
    loss = loss_value;
}

const std::vector<py::object>& Criterion::get_stored_outputs() const {
    return stored_outputs;
}

const std::vector<py::object>& Criterion::get_stored_targets() const {
    return stored_targets;
}

void Criterion::clear_stored_data() {
    stored_outputs.clear();
    stored_targets.clear();
    loss = 0.0f;
}

py::object Criterion::getattr(const std::string& name) {
    return pytorch_criterion.attr(name.c_str());
}

void Criterion::setattr(const std::string& name, py::object value) {
    pytorch_criterion.attr(name.c_str()) = value;
}

bool Criterion::hasattr(const std::string& name) {
    return py::hasattr(pytorch_criterion, name.c_str());
} 