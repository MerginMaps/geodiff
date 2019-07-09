[![Build Status](https://travis-ci.org/lutraconsulting/geodiff.svg?branch=master)](https://travis-ci.org/lutraconsulting/geodiff)
[![CircleCI](https://circleci.com/gh/lutraconsulting/geodiff.svg?style=svg)](https://circleci.com/gh/lutraconsulting/geodiff)
[![Coverage Status](https://img.shields.io/coveralls/lutraconsulting/geodiff.svg)](https://coveralls.io/github/lutraconsulting/geodiff?branch=master)

# geodiff
Library for handling diffs for geospatial data 

## Version update

Update:
`pygeodiff/pygeodiff/__about__.py` and 
`geodiff/src/geodiff.cpp` 

## Development
- Compile geodiff shared library
```
  mkdir build
  cd build
  cmake ../geodiff
  make
```
Run tests and check it is ok `./test_geodiff`


- run pygeodiff tests for python module, you need to setup GEODIFFLIB with path to .so/.dylib from step1
```
  cd pygeodiff
  GEODIFFLIB=`pwd`/../build/libgeodiff.dylib nose2
```
