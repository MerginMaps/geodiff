#!/usr/bin/env bash

set -e
cd geodiff

echo "Linux Release build"
mkdir -p build_rel_lnx
cd build_rel_lnx

cmake ${CMAKE_OPTIONS} \
      -DCMAKE_BUILD_TYPE=Rel \
      -DENABLE_TESTS=ON \
      ..
make


echo "Linux C++ tests"
CTEST_TARGET_SYSTEM=Linux-gcc; ctest -VV

echo "Linux Python tests"
cd ../../
GEODIFFLIB=`pwd`/geodiff/build_rel_lnx/libgeodiff.so nose2
