#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

URL="pypi"

echo -n "Publishing pygeodiff to $URL"
cd $DIR/../..

$DIR/clean.bash

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
   PYTHON=python3
   ${PYTHON} setup.py sdist bdist_wheel
elif [ "$TRAVIS_OS_NAME" == "windows" ]; then
   PYTHON=/mnt/c/Python38/python
   ${PYTHON} setup.py bdist_wheel
else
   PYTHON=python3
   ${PYTHON} setup.py bdist_wheel
fi

# upload to testpypi
${PYTHON} -m twine upload  dist/* --username "__token__" --password "$PYPI_TOKEN" -r "$URL"  --skip-existing

cd $PWD