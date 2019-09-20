[![Build Status](https://travis-ci.org/lutraconsulting/geodiff.svg?branch=master)](https://travis-ci.org/lutraconsulting/geodiff)
[![CircleCI](https://circleci.com/gh/lutraconsulting/geodiff.svg?style=svg)](https://circleci.com/gh/lutraconsulting/geodiff)
[![Coverage Status](https://img.shields.io/coveralls/lutraconsulting/geodiff.svg)](https://coveralls.io/github/lutraconsulting/geodiff?branch=master)

# geodiff
Library for handling diffs for geospatial data 

Use case 1: user has a GeoPackage with some data, then creates a copy and modifies it. Using this library it is possible to create a "difference" (delta) file that contains only changes between the original and the modified GeoPackage. The library can also take the original file and the generated diff file and produce the modified file.

Use case 2: two users start with the same copy of GeoPackage file which they modify independently. This may create conflicts when trying to merge edits of the two users back into one file. The library takes care of resolving any potential conflicts so that the changes can be applied cleanly.

The library is used by [Mergin](https://public.cloudmergin.com/) - a platform for easy sharing of spatial data.

## Envirmonment

Output messages could be adjusted by GEODIFF_LOGGER_LEVEL environment variable. 
See [header](https://github.com/lutraconsulting/geodiff/blob/master/geodiff/src/geodiff.h) for details

## Install 

`pip3 install pygeodiff`

if you got error `ModuleNotFoundError: No module named 'skbuild'` try to update pip with command
`python -m pip install --upgrade pip`

## Publishing 

### PyPi

you need to have registration on pypi/testpypi and manage access to pygeodiff distribution. 
on local PC you need to have `.pypirc` file with username/pass for pypi/testpypi servers.

run `python3 ./scripts/version_update.py --version x.y.z`

following by `./scripts/publish_pypi.bash` (add `--test` if you want to publish to testpypi)

and tagging the repository accordingly!


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
  GEODIFFLIB=`pwd`/../build/libgeodiff.dylib nose2
```

# Dependencies & Licensing

Library uses its own copy of
 - [sqlite3](https://sqlite.org/index.html) (Public Domain)
 - [libgpkg](https://github.com/luciad/libgpkg) (Apache-2)
