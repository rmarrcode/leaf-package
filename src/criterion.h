#ifndef CRITERION_H
#define CRITERION_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <string>
#include <memory>
#include <vector>

namespace py = pybind11;

// Forward declarations
class LeafTrainer;
class Model;
class DistributedModel;

class Criterion {
private:
    py::object pytorch_criterion;
    LeafTrainer* leaf_trainer;
    float loss;  // Store the computed loss
    std::vector<py::object> stored_outputs;
    std::vector<py::object> stored_targets;
    std::vector<py::object> divided_targets; // Store divided targets for distributed computation

public:
    Criterion(py::object criterion, LeafTrainer* trainer);
    
    // Forward pass method that computes loss
    bool forward(py::object outputs, py::object targets);
    
    // Forward pass with divided targets for distributed computation
    bool forward_distributed(py::object outputs, py::object divided_targets);
    
    // Call operator to make it behave like a PyTorch criterion
    bool operator()(py::object outputs, py::object targets);
    
    // Get the underlying PyTorch criterion
    py::object get_pytorch_criterion() const;
    
    // Get the LeafTrainer pointer
    LeafTrainer* get_leaf_trainer() const;
    
    // Get the computed loss
    float get_loss() const;
    
    // Set the loss value
    void set_loss(float loss_value);
    
    // Get stored outputs
    const std::vector<py::object>& get_stored_outputs() const;
    
    // Get stored targets
    const std::vector<py::object>& get_stored_targets() const;
    
    // Get divided targets
    const std::vector<py::object>& get_divided_targets() const;
    
    // Clear stored data
    void clear_stored_data();
    
    // Delegate common PyTorch criterion methods
    py::object getattr(const std::string& name);
    
    // Set criterion attributes
    void setattr(const std::string& name, py::object value);
    
    // Check if criterion has an attribute
    bool hasattr(const std::string& name);
    
    // Divide targets for distributed computation (similar to input division)
    std::vector<py::object> divide_targets(py::object targets, const std::vector<std::string>& server_names);
};

#endif // CRITERION_H 