[![Build Status](https://travis-ci.org/lutraconsulting/geodiff.svg?branch=master)](https://travis-ci.org/lutraconsulting/geodiff)

[![Code Style](https://github.com/lutraconsulting/geodiff/workflows/Code%20Layout/badge.svg)](https://github.com/lutraconsulting/geodiff/actions?query=workflow%3A%22Code+Layout%22)

[![Build status](https://ci.appveyor.com/api/projects/status/94xich0m3x2pu4u3?svg=true)](https://ci.appveyor.com/project/PeterPetrik/geodiff)
[![Coverage Status](https://img.shields.io/coveralls/lutraconsulting/geodiff.svg)](https://coveralls.io/github/lutraconsulting/geodiff?branch=master)
[![PyPI version](https://badge.fury.io/py/pygeodiff.svg)](https://badge.fury.io/py/pygeodiff)

# geodiff
Library for handling diffs for geospatial data 

Use case 1: user has a GeoPackage with some data, then creates a copy and modifies it. Using this library it is possible to create a "difference" (delta) file that contains only changes between the original and the modified GeoPackage. The library can also take the original file and the generated diff file and produce the modified file.

Use case 2: two users start with the same copy of GeoPackage file which they modify independently. This may create conflicts when trying to merge edits of the two users back into one file. The library takes care of resolving any potential conflicts so that the changes can be applied cleanly.

Use case 3: user has a PostgreSQL database with some GIS data, and wants to sync it with the GeoPackage file to be used for field survey. Both GeoPackage and PostgreSQL could be modified, and the library can create "difference" (delta) file, apply them to both sources and keep them in sync.

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

run `python3 ./scripts/update_version.py --version x.y.z`
and push to GitHub

tag the master on github and it will be automatically published!

## Changesets

Changes between datasets are read from and written to a [binary changeset format](docs/changeset-format.md).

## Development
- Install postgresql client and sqlite3 library, e.g. for Linux
```
    sudo apt-get install libsqlite3-dev libpq-dev
```
- Compile geodiff shared library
```
  mkdir build
  cd build
  cmake ../geodiff -DWITH_POSTGRESQL=TRUE -DWITH_INTERNAL_SQLITE3=FALSE
  make
```
Run tests and check it is ok `./test_geodiff`


- run pygeodiff tests for python module, you need to setup GEODIFFLIB with path to .so/.dylib from step1
```
  GEODIFFLIB=`pwd`/../build/libgeodiff.dylib nose2
```

# Dependencies & Licensing

Library uses its own copy of
 - [base64](geodiff/src/3rdparty/base64utils.cpp)
 - [endian](geodiff/src/3rdparty/portableendian.h)
 - [sqlite3](https://sqlite.org/index.html) (Public Domain)
 - [libgpkg](https://github.com/luciad/libgpkg) (Apache-2)
