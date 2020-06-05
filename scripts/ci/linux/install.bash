#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# GEODIFF
sudo apt-get -qq update

# deploy on pypi
if [ -n "$TRAVIS_TAG" ]; then
    docker pull quay.io/pypa/manylinux2010_x86_64
    sudo apt-get install -y python3
    sudo python3 $DIR/../get-pip.py
    sudo python3 -m pip install setuptools twine scikit-build wheel
fi