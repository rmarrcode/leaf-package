#!/bin/bash

# Clean up any existing container
docker rm -f leaf-grpc-server 2>/dev/null || true

# Build Docker image (files are already here)
echo "Building Docker image..."
docker build -t leaf-grpc-server . 2>&1

# Check if build was successful
if [ $? -ne 0 ]; then
    echo "Docker build failed!"
    exit 1
fi

echo "Docker build successful, starting container..."

# Run Docker container (only bind to localhost for SSH tunneling)
docker run -d -p 127.0.0.1:50051:50051 --name leaf-grpc-server leaf-grpc-server 2>&1

# Check if container started successfully
if [ $? -ne 0 ]; then
    echo "Failed to start Docker container!"
    docker logs leaf-grpc-server 2>/dev/null || echo "No container logs available"
    exit 1
fi

# Wait for container to start
sleep 5

# Check if container is running
if ! docker ps | grep -q leaf-grpc-server; then
    echo "Container is not running!"
    docker logs leaf-grpc-server 2>/dev/null || echo "No container logs available"
    exit 1
fi

echo "Docker container started successfully!" 