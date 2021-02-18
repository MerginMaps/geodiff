#!/bin/bash
set -e

eval "$(pyenv init -)"

for VER in "3.7.4" "3.6.8"
do
    pyenv local $VER
    python --version
    python -m pip install setuptools twine scikit-build wheel
    python setup.py bdist_wheel
done
