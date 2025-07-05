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
#include "distributed_model.h"
#include "core.h"

namespace py = pybind11;

PYBIND11_MODULE(_core, m) {
    std::cout << "Initializing _core module..." << std::endl;
    
    py::class_<Model, std::shared_ptr<Model>>(m, "Model")
        .def(py::init<py::object, LeafTrainer*>(),
             py::arg("model"), py::arg("trainer"))
        .def("forward", &Model::forward)
        .def("__call__", &Model::operator())
        .def("get_pytorch_model", &Model::get_pytorch_model)
        .def("get_leaf_trainer", &Model::get_leaf_trainer)
        .def("state_dict", &Model::state_dict)
        .def("parameters", &Model::parameters)
        .def("named_parameters", &Model::named_parameters)
        .def("train", &Model::train)
        .def("eval", &Model::eval)
        .def("to", &Model::to)
        .def("cpu", &Model::cpu)
        .def("cuda", &Model::cuda)
        .def("getattr", &Model::getattr)
        .def("setattr", &Model::setattr)
        .def("hasattr", &Model::hasattr)
        .def("serialize_state", &Model::serialize_state)
        .def("deserialize_state", &Model::deserialize_state);

    py::class_<DistributedModel, std::shared_ptr<DistributedModel>>(m, "DistributedModel")
        .def(py::init<std::shared_ptr<Model>, LeafTrainer*>(),
             py::arg("model"), py::arg("trainer"))
        .def("forward", &DistributedModel::forward)
        .def("__call__", &DistributedModel::operator())
        .def("get_pytorch_model", &DistributedModel::get_pytorch_model)
        .def("get_leaf_trainer", &DistributedModel::get_leaf_trainer)
        .def("state_dict", &DistributedModel::state_dict)
        .def("parameters", &DistributedModel::parameters)
        .def("named_parameters", &DistributedModel::named_parameters)
        .def("train", &DistributedModel::train)
        .def("eval", &DistributedModel::eval)
        .def("to", &DistributedModel::to)
        .def("cpu", &DistributedModel::cpu)
        .def("cuda", &DistributedModel::cuda)
        .def("getattr", &DistributedModel::getattr)
        .def("setattr", &DistributedModel::setattr)
        .def("hasattr", &DistributedModel::hasattr)
        .def("serialize_state", &DistributedModel::serialize_state)
        .def("deserialize_state", &DistributedModel::deserialize_state);

    py::class_<LeafConfig>(m, "LeafConfig")
        .def(py::init<>())
        .def("add_server", &LeafConfig::add_server,
             py::arg("server_name"),
             py::arg("username"),
             py::arg("hostname"),
             py::arg("port") = 22,
             py::arg("key_path") = "")
        .def("get_servers", &LeafConfig::get_servers)
        .def("get_server_info", &LeafConfig::get_server_info)
        .def("remove_server", &LeafConfig::remove_server)
        .def("print_all_resources", &LeafConfig::print_all_resources)
        .def("get_server_connection_info", &LeafConfig::get_server_connection_info);

    py::class_<LeafTrainer>(m, "LeafTrainer")
        .def(py::init<const LeafConfig&>())
        .def("train", &LeafTrainer::train,
             py::arg("model"),
             py::arg("optimizer"),
             py::arg("train_loader"),
             py::arg("epochs"),
             py::arg("criterion") = py::none())
        .def("test_with_hardcoded_values", &LeafTrainer::test_with_hardcoded_values)
        .def("register_model", &LeafTrainer::register_model, py::arg("model"))
        .def("cleanup_models", &LeafTrainer::cleanup_models)
        .def("get_model_count", &LeafTrainer::get_model_count)
        .def("get_server_names", &LeafTrainer::get_server_names)
        .def("get_server_info", &LeafTrainer::get_server_info)
        .def("store_model_weights_on_server", &LeafTrainer::store_model_weights_on_server);
        
    std::cout << "_core module initialization complete!" << std::endl;
} 