# Distributed Training Framework

This framework provides a C++/Python hybrid solution for distributed training across multiple GPUs and servers. It uses MPI for inter-process communication and PyTorch for deep learning operations.

## Requirements

- PyTorch
- CUDA
- OpenMPI
- C++ compiler with C++11 support
- Python 3.6+

## Installation

1. Install the required dependencies:
```bash
pip install torch torchvision
```

2. Install OpenMPI (Ubuntu/Debian):
```bash
sudo apt-get install openmpi-bin libopenmpi-dev
```

3. Build the C++ extension:
```bash
python setup.py build_ext --inplace
```

## Usage

Here's a simple example of how to use the distributed trainer:

```python
import torch
import torch.nn as nn
from distributed_trainer import create_distributed_trainer

# Create your model
model = YourModel()

# Create the distributed trainer
trainer = create_distributed_trainer(model, num_gpus=2)  # Use 2 GPUs

# Training loop
for epoch in range(num_epochs):
    for batch in dataloader:
        inputs, targets = batch
        
        # Perform training step
        loss = trainer.train_step(
            inputs=inputs,
            targets=targets,
            criterion=nn.CrossEntropyLoss(),
            optimizer=torch.optim.Adam(model.parameters())
        )
        
        print(f'Loss: {loss:.4f}')
```

## Running Distributed Training

To run training across multiple machines:

1. Create a hostfile (e.g., `hosts.txt`):
```
machine1 slots=2
machine2 slots=2
```

2. Launch the training script:
```bash
mpirun --hostfile hosts.txt -np 4 python train.py
```

## Features

- Automatic data splitting across GPUs
- Gradient synchronization using MPI
- Support for multiple GPUs per machine
- Support for multiple machines
- Easy-to-use Python interface
- Efficient C++ implementation

## Performance Considerations

- The framework automatically handles data distribution and gradient synchronization
- For best performance, ensure your batch size is divisible by the number of GPUs
- Use appropriate learning rates for distributed training (typically scaled by the number of GPUs)
- Consider using gradient accumulation for very large models

## Limitations

- Currently supports only synchronous training
- Requires homogeneous GPU setup for best performance
- Memory usage scales with the number of GPUs used 