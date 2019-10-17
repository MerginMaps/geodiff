#!/usr/bin/env bash
set -e
choco install python
pip3 install --upgrade setuptools pip
pip3 install twine scikit-build wheel cmake
