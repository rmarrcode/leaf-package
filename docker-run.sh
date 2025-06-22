#!/bin/bash

# Clean up any existing container
docker rm -f leaf-grpc-server 2>/dev/null || true

# Build Docker image (files are already here)
docker build -t leaf-grpc-server . > /dev/null 2>&1

# Check if build was successful
if [ $? -ne 0 ]; then
    cat docker-build.log
    exit 1
fi

# Run Docker container (only bind to localhost for SSH tunneling)
docker run -d -p 127.0.0.1:50051:50051 --name leaf-grpc-server leaf-grpc-server > /dev/null 2>&1

# Check if container started successfully
if [ $? -ne 0 ]; then
    cat docker-run.log
    docker logs leaf-grpc-server
    exit 1
fi

# Wait for container to start
sleep 5

# Check if container is running
if ! docker ps | grep -q leaf-grpc-server; then
    docker logs leaf-grpc-server
    exit 1
fi 