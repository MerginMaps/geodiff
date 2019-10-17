#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# https://chocolatey.org/packages/python3
# python 3.8
choco install python3 --version=3.8.0
C:/Python38/python.exe $DIR/../get-pip.py
C:/Python38/Scripts/pip.exe install setuptools twine scikit-build wheel cmake

# python 3.7
choco install python3 --version=3.7.5
C:/Python37/python.exe $DIR/../get-pip.py
C:/Python37/Scripts/pip.exe install setuptools twine scikit-build wheel cmake

# python 3.6
choco install python3 --version=3.6.8
C:/Python36/python.exe $DIR/../get-pip.py
C:/Python36/Scripts/pip.exe install setuptools twine scikit-build wheel cmake