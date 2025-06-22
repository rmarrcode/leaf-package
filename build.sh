#!/bin/bash

# Leaf Package Build Script
# This script completely cleans and rebuilds the leaf package from scratch

set -e  # Exit on any error

# Check if we're in the right directory
if [ ! -f "setup.py" ]; then
    echo "setup.py not found. Please run this script from the leaf-package directory."
    exit 1
fi

# Uninstall existing leaf package
pip uninstall leaf -y 2>/dev/null || true

# Remove build directories
rm -rf build/ leaf.egg-info/ dist/ *.egg-info/

# Remove compiled Python files
find . -name "*.pyc" -delete 2>/dev/null || true
find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true

# Remove compiled extensions
rm -rf leaf/_core*.so 2>/dev/null || true

# Clean pip cache
pip cache purge 2>/dev/null || true

# Verify clean state
if [ -d "build" ] || [ -d "leaf.egg-info" ] || [ -f "leaf/_core"*.so ]; then
    echo "Cleanup failed. Some build artifacts still exist."
    exit 1
fi

# Install build dependencies
pip install --upgrade pip setuptools wheel

# Build and install package
pip install -e . --no-cache-dir --force-reinstall

# Verify installation
if python -c "from leaf._core import LeafConfig; print('LeafConfig imported successfully')" 2>/dev/null; then
    echo "Package built and installed successfully!"
else
    echo "Package installation verification failed!"
    exit 1
fi

echo "Recent files in leaf/ directory:"
ls -la leaf/ 2>/dev/null || echo "No leaf directory found"

echo "Build completed successfully at $(date)"
echo "You can now use:"
echo "  from leaf._core import LeafConfig"
echo "  from leaf._core import LeafTrainer"
echo 