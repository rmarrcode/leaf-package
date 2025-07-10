#ifndef DISTRIBUTED_MODEL_H
#define DISTRIBUTED_MODEL_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <string>
#include <memory>
#include <vector>

namespace py = pybind11;

// Forward declarations
class Criterion;

// Forward declarations
class LeafTrainer;
class Model;

class DistributedModel {
private:
    std::shared_ptr<Model> model;
    LeafTrainer* leaf_trainer;
    size_t index; // Index in the distributed_models array
    float loss;  // Store the computed loss
    std::shared_ptr<Criterion> criterion;  // Pointer to the registered criterion

public:
    DistributedModel(std::shared_ptr<Model> model, LeafTrainer* trainer, size_t index);

    // Forward pass: distribute to all servers and return true if successful
    bool forward(py::object input);
    bool operator()(py::object input);

    // Get the underlying PyTorch model
    py::object get_pytorch_model() const;
    LeafTrainer* get_leaf_trainer() const;
    
    // Get the index of this model in the distributed_models array
    size_t get_index() const;

    // Delegate common PyTorch model methods
    py::object state_dict();
    py::object parameters();
    py::object named_parameters();
    py::object train();
    py::object eval();
    py::object to(py::object device);
    py::object cpu();
    py::object cuda();
    py::object getattr(const std::string& name);
    void setattr(const std::string& name, py::object value);
    bool hasattr(const std::string& name);
    // Get the computed loss
    float get_loss() const;
    
    // Set the loss value
    void set_loss(float loss_value);
    
    // Get the registered criterion
    std::shared_ptr<Criterion> get_criterion() const;
    
    // Set the criterion
    void set_criterion(std::shared_ptr<Criterion> criterion_ptr);
    
    std::vector<float> serialize_state() const;
    void deserialize_state(const std::vector<float>& state);
};

#endif // DISTRIBUTED_MODEL_H 