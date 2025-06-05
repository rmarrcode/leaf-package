from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CppExtension
import os
import platform
import sys
import torch

def get_extension():
    # Get PyTorch include directory
    torch_include = os.path.join(os.path.dirname(torch.__file__), 'include')
    
    # Get MPI paths
    MPI_HOME = os.environ.get('MPI_HOME', '/usr/local/mpi')
    
    # Common settings for all platforms
    common_settings = {
        'name': 'leaf_trainer',
        'sources': ['leaf_trainer.cpp'],
        'include_dirs': [
            torch_include,
            os.path.join(MPI_HOME, 'include')
        ],
        'library_dirs': [
            os.path.join(MPI_HOME, 'lib')
        ],
        'libraries': ['mpi'],
        'extra_compile_args': {
            'cxx': ['-O3', '-std=c++17']
        }
    }
    
    # Check if we're on Apple Silicon (M1/M2)
    is_apple_silicon = platform.processor() == 'arm'
    
    # Check if CUDA is available
    has_cuda = torch.cuda.is_available() and not is_apple_silicon
    
    if has_cuda:
        # Use CUDA extension for NVIDIA GPUs
        from torch.utils.cpp_extension import CUDAExtension
        CUDA_HOME = os.environ.get('CUDA_HOME', '/usr/local/cuda')
        return CUDAExtension(
            **common_settings,
            include_dirs=common_settings['include_dirs'] + [
                os.path.join(CUDA_HOME, 'include')
            ],
            library_dirs=common_settings['library_dirs'] + [
                os.path.join(CUDA_HOME, 'lib64')
            ],
            libraries=common_settings['libraries'] + ['cudart'],
            extra_compile_args={
                'cxx': ['-O3', '-std=c++17'],
                'nvcc': ['-O3']
            }
        )
    else:
        # Use standard extension for CPU or Apple Silicon
        return CppExtension(**common_settings)

setup(
    name='leaf_trainer',
    version='0.1.0',
    packages=['leaf_trainer'],
    ext_modules=[get_extension()],
    cmdclass={
        'build_ext': BuildExtension
    },
    install_requires=[
        'torch>=1.7.0',
        'numpy>=1.19.0',
        'psutil>=5.8.0'  # For resource discovery
    ],
    python_requires='>=3.7',
) 