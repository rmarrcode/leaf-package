#!/bin/bash

# Leaf Package Build Script
# This script completely cleans and rebuilds the leaf package from scratch

set -e  # Exit on any error

echo "=== Leaf Package Build Script ==="
echo "Starting clean build at $(date)"
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the right directory
if [ ! -f "setup.py" ]; then
    print_error "setup.py not found. Please run this script from the leaf-package directory."
    exit 1
fi

print_status "Step 1: Uninstalling existing leaf package..."
pip uninstall leaf -y 2>/dev/null || print_warning "No existing leaf package found to uninstall"

print_status "Step 2: Cleaning build artifacts..."
# Remove build directories
rm -rf build/ leaf.egg-info/ dist/ *.egg-info/

# Remove compiled Python files
find . -name "*.pyc" -delete 2>/dev/null || true
find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true

# Remove compiled extensions
rm -rf leaf/_core*.so 2>/dev/null || true

print_status "Step 3: Cleaning pip cache..."
pip cache purge 2>/dev/null || print_warning "Could not purge pip cache"

print_status "Step 4: Verifying clean state..."
if [ -d "build" ] || [ -d "leaf.egg-info" ] || [ -f "leaf/_core"*.so ]; then
    print_error "Cleanup failed. Some build artifacts still exist."
    exit 1
fi

print_status "Step 5: Installing build dependencies..."
pip install --upgrade pip setuptools wheel

print_status "Step 6: Building and installing package..."
pip install -e . --no-cache-dir --force-reinstall

print_status "Step 7: Verifying installation..."
if python -c "from leaf._core import LeafConfig; print('LeafConfig imported successfully')" 2>/dev/null; then
    print_success "Package built and installed successfully!"
else
    print_error "Package installation verification failed!"
    exit 1
fi

print_status "Step 8: Checking file timestamps..."
echo "Recent files in leaf/ directory:"
ls -la leaf/ 2>/dev/null || echo "No leaf directory found"

echo
print_success "Build completed successfully at $(date)"
echo
echo "You can now use:"
echo "  from leaf._core import LeafConfig"
echo "  from leaf._core import LeafTrainer"
echo 