#!/usr/bin/env bash

set -e
cd geodiff

echo "OSX native build"
mkdir -p build_osx
cd build_osx

cmake ${CMAKE_OPTIONS} \
      -DCMAKE_BUILD_TYPE=Rel \
      -DENABLE_TESTS=ON \
      ..
make

ctest -VV

cd ../../
GEODIFFLIB=`pwd`/geodiff/build_osx/libgeodiff.dylib nose2