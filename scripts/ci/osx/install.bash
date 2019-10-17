#!/usr/bin/env bash

ls -la /usr/local/opt
ls -la /usr/local/Cellar

brew install python3
brew install ninja
sudo python3 -m pip install nose2
sudo python3 -m pip install setuptools twine scikit-build wheel
