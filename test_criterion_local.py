#!/usr/bin/env python3
"""
Local test for the Criterion class functionality.
This tests the criterion without requiring server connections.
"""

import torch
import torch.nn as nn
from leaf._core import LeafConfig, LeafTrainer

def test_criterion_local():
    """Test the Criterion class with local configuration only."""
    
    print("=== Testing Criterion Class (Local Only) ===")
    
    # Create a simple configuration (local only)
    config = LeafConfig()
    
    # Create the trainer
    trainer = LeafTrainer(config)
    
    # Create a simple model
    model = nn.Sequential(
        nn.Linear(10, 5),
        nn.ReLU(),
        nn.Linear(5, 2)
    )
    
    # Create a criterion
    criterion = nn.CrossEntropyLoss()
    
    # Register the model and criterion
    print("Registering model...")
    leaf_model = trainer.register_model(model)
    
    print("Registering criterion...")
    leaf_criterion = trainer.register_criterion(criterion)
    
    # Create some test data
    batch_size = 8
    inputs = torch.randn(batch_size, 10)
    targets = torch.randint(0, 2, (batch_size,))
    
    print(f"Input shape: {inputs.shape}")
    print(f"Target shape: {targets.shape}")
    
    # Forward pass through the model
    print("Running forward pass...")
    outputs = leaf_model(inputs)
    
    print(f"Output shape: {outputs.shape}")
    
    # Calculate loss using the criterion
    print("Calculating loss...")
    loss = leaf_criterion(outputs, targets)
    
    print(f"Loss: {loss}")
    print(f"Loss type: {type(loss)}")
    
    # Check if loss is stored in the model
    model_loss = leaf_model.get_loss()
    print(f"Model loss: {model_loss}")
    
    # Test with multiple outputs (simulating distributed scenario)
    print("\n=== Testing with multiple outputs ===")
    
    # Create multiple model outputs (simulating distributed computation)
    outputs_list = [outputs[:4], outputs[4:]]  # Split the batch
    targets_list = [targets[:4], targets[4:]]  # Split the targets accordingly
    
    print(f"Outputs list length: {len(outputs_list)}")
    print(f"Targets list length: {len(targets_list)}")
    
    # Calculate loss for each part
    total_loss = 0
    for i, (out, targ) in enumerate(zip(outputs_list, targets_list)):
        part_loss = leaf_criterion(out, targ)
        print(f"Part {i} loss: {part_loss}")
        total_loss += part_loss
    
    print(f"Total loss: {total_loss}")
    
    # Test criterion attributes
    print("\n=== Testing criterion attributes ===")
    print(f"Criterion type: {type(leaf_criterion)}")
    print(f"Underlying PyTorch criterion: {leaf_criterion.get_pytorch_criterion()}")
    print(f"Stored losses: {leaf_criterion.get_stored_losses()}")
    
    # Test model loss storage
    print("\n=== Testing model loss storage ===")
    print(f"Model loss before: {leaf_model.get_loss()}")
    
    # Clear and set new loss
    leaf_model.clear_loss()
    print(f"Model loss after clear: {leaf_model.get_loss()}")
    
    leaf_model.set_loss(loss)
    print(f"Model loss after set: {leaf_model.get_loss()}")
    
    # Test with different loss functions
    print("\n=== Testing different loss functions ===")
    
    # MSELoss
    mse_criterion = nn.MSELoss()
    leaf_mse_criterion = trainer.register_criterion(mse_criterion)
    
    # Create regression targets (for demonstration)
    regression_targets = torch.randn_like(outputs)
    
    mse_loss = leaf_mse_criterion(outputs, regression_targets)
    print(f"MSE Loss: {mse_loss}")
    
    # L1Loss
    l1_criterion = nn.L1Loss()
    leaf_l1_criterion = trainer.register_criterion(l1_criterion)
    
    l1_loss = leaf_l1_criterion(outputs, regression_targets)
    print(f"L1 Loss: {l1_loss}")
    
    print("\n=== Criterion test completed successfully! ===")
    return True

if __name__ == "__main__":
    try:
        test_criterion_local()
    except Exception as e:
        print(f"Test failed with error: {e}")
        import traceback
        traceback.print_exc() 