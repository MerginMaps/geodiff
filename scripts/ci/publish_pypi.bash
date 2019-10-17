#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PWD=`pwd`

URL="pypi"

echo -n "Publishing pygeodiff to $URL"
cd $DIR/../..

$DIR/clean.bash

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    python3 setup.py sdist bdist_wheel
else
   python3 setup.py bdist_wheel
fi

# upload to testpypi
twine upload dist/* -r "$URL" --username "__token__" --password "$PYPI_TOKEN"

cd $PWD