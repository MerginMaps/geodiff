#!/usr/bin/env bash
set -e

# GEODIFF
sudo apt-get -qq update

# Valgrind
sudo apt-get install libc6-dbg gdb valgrind

# Code coverage
sudo apt-get install -y ruby
sudo apt-get install -y lcov
sudo gem install coveralls-lcov

# deploy on pypi
sudo apt-get -y install python3 python3-pip
sudo apt-get install ninja-build
sudo python -m pip install setuptools twine scikit-build wheel cmake