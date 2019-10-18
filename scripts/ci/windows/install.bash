#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ -n "$TRAVIS_TAG" ]; then
    # python 3.6
    choco install python3 --version=3.6.8 --force
    C:/Python36/python.exe $DIR/../get-pip.py
    C:/Python36/Scripts/pip.exe install setuptools twine scikit-build wheel cmake

    # python 3.7
    choco install python3 --version=3.7.5 --force
    C:/Python37/python.exe $DIR/../get-pip.py
    C:/Python37/Scripts/pip.exe install setuptools twine scikit-build wheel cmake

    # python 3.8
    choco install python3 --version=3.8.0 --force
    C:/Python38/python.exe $DIR/../get-pip.py
    C:/Python38/Scripts/pip.exe install setuptools twine scikit-build wheel cmake
fi

    ### 32 bit
    choco install python3 --version=3.6.8 -forcex86 --params "/InstallDir:C:\Python37_32b" --force
    C:/Python37_32b/python.exe $DIR/../get-pip.py
    C:/Python37_32b/Scripts/pip.exe install setuptools twine scikit-build wheel cmake

    cd $DIR/../../..
    C:/Python37_32b setup.py bdist_wheel
    ls -la dist/