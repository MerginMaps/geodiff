[![Coverage Tests](https://github.com/lutraconsulting/geodiff/workflows/Coverage%20Tests/badge.svg)](https://github.com/lutraconsulting/geodiff/actions?query=workflow%3A%22Coverage+Tests%22)
[![MemCheck Tests](https://github.com/lutraconsulting/geodiff/workflows/MemCheck%20Tests/badge.svg)](https://github.com/lutraconsulting/geodiff/actions?query=workflow%3A%22MemCheck+Tests%22)
[![Code Style](https://github.com/lutraconsulting/geodiff/workflows/Code%20Layout/badge.svg)](https://github.com/lutraconsulting/geodiff/actions?query=workflow%3A%22Code+Layout%22)
[![Coverage Status](https://img.shields.io/coveralls/lutraconsulting/geodiff.svg)](https://coveralls.io/github/lutraconsulting/geodiff?branch=master)
[![PyPI version](https://badge.fury.io/py/pygeodiff.svg)](https://badge.fury.io/py/pygeodiff)
[![Build PyPI wheels](https://github.com/lutraconsulting/geodiff/actions/workflows/python_packages.yml/badge.svg)](https://github.com/lutraconsulting/geodiff/actions/workflows/python_packages.yml)

# geodiff
Library for handling diffs for geospatial data 

Use case 1: user has a GeoPackage with some data, then creates a copy and modifies it. Using this library it is possible to create a "difference" (delta) file that contains only changes between the original and the modified GeoPackage. The library can also take the original file and the generated diff file and produce the modified file.

Use case 2: two users start with the same copy of GeoPackage file which they modify independently. This may create conflicts when trying to merge edits of the two users back into one file. The library takes care of resolving any potential conflicts so that the changes can be applied cleanly.

Use case 3: user has a PostgreSQL database with some GIS data, and wants to sync it with the GeoPackage file to be used for field survey. Both GeoPackage and PostgreSQL could be modified, and the library can create "difference" (delta) file, apply them to both sources and keep them in sync.

The library is used by [Mergin](https://public.cloudmergin.com/) - a platform for easy sharing of spatial data.

## How to use geodiff

There are multiple ways how geodiff can be used:

- `geodiff` command line interface (CLI) tool
- `pygeodiff` Python module
- `geodiff` library using C API

The library nowadays comes with support for two drivers:
- SQLite / GeoPackage - always available
- PostgreSQL / PostGIS - optional, needs to be compiled

## Changesets

Changes between datasets are read from and written to a [binary changeset format](docs/changeset-format.md).

# Using command line interface

To get changes between two GeoPackage files and write them to `a-to-b.diff` (a binary diff file):
```bash
geodiff diff data-a.gpkg data-b.gpkg a-to-b.diff
```

To print changes between two GeoPackage files to the standard output:
```bash
geodiff diff --json data-a.gpkg data-b.gpkg
```

To apply changes from `a-to-b.diff` to `data-a.gpkg`:
```bash
geodiff apply data-a.gpkg a-to-b.diff
```

To invert a diff file `a-to-b.diff` and revert `data-a.gpkg` to the original content:
```base
geodiff invert a-to-b.diff b-to-a.diff
geodiff apply data-a.gpkg b-to-a.diff
```

The `geodiff` tool supports other various commands, use `geodiff help` for the full list.

# Using Python module

Install the module from pip:
```bash
pip3 install pygeodiff
```

If you get error `ModuleNotFoundError: No module named 'skbuild'` try to update pip with command
`python -m pip install --upgrade pip`

Sample usage of the Python module:

```python
import pygeodiff

geodiff = pygeodiff.GeoDiff()

# create a diff between two GeoPackage files
geodiff.create_changeset('data-a.gpkg', 'data-b.gpkg', 'a-to-b.diff')

# apply changes from a-to-b.diff to the GeoPackage file data-a.gpkg
geodiff.apply_changeset('data-a.gpkg, 'a-to-b.diff')

# export changes from the binary diff format to JSON
geodiff.list_changes('a-to-b.diff', 'a-to-b.json')
```

If there are any problems, calls will raise `pygeodiff.GeoDiffLibError` exception. 

# Using the library with C API

See [geodiff.h header file](https://github.com/lutraconsulting/geodiff/blob/master/geodiff/src/geodiff.h) for the list of API calls and their documentation.

Output messages can be adjusted by GEODIFF_LOGGER_LEVEL environment variable.

# Building geodiff

Install postgresql client and sqlite3 library, e.g. for Linux
```bash
sudo apt-get install libsqlite3-dev libpq-dev
```
or MacOS (using SQLite from [QGIS deps](https://qgis.org/downloads/macos/deps/)) by defining SQLite variables in 
a cmake configuration as following:
```bash
SQLite3_INCLUDE_DIR=/opt/QGIS/qgis-deps-${QGIS_DEPS_VERSION}/stage/include 
SQLite3_LIBRARY=/opt/QGIS/qgis-deps-${QGIS_DEPS_VERSION}/stage/lib/libsqlite3.dylib 
```

Compile geodiff:
```bash
mkdir build
cd build
cmake .. -DWITH_POSTGRESQL=TRUE
make
```

# Development of geodiff 

## Running tests

C++ tests: run `make test` or `ctest` to run all tests. Alternatively run just a single test, e.g. `./tests/geodiff_changeset_reader_test`

Python tests: you need to setup GEODIFFLIB with path to .so/.dylib from build step
```bash
GEODIFFLIB=`pwd`/../build/libgeodiff.dylib nose2
```

## Releasing new version 

- run `python3 ./scripts/update_version.py --version x.y.z`
- push to GitHub
- tag the master & create github release - Python wheels will be automatically published to PyPI!

# Dependencies & Licensing

Library uses its own copy of
 - [base64](geodiff/src/3rdparty/base64utils.cpp)
 - [endian](geodiff/src/3rdparty/portableendian.h)
 - [libgpkg](https://github.com/luciad/libgpkg) (Apache-2)
