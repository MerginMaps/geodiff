#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

echo -n "Publishing pygeodiff to $URL"
cd $DIR/../..

$DIR/clean.bash

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
   PYTHON=python3
   PLAT=manylinux2010_x86_64
   DOCKER_IMAGE=quay.io/pypa/manylinux2010_x86_64
   docker run --rm -e PLAT=$PLAT -v $DIR/../../:/io $DOCKER_IMAGE /io/scripts/ci/linux/build_wheel.bash

elif [ "$TRAVIS_OS_NAME" == "windows" ]; then
   PYTHON=C:/Python38/python.exe
   ${PYTHON} setup.py bdist_wheel
else
   # MacOS
   PYTHON=python3
   ${PYTHON} setup.py sdist bdist_wheel
fi

# upload to testpypi
${PYTHON} -m twine --version
${PYTHON} -m twine upload  dist/* --username "__token__" --password "$PYPI_TOKEN"  --skip-existing

cd $PWD