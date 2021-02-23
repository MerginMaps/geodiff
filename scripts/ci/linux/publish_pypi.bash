#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

echo -n "building pygeodiff dist"
cd $DIR/../../..

$DIR/../clean.bash

# build sdist
python3 setup.py sdist
# publish when tagged
if [ -n "$TRAVIS_TAG" ]; then
  python3 -m twine upload dist/pygeodiff*.tar.gz --username "__token__" --password "$PYPI_TOKEN"  --skip-existing
else
  echo "Skipping deployment of source, not tagged"
fi

$DIR/../clean.bash

# build wheels
PLAT=manylinux2010_x86_64
DOCKER_IMAGE=quay.io/pypa/manylinux2010_x86_64
docker run --rm -e PLAT=$PLAT -v $DIR/../../../:/io $DOCKER_IMAGE /io/scripts/ci/linux/build_wheel.bash

if [ -n "$TRAVIS_TAG" ]; then
  python3 -m twine upload  dist/* --username "__token__" --password "$PYPI_TOKEN"  --skip-existing
else
  echo "Skipping deployment of wheels, not tagged"
fi
cd $PWD
