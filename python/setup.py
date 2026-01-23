"""
Setup script for evocore Python bindings.

This script handles cffi compilation of the C extension module.
"""

from setuptools import setup

setup(
    cffi_modules=["evocore/_ffi_build.py:ffi"],
)
