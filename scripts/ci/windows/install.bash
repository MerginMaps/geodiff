#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

choco install python3
C:/Python38/python.exe $DIR/../get-pip.py
C:/Python38/Scripts/pip.exe install setuptools twine scikit-build wheel cmake
