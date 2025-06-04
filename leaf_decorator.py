import functools
import torch
import torch.nn as nn
from typing import List, Optional, Callable, Any
from .leaf_trainer import LeafTrainer

def leaf(
    num_gpus: Optional[int] = None,
    gpu_ids: Optional[List[int]] = None,
    batch_size_multiplier: int = 1
):
    """
    Decorator to make any training function run across multiple GPUs/servers using Leaf.
    
    Args:
        num_gpus: Number of GPUs to use (uses first N GPUs)
        gpu_ids: Specific GPU IDs to use
        batch_size_multiplier: Multiply the batch size by this factor for distributed training
    
    Example:
        @leaf(num_gpus=2)
        def train(model, dataloader, criterion, optimizer):
            # Your existing training code here
            pass
    """
    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            # Extract model from args or kwargs
            model = next((arg for arg in args if isinstance(arg, nn.Module)), 
                        kwargs.get('model'))
            if model is None:
                raise ValueError("No model found in function arguments")
            
            # Create leaf trainer
            trainer = LeafTrainer(model, gpu_ids=gpu_ids, num_gpus=num_gpus)
            
            # Adjust batch size if dataloader is provided
            if 'dataloader' in kwargs:
                kwargs['dataloader'] = trainer.adjust_dataloader(kwargs['dataloader'], batch_size_multiplier)
            
            # Add trainer to kwargs
            kwargs['trainer'] = trainer
            
            # Call the original function
            return func(*args, **kwargs)
        return wrapper
    return decorator

# Helper function to convert existing training code
def convert_to_leaf(
    train_func: Callable,
    model: nn.Module,
    num_gpus: Optional[int] = None,
    gpu_ids: Optional[List[int]] = None
) -> Callable:
    """
    Convert an existing training function to use Leaf distributed training.
    
    Args:
        train_func: The original training function
        model: The model to train
        num_gpus: Number of GPUs to use
        gpu_ids: Specific GPU IDs to use
    
    Returns:
        A new function that uses Leaf distributed training
    """
    @leaf(num_gpus=num_gpus, gpu_ids=gpu_ids)
    def leaf_train_func(*args, **kwargs):
        return train_func(*args, **kwargs)
    
    return leaf_train_func 