#!/usr/bin/env python
# -*- coding: utf-8 -*-

from skbuild import setup
import platform

# use scripts/update_version.py to update the version here and in other places at once
VERSION = '1.0.6'

cmake_args = [
    '-DENABLE_TESTS:BOOL=OFF',
    '-DENABLE_COVERAGE:BOOL=OFF',
    '-DBUILD_TOOLS:BOOL=OFF',
    '-DPYGEODIFFVERSION='+str(VERSION)
]

arch = platform.architecture()[0]  # 64bit or 32bit
if ('Windows' in platform.system()) and ("32" in arch):
    cmake_args.append('-AWin32')

setup(
    name="pygeodiff",
    version=VERSION,
    author="Peter Petrik",
    author_email="peter.petrik@lutraconsulting.co.uk",
    description="Python wrapper around GeoDiff library",
    long_description="Python wrapper around GeoDiff library",
    url="https://github.com/lutraconsulting/geodiff",
    packages=["pygeodiff"],
    include_package_data=False,
    keywords=["diff", "gis", "geo", "geopackage", "merge"],
    scripts=[],
    entry_points={"console_scripts": ["pygeodiff=pygeodiff.main:main"]},
    zip_safe=False,
    cmake_args=cmake_args,
    cmake_source_dir="geodiff",
    cmake_with_sdist=False,
    test_suite="tests.test_project",
    python_requires=">=3.7",
    license="License :: OSI Approved :: MIT License",
    classifiers=[
        "Programming Language :: Python :: 3",
    ],
)
