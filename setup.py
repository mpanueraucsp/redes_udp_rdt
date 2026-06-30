from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "UDPBind",
        ["bindings.cpp", "UDP.cpp", "ACK.cpp"], 
        include_dirs=[pybind11.get_include()],
        language='c++',
        extra_compile_args=['-std=c++11', '-pthread'] 
    ),
]

setup(
    name="UDPBind",
    ext_modules=ext_modules,
    install_requires=['pybind11>=2.10.0'],
    python_requires=">=3.6",
)