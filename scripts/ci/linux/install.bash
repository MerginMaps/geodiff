#!/usr/bin/env bash
set -e

# GEODIFF
sudo apt-get -qq update
sudo apt-get install libboost-all-dev

# MinGW
sudo apt-get install mingw-w64

# Valgrind
sudo apt-get install libc6-dbg gdb valgrind

# Code coverage
sudo apt-get install -y ruby
sudo apt-get install -y lcov
sudo gem install coveralls-lcov
