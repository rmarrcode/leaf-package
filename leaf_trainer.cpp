#include <torch/extension.h>
#include <torch/script.h>
#include <mpi.h>
#include <vector>
#include <memory>
#include <iostream>
#include <string>
#include <map>

// Check for CUDA availability
#ifdef TORCH_CUDA_AVAILABLE
#include <torch/cuda.h>
#define LEAF_CUDA_AVAILABLE 1
#else
#define LEAF_CUDA_AVAILABLE 0
#endif

// Structure to hold server resource information
struct ServerResources {
    std::vector<int> gpu_ids;
    float memory_gb;
    int cpu_cores;
    std::string hostname;
};

// Structure to hold leaf training configuration
struct LeafConfig {
    int world_size;
    int rank;
    std::map<std::string, ServerResources> servers;
    bool use_cuda;
    int batch_size_multiplier;
    
    // Helper methods
    std::vector<int> get_all_gpu_ids() const {
        std::vector<int> all_gpus;
        for (const auto& server : servers) {
            all_gpus.insert(all_gpus.end(), 
                          server.second.gpu_ids.begin(), 
                          server.second.gpu_ids.end());
        }
        return all_gpus;
    }
    
    int get_total_gpus() const {
        int total = 0;
        for (const auto& server : servers) {
            total += server.second.gpu_ids.size();
        }
        return total;
    }
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
        
        // Setup devices
        if (config.use_cuda && LEAF_CUDA_AVAILABLE) {
            auto all_gpus = config.get_all_gpu_ids();
            for (int gpu_id : all_gpus) {
                devices.push_back(torch::Device(torch::kCUDA, gpu_id));
            }
        } else {
            devices.push_back(torch::Device(torch::kCPU));
        }
    }
    
    // Split input data across devices
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
    
    // Forward pass with distributed computation using a Python model object
    torch::Tensor forward(py::object py_model, const torch::Tensor& input) {
        auto split_inputs = split_input(input);
        std::vector<torch::Tensor> outputs;

        for (size_t i = 0; i < split_inputs.size(); ++i) {
            // Move model to the corresponding device if the Python object supports .to()
            try {
                py_model.attr("to")(devices[i].str());
            } catch (const std::exception& e) {
                // If .to() isn't available, ignore
            }
            // Call the Python model (invokes __call__/forward)
            py::object out_obj = py_model(split_inputs[i]);
            outputs.push_back(out_obj.cast<torch::Tensor>());
        }

        // Gather results
        return torch::cat(outputs, 0);
    }
    
    // Backward pass with gradient synchronization
    void backward(torch::nn::Module& model, const torch::Tensor& loss) {
        loss.backward();
        average_gradients(model);
    }
    
    // Generate MPI hostfile
    void generate_hostfile(const std::string& filename) {
        std::ofstream file(filename);
        for (const auto& server : config.servers) {
            file << server.second.hostname << " slots=" 
                 << server.second.gpu_ids.size() << "\n";
        }
    }
    
    ~LeafTrainer() {
        MPI_Finalize();
    }
};

// Python bindings
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    py::class_<ServerResources>(m, "ServerResources")
        .def(py::init<>())
        .def_readwrite("gpu_ids", &ServerResources::gpu_ids)
        .def_readwrite("memory_gb", &ServerResources::memory_gb)
        .def_readwrite("cpu_cores", &ServerResources::cpu_cores)
        .def_readwrite("hostname", &ServerResources::hostname);
        
    py::class_<LeafConfig>(m, "LeafConfig")
        .def(py::init<>())
        .def_readwrite("world_size", &LeafConfig::world_size)
        .def_readwrite("rank", &LeafConfig::rank)
        .def_readwrite("servers", &LeafConfig::servers)
        .def_readwrite("use_cuda", &LeafConfig::use_cuda)
        .def_readwrite("batch_size_multiplier", &LeafConfig::batch_size_multiplier)
        .def("get_all_gpu_ids", &LeafConfig::get_all_gpu_ids)
        .def("get_total_gpus", &LeafConfig::get_total_gpus);
        
    py::class_<LeafTrainer>(m, "LeafTrainer")
        .def(py::init<const LeafConfig&>())
        .def("split_input", &LeafTrainer::split_input)
        .def("average_gradients", &LeafTrainer::average_gradients)
        .def("forward", &LeafTrainer::forward, py::arg("model"), py::arg("input"))
        .def("backward", &LeafTrainer::backward)
        .def("generate_hostfile", &LeafTrainer::generate_hostfile);
} 