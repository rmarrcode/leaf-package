#ifndef MODEL_H
#define MODEL_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <string>
#include <memory>
#include <vector>

namespace py = pybind11;

// Forward declaration
class LeafTrainer;

class Model {
private:
    py::object pytorch_model;
    LeafTrainer* leaf_trainer;

public:
    Model(py::object model, LeafTrainer* trainer);
    
    // Forward pass method that mimics the original model's behavior
    py::object forward(py::object input);
    
    // Call operator to make it behave like a PyTorch model
    py::object operator()(py::object input);
    
    // Get the underlying PyTorch model
    py::object get_pytorch_model() const;
    
    // Get the LeafTrainer pointer
    LeafTrainer* get_leaf_trainer() const;
    
    // Delegate common PyTorch model methods to the underlying model
    py::object state_dict();
    py::object parameters();
    py::object named_parameters();
    py::object train();
    py::object eval();
    py::object to(py::object device);
    py::object cpu();
    py::object cuda();
    
    // Get model attributes
    py::object getattr(const std::string& name);
    
    // Set model attributes
    void setattr(const std::string& name, py::object value);
    
    // Check if model has an attribute
    bool hasattr(const std::string& name);
    
    // Serialize model state to vector of floats
    std::vector<float> serialize_state() const;
    
    // Deserialize model state from vector of floats
    void deserialize_state(const std::vector<float>& state);
};

#endif // MODEL_H 