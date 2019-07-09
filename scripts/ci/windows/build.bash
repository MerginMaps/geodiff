#!/usr/bin/env bash
set -e
cd geodiff

CONDA_DIR="C:/tools/miniconda3"
C:/tools/miniconda3/condabin/activate.bat

echo "Windows Visual Studio 15 64b build"
mkdir -p build_win
cd build_win
C:/Program\ Files/CMake/bin/cmake -G "Visual Studio 15 Win64" ${CMAKE_OPTIONS} \
   -DCMAKE_BUILD_TYPE=Rel \
   -DENABLE_TESTS=ON \
   -DBOOST_ROOT=${CONDA_DIR} \
   ..

C:/Program\ Files/CMake/bin/cmake --build .

export PATH="$PATH:/c/OSGeo4W64/bin:/c/Users/travis/build/lutraconsulting/geodiff/build_win/tools/Debug:/c/Users/travis/build/lutraconsulting/geodiff/build_win/mdal/Debug"
echo "PATH used: $PATH"

C:/Program\ Files/CMake/bin/ctest -VV
cd ..
