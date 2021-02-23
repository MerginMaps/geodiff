#!/usr/bin/env bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

echo -n "building pygeodiff dist"
cd $DIR/../../..

$DIR/../clean.bash

# build sdist
echo "Skipping dist of source for osx"

# build wheels
$DIR/osx/build_wheel.bash
if [ -n "$TRAVIS_TAG" ]; then
  pyenv local 3.8.7
  python --version
  python -m twine upload  dist/* --username "__token__" --password "$PYPI_TOKEN"  --skip-existing
else
  echo "Skipping deployment of wheels, not tagged"
fi

cd $PWD
