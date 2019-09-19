#!/usr/bin/env python
# -*- coding: utf-8 -*-

# For a fully annotated version of this file and what it does, see
# https://github.com/pypa/sampleproject/blob/master/setup.py

# To upload this file to PyPI you must build it then upload it:
# python setup.py sdist bdist_wheel  # build in 'dist' folder
# python-m twine upload dist/*  # 'twine' must be installed: 'pip install twine'

import io
import re
import os
from setuptools import find_packages
#from setuptools import setup
from skbuild import setup

EXCLUDE_FROM_PACKAGES = ["contrib", "docs", "tests*"]
CURDIR = os.path.abspath(os.path.dirname(__file__))

with io.open(os.path.join(CURDIR, "README"), "r", encoding="utf-8") as f:
    README = f.read()


VERSION = '0.2.2'

setup(
    name="pygeodiff",
    version=VERSION,
    author="Peter Petrik",
    author_email="peter.petrik@lutraconsulting.co.uk",
    description="Python wrapper around GeoDiff library",
    long_description=README,
    long_description_content_type="text/markdown",
    url="https://github.com/lutraconsulting/geodiff",
    packages=find_packages(exclude=EXCLUDE_FROM_PACKAGES),
    include_package_data=True,
    keywords=["diff", "gis", "geo", "geopackage", "merge"],
    scripts=[],
    entry_points={"console_scripts": ["pygeodiff=pygeodiff.main:main"]},
    zip_safe=False,
    cmake_args=['-DENABLE_TESTS:BOOL=OFF', '-DENABLE_COVERAGE:BOOL=OFF', '-DBUILD_TOOLS:BOOL=OFF', '-DPYGEODIFFVERSION='+str(VERSION)],
    cmake_source_dir="../geodiff",
    cmake_with_sdist=True,
    test_suite="tests.test_project",
    python_requires=">=3.6",
    # license and classifier list:
    # https://pypi.org/pypi?%3Aaction=list_classifiers
    license="License :: OSI Approved :: MIT License",
    classifiers=[
        # "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        # "Operating System :: OS Independent",
        # "Private :: Do Not Upload"
    ],
)