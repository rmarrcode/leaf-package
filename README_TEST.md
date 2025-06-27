# Testing the Hardcoded Gradient Computation Function

This document explains how to use the new `test_with_hardcoded_values()` function that was added to the `LeafTrainer` class.

## Overview

The `test_with_hardcoded_values()` function allows you to test the gradient computation functionality without requiring any PyTorch models or data loaders. It uses hardcoded input data and model state to verify that the core gradient computation logic works correctly.

## Usage

### In Python Script

```python
from leaf._core import LeafConfig, LeafTrainer

# Create configuration
config = LeafConfig()

# Create trainer
trainer = LeafTrainer(config)

# Test with hardcoded values
results = trainer.test_with_hardcoded_values()

print(f"Success: {results.get('success', False)}")
if results.get('success', False):
    print(f"Loss: {results.get('loss', 'N/A')}")
    print(f"Gradients size: {results.get('gradients_size', 'N/A')}")
    gradients = results.get('gradients', [])
    if gradients:
        print(f"First 5 gradients: {gradients[:5]}")
else:
    print(f"Error: {results.get('error', 'Unknown error')}")
```

### In Jupyter Notebook

Add this cell to your notebook:

```python
# Test the hardcoded gradient computation function
print("Testing hardcoded gradient computation...")
hardcoded_results = leaf_trainer.test_with_hardcoded_values()
print(f"\nHardcoded test results: {hardcoded_results}")
```

### Using the Test Script

Run the provided test script:

```bash
cd leaf-package
python test_hardcoded.py
```

## What the Function Does

The `test_with_hardcoded_values()` function:

1. **Creates hardcoded input data**: A simplified 4D tensor representing a small batch of images (2x2 patches for 3 channels)
2. **Creates hardcoded model state**: A simplified version of a ResNet model state including:
   - First convolutional layer weights (3x3x3 kernel)
   - Batch normalization weights and biases
   - Final fully connected layer weights and biases
3. **Creates dummy target data**: One-hot encoding for class 0
4. **Calls the gradient computation**: Uses the `ServerCommunicationServiceImpl` directly to compute gradients
5. **Returns results**: A dictionary containing success status, loss value, gradients size, and the actual gradients

## Expected Output

If successful, you should see output like:

```
=== Testing with hardcoded values ===
=== Testing get_gradients_from_server with hardcoded values ===
  Input data size: 12 elements
  Model state size: 49 elements
  Target data size: 10 elements
  Computing gradients locally...
  Local computation completed successfully!
  Loss: [some loss value]
  Gradients size: [some number] elements
  Sample gradients: [gradient values]
✓ Test with hardcoded values completed successfully!
```

## Troubleshooting

If you encounter errors:

1. **Import errors**: Make sure the `_core` module is compiled and the `src` directory is in your Python path
2. **Compilation errors**: Ensure all dependencies are installed and the C++ code compiles successfully
3. **Runtime errors**: Check that the `ServerCommunicationServiceImpl` is properly implemented

## Benefits

This test function is useful for:

- **Isolating issues**: Testing gradient computation without PyTorch model complexity
- **Debugging**: Identifying problems in the core computation logic
- **Development**: Verifying changes to the gradient computation code
- **CI/CD**: Automated testing of the gradient computation functionality

## Technical Details

The hardcoded values are based on real PyTorch tensor data but simplified for testing:

- **Input data**: 12 elements representing a 2x2x3 tensor (simplified from the full 4D tensor)
- **Model state**: 49 elements representing key layers of a ResNet model
- **Target data**: 10 elements representing one-hot encoding for CIFAR-10 classes

The function bypasses the PyTorch model interface and directly tests the C++ gradient computation implementation. 