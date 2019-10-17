#!/usr/bin/env bash
set -e
choco install python
refreshenv
pip install --upgrade setuptools pip
pip install twine scikit-build wheel cmake
