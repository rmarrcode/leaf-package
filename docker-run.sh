#!/bin/bash

# Print Docker version for debugging
echo "Using Docker command: $(which docker)"
docker --version

# Clean up any existing container
echo "Cleaning up any existing container..."
docker rm -f leaf-grpc-server 2>/dev/null || true

# The files are already in the current directory (/tmp/leaf-build)
echo "Current directory: $(pwd)"
echo "Listing files in current directory:"
ls -la

# Build Docker image (files are already here)
echo "Building Docker image..."
docker build -t leaf-grpc-server . 2>&1 | tee docker-build.log

# Check if build was successful
if [ $? -ne 0 ]; then
    echo "Error: Docker build failed"
    echo "Build log contents:"
    cat docker-build.log
    exit 1
fi

# Run Docker container (only bind to localhost for SSH tunneling)
echo "Starting Docker container for SSH tunnel access..."
docker run -d -p 127.0.0.1:50051:50051 --name leaf-grpc-server leaf-grpc-server 2>&1 | tee docker-run.log

# Check if container started successfully
if [ $? -ne 0 ]; then
    echo "Error: Container failed to start"
    echo "Run log contents:"
    cat docker-run.log
    echo "Container logs:"
    docker logs leaf-grpc-server
    exit 1
fi

# Verify container is running
echo "Verifying container status..."
docker ps | grep leaf-grpc-server

# Wait for container to start
echo "Waiting for container to start..."
sleep 5

# Check if container is running
if ! docker ps | grep -q leaf-grpc-server; then
    echo "Error: Container failed to start"
    echo "Container logs:"
    docker logs leaf-grpc-server
    exit 1
fi

echo "Container is running. Checking logs..."
docker logs leaf-grpc-server

# Verify port is exposed locally
echo "Checking if port 50051 is exposed locally..."
docker port leaf-grpc-server
timeout 5 bash -c "</dev/tcp/localhost/50051" && echo "Port 50051 is accessible locally" || echo "Port 50051 is NOT accessible locally"

echo "Server is ready for SSH tunnel access!"
echo "From your local machine, run:"
echo "ssh -L 50051:localhost:50051 root@$(hostname -I | awk '{print $1}') -p 40236"
echo "Then connect to localhost:50051 from your local code" 