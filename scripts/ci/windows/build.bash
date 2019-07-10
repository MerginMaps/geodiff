#!/usr/bin/env bash
set -e
cd geodiff

echo "Windows Visual Studio 15 64b build"
mkdir -p build_win
cd build_win
C:/Program\ Files/CMake/bin/cmake -G "Visual Studio 15 Win64" ${CMAKE_OPTIONS} \
   -DCMAKE_BUILD_TYPE=Rel \
   -DENABLE_TESTS=ON \
   ..

C:/Program\ Files/CMake/bin/cmake --build .

export PATH="$PATH:/c/Users/travis/build/lutraconsulting/geodiff/geodiff/build_win/Debug/"
echo "PATH used: $PATH"

C:/Program\ Files/CMake/bin/ctest -VV --exclude-regex "geodiffinfo_test"
cd ..