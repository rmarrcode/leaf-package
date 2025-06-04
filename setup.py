from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CUDAExtension
import os

# Get CUDA path
CUDA_HOME = os.environ.get('CUDA_HOME', '/usr/local/cuda')

setup(
    name='leaf_trainer',
    ext_modules=[
        CUDAExtension(
            name='leaf_trainer',
            sources=['leaf_trainer.cpp'],
            extra_compile_args={
                'cxx': ['-O3'],
                'nvcc': ['-O3']
            },
            include_dirs=[
                '/usr/local/include',
                f'{CUDA_HOME}/include',
                '/usr/include/openmpi-x86_64'  # Adjust based on your MPI installation
            ],
            libraries=['mpi', 'mpi_cxx'],
            library_dirs=[
                '/usr/local/lib',
                f'{CUDA_HOME}/lib64',
                '/usr/lib64/openmpi/lib'  # Adjust based on your MPI installation
            ]
        )
    ],
    cmdclass={
        'build_ext': BuildExtension
    }
) 