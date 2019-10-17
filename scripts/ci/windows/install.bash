#!/usr/bin/env bash
set -e
choco install python
python get-pip.py
pip3 install twine scikit-build wheel cmake
