import functools
import os
from typing import Callable, Optional, Dict, List, Any
import torch
import torch.nn as nn
import inspect

# Import the C++ extension
try:
    from . import _core
except ImportError:
    raise ImportError("Could not import _core. Make sure the C++ extension is built.")

def leaf(*args, **kwargs):
    """
    Decorator for distributed training with Leaf.
    
    Args:
        num_gpus (int): Number of GPUs to use
        gpu_ids (list): List of GPU IDs to use
        use_mpi (bool): Whether to use MPI for distributed training
        batch_size_multiplier (int): Multiplier for batch size
        servers (dict): Dictionary of server configurations
    """
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*func_args, **func_kwargs):
            # Create configuration
            config = _core.LeafConfig()
            
            # Set GPU configuration
            if 'num_gpus' in kwargs:
                config.use_cuda = True
                config.gpu_ids = list(range(kwargs['num_gpus']))
            elif 'gpu_ids' in kwargs:
                config.use_cuda = True
                config.gpu_ids = kwargs['gpu_ids']
            else:
                config.use_cuda = torch.cuda.is_available()
                if config.use_cuda:
                    config.gpu_ids = list(range(torch.cuda.device_count()))
            
            # Set MPI configuration
            config.use_mpi = kwargs.get('use_mpi', False)
            
            # Set batch size multiplier
            config.batch_size_multiplier = kwargs.get('batch_size_multiplier', 1)
            
            # Add server configurations if provided
            if 'servers' in kwargs:
                for server_name, server_config in kwargs['servers'].items():
                    config.add_server(
                        server_name,
                        server_config.get('username', ''),
                        server_config.get('hostname', ''),
                        server_config.get('port', 22),
                        server_config.get('key_path', '')
                    )
            
            # Create trainer
            trainer = _core.LeafTrainer(config)
            
            # Create partial function with all arguments
            partial_func = functools.partial(func, *func_args, **func_kwargs)
            
            # Run the function
            return trainer.run(partial_func)
            
        return wrapper
    
    # Handle both @leaf and @leaf() cases
    if len(args) == 1 and callable(args[0]):
        return decorator(args[0])
    return decorator

def convert_to_leaf(func):
    """
    Convert an existing training function to use Leaf.
    This is a convenience function that applies the @leaf decorator.
    """
    return leaf()(func)

# Example usage:
# @leaf(
#     num_gpus=2,
#     use_mpi=True,
#     servers={
#         'server1': {
#             'username': 'user',
#             'hostname': 'server1.example.com',
#             'port': 22,
#             'key_path': '/path/to/key'
#         }
#     }
# )
# def train(model, dataloader, criterion, optimizer):
#     for batch in dataloader:
#         optimizer.zero_grad()
#         outputs = model(batch)
#         loss = criterion(outputs, batch.targets)
#         loss.backward()
#         optimizer.step() 