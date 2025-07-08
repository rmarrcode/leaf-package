#!/usr/bin/env python3
"""
Debug script to test server connection issues.
"""

import subprocess
import time
import sys

def run_command(cmd, description):
    """Run a command and return success status."""
    print(f"\n=== {description} ===")
    print(f"Command: {cmd}")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        print(f"Return code: {result.returncode}")
        if result.stdout:
            print(f"STDOUT: {result.stdout}")
        if result.stderr:
            print(f"STDERR: {result.stderr}")
        return result.returncode == 0
    except Exception as e:
        print(f"Exception: {e}")
        return False

def main():
    print("=== Server Connection Debug ===")
    
    # Test 1: Check if SSH tunnel is working
    print("\n1. Checking SSH tunnel...")
    tunnel_check = run_command("lsof -Pi :50052 -sTCP:LISTEN", "SSH tunnel check")
    
    # Test 2: Check if we can connect to the tunneled port
    print("\n2. Testing connection to tunneled port...")
    port_check = run_command("nc -z localhost 50052", "Port connectivity test")
    
    # Test 3: Check Docker container status on remote server
    print("\n3. Checking remote Docker container...")
    docker_check = run_command(
        "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -p 56442 root@174.93.255.152 'docker ps | grep leaf-grpc-server'",
        "Remote Docker container status"
    )
    
    # Test 4: Check if gRPC server is responding on remote server
    print("\n4. Testing gRPC server on remote server...")
    grpc_check = run_command(
        "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -p 56442 root@174.93.255.152 'nc -z localhost 50051'",
        "Remote gRPC server connectivity"
    )
    
    # Test 5: Check Docker logs
    print("\n5. Checking Docker container logs...")
    logs_check = run_command(
        "ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -p 56442 root@174.93.255.152 'docker logs leaf-grpc-server'",
        "Docker container logs"
    )
    
    # Summary
    print("\n=== Summary ===")
    print(f"SSH tunnel: {'✓' if tunnel_check else '✗'}")
    print(f"Port connectivity: {'✓' if port_check else '✗'}")
    print(f"Docker container: {'✓' if docker_check else '✗'}")
    print(f"Remote gRPC: {'✓' if grpc_check else '✗'}")
    print(f"Container logs: {'✓' if logs_check else '✗'}")
    
    if not tunnel_check:
        print("\nIssue: SSH tunnel is not established")
    elif not port_check:
        print("\nIssue: Cannot connect to tunneled port")
    elif not docker_check:
        print("\nIssue: Docker container is not running")
    elif not grpc_check:
        print("\nIssue: gRPC server is not responding on remote server")
    else:
        print("\nAll checks passed. The issue might be in the gRPC client code.")

if __name__ == "__main__":
    main() 