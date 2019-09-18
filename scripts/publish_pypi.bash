#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

cd $DIR/../pygeodiff

rm -rf pygeodiff.egg-info
rm -rf _skbuild
rm -rf dist

# source distribution
python3 setup.py sdist bdist_wheel

# upload to testpypi
twine upload dist/pygeodiff-0.2.0.tar.gz -r testpypi

cd $PWD