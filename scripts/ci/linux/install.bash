#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# GEODIFF
sudo apt-get -qq update

# Valgrind
sudo apt-get install libc6-dbg gdb valgrind

# Code coverage
sudo apt-get install -y ruby
sudo apt-get install -y lcov
sudo gem install coveralls-lcov

# deploy on pypi
sudo apt-get install -y python3
sudo apt-get install -y ninja-build
sudo python3 $DIR/../get-pip.py
sudo python3 -m pip install setuptools twine scikit-build wheel cmake