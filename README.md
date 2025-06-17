# Leaf

A Python package for managing server configurations and compute resources.

## Installation

```bash
pip install -e .
```

## Usage

```python
from leaf import create_config

# Create a new configuration (automatically discovers local resources)
config = create_config()

# Get localhost information
local_info = config.get_server_info("localhost")
print(f"Local server connected: {local_info['connected']}")
print(f"Is local: {local_info['is_local']}")

# List local compute resources
for resource in local_info['resources']:
    print(f"\nResource: {resource['name']}")
    print(f"Type: {resource['type']}")
    print("Properties:")
    for key, value in resource['properties'].items():
        print(f"  {key}: {value}")

# Add a remote server with SSH credentials
config.add_server(
    server_name="gpu-server-1",
    username="user1",
    hostname="gpu-server-1.example.com",
    port=22,  # Optional, defaults to 22
    key_path="/path/to/private/key"  # Optional
)

# Get server information
server_info = config.get_server_info("gpu-server-1")
print(f"Server connected: {server_info['connected']}")
print(f"Is local: {server_info['is_local']}")
print(f"Username: {server_info['username']}")
print(f"Hostname: {server_info['hostname']}")

# List available compute resources
for resource in server_info['resources']:
    print(f"\nResource: {resource['name']}")
    print(f"Type: {resource['type']}")
    print("Properties:")
    for key, value in resource['properties'].items():
        print(f"  {key}: {value}")

# Get list of all servers (including localhost)
servers = config.get_servers()
print("Available servers:", servers)

# Remove a server (cannot remove localhost)
config.remove_server("gpu-server-1")
```

## Dependences
Open MPI
GRPC
Abseil

## Features

- Automatic local resource discovery
- Server management with SSH credentials
- Automatic connection verification
- Resource discovery (currently supports NVIDIA GPUs)
- Detailed server and resource information
- Easy-to-use Python interface

## Requirements

- Python 3.6+
- pybind11
- SSH client
- NVIDIA drivers and nvidia-smi (for GPU discovery) 