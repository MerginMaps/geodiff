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
docker pull quay.io/pypa/manylinux2010_x86_64
