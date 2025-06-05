from typing import List, Dict, Optional
from dataclasses import dataclass
import json
import os
import platform
import torch
import psutil
import subprocess

@dataclass
class ServerConfig:
    hostname: str
    gpu_ids: List[int]
    memory_gb: float
    cpu_cores: int

def discover_local_resources() -> Dict:
    """
    Discover available computational resources on the local machine.
    Returns a dictionary with hardware information.
    """
    resources = {
        'hostname': platform.node(),
        'gpu_ids': [],
        'memory_gb': psutil.virtual_memory().total / (1024**3),  # Convert to GB
        'cpu_cores': psutil.cpu_count(logical=True)
    }
    
    # Check for CUDA availability
    if torch.cuda.is_available():
        resources['gpu_ids'] = list(range(torch.cuda.device_count()))
    
    return resources

def create_leaf_config(
    servers: Optional[List[Dict]] = None,
    batch_size_multiplier: int = 1,
    use_cuda: bool = True,
    auto_discover: bool = True
) -> Dict:
    """
    Create a Leaf configuration from server specifications or auto-discover resources.
    
    Args:
        servers: List of server configurations (optional if auto_discover=True)
        batch_size_multiplier: Multiply batch size by this factor
        use_cuda: Whether to use CUDA
        auto_discover: Whether to automatically discover local resources
    
    Example:
        # Auto-discover local resources
        config = create_leaf_config(auto_discover=True)
        
        # Or specify servers manually
        config = create_leaf_config([
            {
                'hostname': 'server1',
                'gpu_ids': [0, 1],
                'memory_gb': 32,
                'cpu_cores': 8
            }
        ])
    """
    from leaf_trainer import LeafConfig, ServerResources
    
    # Create C++ configuration
    config = LeafConfig()
    config.use_cuda = use_cuda
    config.batch_size_multiplier = batch_size_multiplier
    
    if auto_discover and (servers is None or len(servers) == 0):
        # Auto-discover local resources
        local_resources = discover_local_resources()
        server = ServerResources()
        server.hostname = local_resources['hostname']
        server.gpu_ids = local_resources['gpu_ids']
        server.memory_gb = local_resources['memory_gb']
        server.cpu_cores = local_resources['cpu_cores']
        config.servers[server.hostname] = server
    else:
        # Use provided server configurations
        for server_data in servers:
            server = ServerResources()
            server.hostname = server_data['hostname']
            server.gpu_ids = server_data['gpu_ids']
            server.memory_gb = server_data['memory_gb']
            server.cpu_cores = server_data['cpu_cores']
            config.servers[server.hostname] = server
    
    return config

def save_config(config: Dict, path: str):
    """Save configuration to a JSON file."""
    config_data = {
        'servers': [
            {
                'hostname': server.hostname,
                'gpu_ids': server.gpu_ids,
                'memory_gb': server.memory_gb,
                'cpu_cores': server.cpu_cores
            }
            for server in config.servers.values()
        ],
        'batch_size_multiplier': config.batch_size_multiplier,
        'use_cuda': config.use_cuda
    }
    
    with open(path, 'w') as f:
        json.dump(config_data, f, indent=2)

def load_config(path: str) -> Dict:
    """Load configuration from a JSON file."""
    with open(path, 'r') as f:
        config_data = json.load(f)
    
    return create_leaf_config(
        servers=config_data['servers'],
        batch_size_multiplier=config_data.get('batch_size_multiplier', 1),
        use_cuda=config_data.get('use_cuda', True)
    )

def get_optimal_batch_size(config: Dict) -> int:
    """
    Calculate optimal batch size based on available resources.
    """
    total_gpus = sum(len(server.gpu_ids) for server in config.servers.values())
    if total_gpus > 0:
        # Use GPU memory to determine batch size
        gpu_memory = torch.cuda.get_device_properties(0).total_memory / (1024**3)  # GB
        return int(gpu_memory * 1024)  # Rough estimate: 1GB per 1024 samples
    else:
        # Use CPU memory to determine batch size
        cpu_memory = psutil.virtual_memory().total / (1024**3)  # GB
        return int(cpu_memory * 512)  # Rough estimate: 1GB per 512 samples 