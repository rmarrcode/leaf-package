#ifndef CRITERION_H
#define CRITERION_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <string>
#include <memory>
#include <vector>

namespace py = pybind11;

// Forward declaration
class LeafTrainer;
class Model;

class Criterion {
private:
    py::object pytorch_criterion;
    LeafTrainer* leaf_trainer;
    std::vector<py::object> stored_losses;

public:
    Criterion(py::object criterion, LeafTrainer* trainer);
    
    // Calculate loss for outputs and targets, dividing targets the same way inputs were divided
    py::object operator()(py::object outputs, py::object targets);
    
    // Get the underlying PyTorch criterion
    py::object get_pytorch_criterion() const;
    
    // Get the LeafTrainer pointer
    LeafTrainer* get_leaf_trainer() const;
    
    // Get stored losses
    const std::vector<py::object>& get_stored_losses() const;
    
    // Clear stored losses
    void clear_stored_losses();
    
    // Calculate loss for a specific model with its corresponding target slice
    py::object calculate_loss_for_model(py::object model_outputs, py::object target_slice, Model* model);
    
    // Distribute targets across models based on how inputs were distributed
    std::vector<py::object> distribute_targets(py::object targets, const std::vector<Model*>& models);
    
    // Delegate common PyTorch criterion methods to the underlying criterion
    py::object getattr(const std::string& name);
    void setattr(const std::string& name, py::object value);
    bool hasattr(const std::string& name);
};

#endif // CRITERION_H 