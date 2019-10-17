#!/usr/bin/env bash
set -e
choco install python3
python3 -m pip install --upgrade setuptools pip
python3 -m pip install twine scikit-build wheel cmake
