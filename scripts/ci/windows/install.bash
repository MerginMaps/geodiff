#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

choco install python3
python $DIR/../get-pip.py
pip install --upgrade setuptools pip
pip install twine scikit-build wheel cmake
