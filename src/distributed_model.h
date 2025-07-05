#ifndef DISTRIBUTED_MODEL_H
#define DISTRIBUTED_MODEL_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <string>
#include <memory>
#include <vector>

namespace py = pybind11;

// Forward declarations
class LeafTrainer;
class Model;

class DistributedModel {
private:
    std::shared_ptr<Model> model;
    LeafTrainer* leaf_trainer;

public:
    DistributedModel(std::shared_ptr<Model> model, LeafTrainer* trainer);

    // Forward pass: split input and distribute to all servers
    py::object forward(py::object input);
    py::object operator()(py::object input);

    // Get the underlying PyTorch model
    py::object get_pytorch_model() const;
    LeafTrainer* get_leaf_trainer() const;

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
    std::vector<float> serialize_state() const;
    void deserialize_state(const std::vector<float>& state);
};

#endif // DISTRIBUTED_MODEL_H 