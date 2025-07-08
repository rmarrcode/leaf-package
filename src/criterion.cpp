#include "criterion.h"
#include "core.h"
#include "model.h"
#include <iostream>
#include <cstring>

namespace py = pybind11;

Criterion::Criterion(py::object criterion, LeafTrainer* trainer)
    : pytorch_criterion(criterion), leaf_trainer(trainer) {}

py::object Criterion::operator()(py::object outputs, py::object targets) {
    try {
        // Get all models from the trainer
        std::vector<Model*> models;
        
        // For now, we'll assume outputs is a list/tuple of model outputs
        // and we need to match them with corresponding target slices
        
        if (py::isinstance<py::list>(outputs) || py::isinstance<py::tuple>(outputs)) {
            py::list outputs_list = py::cast<py::list>(outputs);
            std::vector<py::object> target_slices = distribute_targets(targets, models);
            
            py::list total_losses;
            
            // Calculate loss for each model output with its corresponding target slice
            for (size_t i = 0; i < outputs_list.size(); ++i) {
                if (i < target_slices.size()) {
                    py::object model_loss = calculate_loss_for_model(outputs_list[i], target_slices[i], nullptr);
                    total_losses.append(model_loss);
                    stored_losses.push_back(model_loss);
                }
            }
            
            // Return the total loss (sum of all individual losses)
            if (!total_losses.empty()) {
                py::object total_loss = total_losses[0];
                for (size_t i = 1; i < total_losses.size(); ++i) {
                    total_loss = total_loss.attr("__add__")(total_losses[i]);
                }
                return total_loss;
            }
        } else {
            // Single output case - calculate loss directly
            py::object loss = pytorch_criterion(outputs, targets);
            stored_losses.push_back(loss);
            return loss;
        }
        
        return py::none();
    } catch (const std::exception& e) {
        std::cout << "Error in Criterion::operator(): " << e.what() << std::endl;
        return py::none();
    }
}

py::object Criterion::get_pytorch_criterion() const {
    return pytorch_criterion;
}

LeafTrainer* Criterion::get_leaf_trainer() const {
    return leaf_trainer;
}

const std::vector<py::object>& Criterion::get_stored_losses() const {
    return stored_losses;
}

void Criterion::clear_stored_losses() {
    stored_losses.clear();
}

py::object Criterion::calculate_loss_for_model(py::object model_outputs, py::object target_slice, Model* model) {
    try {
        // Calculate loss using the underlying PyTorch criterion
        py::object loss = pytorch_criterion(model_outputs, target_slice);
        
        // If we have a model pointer, store the loss in the model
        if (model != nullptr) {
            // Try to set the loss attribute on the model
            try {
                model->setattr("loss", loss);
            } catch (const std::exception& e) {
                std::cout << "Warning: Could not set loss attribute on model: " << e.what() << std::endl;
            }
        }
        
        return loss;
    } catch (const std::exception& e) {
        std::cout << "Error in calculate_loss_for_model: " << e.what() << std::endl;
        return py::none();
    }
}

std::vector<py::object> Criterion::distribute_targets(py::object targets, const std::vector<Model*>& models) {
    std::vector<py::object> target_slices;
    
    try {
        // Get the size of the targets tensor
        if (py::hasattr(targets, "size")) {
            py::object size_obj = targets.attr("size")();
            
            // For now, we'll implement a simple splitting strategy
            // This should be enhanced to match exactly how inputs were distributed
            
            if (py::isinstance<py::tuple>(size_obj)) {
                py::tuple size_tuple = py::cast<py::tuple>(size_obj);
                
                if (size_tuple.size() > 0) {
                    // Get the batch size (first dimension)
                    int batch_size = py::cast<int>(size_tuple[0]);
                    
                    // Simple equal splitting for now
                    // This should be enhanced to match the exact distribution used for inputs
                    int num_models = models.size();
                    if (num_models == 0) {
                        // If no models provided, just return the original targets
                        target_slices.push_back(targets);
                        return target_slices;
                    }
                    
                    int slice_size = batch_size / num_models;
                    int remainder = batch_size % num_models;
                    
                    int start_idx = 0;
                    for (int i = 0; i < num_models; ++i) {
                        int end_idx = start_idx + slice_size + (i < remainder ? 1 : 0);
                        
                        // Create a slice of the targets tensor
                        py::object target_slice;
                        if (py::hasattr(targets, "index_select")) {
                            // Use index_select for more efficient slicing
                            py::object indices = py::module::import("torch").attr("arange")(start_idx, end_idx);
                            target_slice = targets.attr("index_select")(0, indices);
                        } else {
                            // Fallback to basic slicing
                            target_slice = targets.attr("__getitem__")(py::slice(start_idx, end_idx, 1));
                        }
                        
                        target_slices.push_back(target_slice);
                        start_idx = end_idx;
                    }
                }
            }
        }
        
        // If we couldn't split properly, just return the original targets
        if (target_slices.empty()) {
            target_slices.push_back(targets);
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error in distribute_targets: " << e.what() << std::endl;
        target_slices.push_back(targets);
    }
    
    return target_slices;
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