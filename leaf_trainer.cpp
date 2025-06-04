#include <torch/extension.h>
#include <torch/cuda.h>
#include <mpi.h>
#include <vector>
#include <memory>
#include <iostream>

// Structure to hold leaf training configuration
struct LeafConfig {
    int world_size;
    int rank;
    std::vector<int> gpu_ids;
    bool use_cuda;
};

// Class to manage leaf training
class LeafTrainer {
private:
    LeafConfig config;
    std::vector<torch::Device> devices;
    
public:
    LeafTrainer(const LeafConfig& cfg) : config(cfg) {
        // Initialize MPI
        int provided;
        MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided);
        MPI_Comm_size(MPI_COMM_WORLD, &config.world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &config.rank);
        
        // Setup CUDA devices
        if (config.use_cuda) {
            for (int gpu_id : config.gpu_ids) {
                devices.push_back(torch::Device(torch::kCUDA, gpu_id));
            }
        } else {
            devices.push_back(torch::Device(torch::kCPU));
        }
    }
    
    // Split input data across GPUs
    std::vector<torch::Tensor> split_input(const torch::Tensor& input) {
        std::vector<torch::Tensor> split_tensors;
        int num_splits = devices.size();
        int split_size = input.size(0) / num_splits;
        
        for (int i = 0; i < num_splits; ++i) {
            int start_idx = i * split_size;
            int end_idx = (i == num_splits - 1) ? input.size(0) : (i + 1) * split_size;
            split_tensors.push_back(input.slice(0, start_idx, end_idx).to(devices[i]));
        }
        
        return split_tensors;
    }
    
    // Average gradients across all processes
    void average_gradients(torch::nn::Module& model) {
        for (auto& param : model.parameters()) {
            if (param.grad().defined()) {
                torch::Tensor grad = param.grad();
                MPI_Allreduce(MPI_IN_PLACE, grad.data_ptr(), grad.numel(),
                            MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD);
                grad.div_(config.world_size);
            }
        }
    }
    
    // Forward pass with distributed computation
    torch::Tensor forward(torch::nn::Module& model, const torch::Tensor& input) {
        auto split_inputs = split_input(input);
        std::vector<torch::Tensor> outputs;
        
        for (size_t i = 0; i < split_inputs.size(); ++i) {
            model.to(devices[i]);
            outputs.push_back(model.forward(split_inputs[i]));
        }
        
        // Gather results
        return torch::cat(outputs, 0);
    }
    
    // Backward pass with gradient synchronization
    void backward(torch::nn::Module& model, const torch::Tensor& loss) {
        loss.backward();
        average_gradients(model);
    }
    
    ~LeafTrainer() {
        MPI_Finalize();
    }
};

// Python bindings
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    py::class_<LeafConfig>(m, "LeafConfig")
        .def(py::init<>())
        .def_readwrite("world_size", &LeafConfig::world_size)
        .def_readwrite("rank", &LeafConfig::rank)
        .def_readwrite("gpu_ids", &LeafConfig::gpu_ids)
        .def_readwrite("use_cuda", &LeafConfig::use_cuda);
        
    py::class_<LeafTrainer>(m, "LeafTrainer")
        .def(py::init<const LeafConfig&>())
        .def("split_input", &LeafTrainer::split_input)
        .def("average_gradients", &LeafTrainer::average_gradients)
        .def("forward", &LeafTrainer::forward)
        .def("backward", &LeafTrainer::backward);
} 