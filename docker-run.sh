#!/bin/bash

# Print Docker version for debugging
echo "Using Docker command: $(which docker)"
docker --version

# Clean up any existing container
echo "Cleaning up any existing container..."
docker rm -f leaf-grpc-server 2>/dev/null || true

# Create a build directory
BUILD_DIR="/tmp/leaf-build"
echo "Creating build directory at $BUILD_DIR..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy files to build directory
echo "Setting up build directory..."
echo "Current directory: $(pwd)"
echo "Listing files in current directory:"
ls -la

# Copy files with verbose output
echo "Copying Dockerfile..."
cp -v Dockerfile "$BUILD_DIR/"
echo "Copying source files..."
cp -v src/leaf/server_test.cpp "$BUILD_DIR/"
cp -v src/leaf/server_test.proto "$BUILD_DIR/"

# Verify files were copied
echo "Verifying files in build directory:"
ls -la "$BUILD_DIR"

# Change to build directory
echo "Changing to build directory..."
cd "$BUILD_DIR"

# Build Docker image
echo "Building Docker image..."
docker build -t leaf-grpc-server . 2>&1 | tee docker-build.log

# Check if build was successful
if [ $? -ne 0 ]; then
    echo "Error: Docker build failed"
    echo "Build log contents:"
    cat docker-build.log
    exit 1
fi

# Run Docker container
echo "Starting Docker container..."
docker run -d -p 50051:50051 --name leaf-grpc-server leaf-grpc-server 2>&1 | tee docker-run.log

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

echo "Server should be ready to accept connections on port 50051" 