from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import setuptools
import os

__version__ = '0.0.1'

# As of Python 3.6, CCompiler has a `has_flag` method.
# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile
    with tempfile.NamedTemporaryFile('w', suffix='.cpp') as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        try:
            compiler.compile([f.name], extra_postargs=[flagname])
        except setuptools.distutils.errors.CompileError:
            return False
    return True

def cpp_flag(compiler):
    """Return the -std=c++[11/14/17] compiler flag.
    The newer version is prefered over c++11 (when it is available).
    """
    flags = ['-std=c++17', '-std=c++14', '-std=c++11']

    for flag in flags:
        if has_flag(compiler, flag):
            return flag

    raise RuntimeError('Unsupported compiler -- at least C++11 support '
                      'is needed!')

class get_pybind_include(object):
    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)

class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    c_opts = {
        'msvc': ['/EHsc'],
        'unix': [],
    }
    l_opts = {
        'msvc': [],
        'unix': [],
    }

    if sys.platform == 'darwin':
        darwin_opts = ['-stdlib=libc++', '-mmacosx-version-min=10.7']
        c_opts['unix'] += darwin_opts
        l_opts['unix'] += darwin_opts

    def build_extensions(self):
        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        link_opts = self.l_opts.get(ct, [])
        if ct == 'unix':
            opts.append('-DVERSION_INFO="%s"' % self.distribution.get_version())
            opts.append(cpp_flag(self.compiler))
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')
        elif ct == 'msvc':
            opts.append('/DVERSION_INFO=\\"%s\\"' % self.distribution.get_version())
        for ext in self.extensions:
            ext.extra_compile_args = opts
            ext.extra_link_args = link_opts
        build_ext.build_extensions(self)

# Get environment variables for include and library paths
mpi_include = os.environ.get('MPI_HOME', '') + '/include'
grpc_include = os.environ.get('GRPC_HOME', '') + '/include'
protobuf_include = os.environ.get('PROTOBUF_HOME', '') + '/include'
absl_include = os.environ.get('ABSL_HOME', '') + '/include'

ext_modules = [
    Extension(
        'leaf._core',
        sources=[
            'src/core.cpp',
            'src/core_impl.cpp',
            'src/model.cpp',
            'src/distributed_model.cpp',
            'src/criterion.cpp',
            'src/user_credentials.cpp',
            'src/server.cpp',
            'src/server_communication.cpp',
            'src/server_communication.pb.cc',
            'src/server_communication.grpc.pb.cc'
        ],
        include_dirs=[
            get_pybind_include(),
            get_pybind_include(user=True),
            mpi_include,
            grpc_include,
            protobuf_include,
            absl_include,
            'src',  # Add the directory containing the generated files
        ],
        libraries=['grpc++', 'grpc++_reflection', 'protobuf'],
        library_dirs=[
            os.environ.get('GRPC_HOME', '') + '/lib',
            os.environ.get('PROTOBUF_HOME', '') + '/lib'
        ],
        language='c++'
    ),
]

setup(
    name='leaf',
    version=__version__,
    author='Ryan Marr',
    author_email='r.marr747@gmail.com',
    description='Leaf is a distributed training framework.',
    long_description='',
    ext_modules=ext_modules,
    install_requires=['pybind11>=2.6.0'],
    setup_requires=['pybind11>=2.6.0'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
    python_requires=">=3.6",
) 