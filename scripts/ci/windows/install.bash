#!/usr/bin/env bash
set -e
choco install python3
choco install pip
pip install --upgrade setuptools pip
pip install twine scikit-build wheel cmake
