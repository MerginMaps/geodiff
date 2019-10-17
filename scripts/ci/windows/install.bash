#!/usr/bin/env bash
set -e
choco install python
python -m pip install --upgrade setuptools pip
python -m pip install twine scikit-build wheel cmake
