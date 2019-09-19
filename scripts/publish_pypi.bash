#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

if [[ $# -eq 1 ]]; then
    URL="testpypi"
else
    URL="pypi"
fi

echo -n "Publishing pygeodiff to $URL"
cd $DIR/../pygeodiff

rm -rf pygeodiff.egg-info
rm -rf _skbuild
rm -rf dist

# source distribution
python3 setup.py sdist

# binary distribution

# upload to testpypi
twine upload dist/* -r $URL

cd $PWD