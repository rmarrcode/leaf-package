import functools
import os
from typing import Callable, Optional, Dict, List
from leaf_trainer import LeafTrainer, LeafConfig, ServerResources

def leaf(
    num_gpus: Optional[int] = None,
    gpu_ids: Optional[List[int]] = None,
    config: Optional[Dict] = None,
    batch_size_multiplier: int = 1,
    use_cuda: bool = True
):
    """
    Decorator for distributed training with Leaf.
    
    Args:
        num_gpus: Number of GPUs to use (if not using config)
        gpu_ids: List of GPU IDs to use (if not using config)
        config: Leaf configuration object (if not using num_gpus/gpu_ids)
        batch_size_multiplier: Multiply batch size by this factor
        use_cuda: Whether to use CUDA
    """
    def decorator(func: Callable):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            # Create configuration
            if config is not None:
                leaf_config = config
            else:
                leaf_config = LeafConfig()
                leaf_config.use_cuda = use_cuda
                leaf_config.batch_size_multiplier = batch_size_multiplier
                
                if gpu_ids is not None:
                    # Use specified GPU IDs
                    server = ServerResources()
                    server.hostname = "localhost"
                    server.gpu_ids = gpu_ids
                    leaf_config.servers["localhost"] = server
                elif num_gpus is not None:
                    # Use first N GPUs
                    server = ServerResources()
                    server.hostname = "localhost"
                    server.gpu_ids = list(range(num_gpus))
                    leaf_config.servers["localhost"] = server
            
            # Initialize trainer
            trainer = LeafTrainer(leaf_config)
            
            # Run training
            return trainer.run(func, *args, **kwargs)
        
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

config = create_leaf_config([
    {
        'hostname': 'server1',
        'gpu_ids': [0, 1],
        'memory_gb': 32,
        'cpu_cores': 8
    },
    {
        'hostname': 'server2',
        'gpu_ids': [0, 1, 2],
        'memory_gb': 64,
        'cpu_cores': 16
    }
])

save_config(config, 'leaf_config.json')

@leaf(config=load_config('leaf_config.json'))
def train(model, dataloader, criterion, optimizer):
    # Your training code here
    pass 