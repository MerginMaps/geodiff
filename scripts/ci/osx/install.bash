#!/usr/bin/env bash

ls -la /usr/local/opt
ls -la /usr/local/Cellar

brew update
brew install python3
sudo python3 -m pip install nose2

brew install ninja
brew install pyenv

echo "%%% before pyenv install"
which python3
python3 --version

pyenv install 3.8.7
pyenv install 3.7.4
pyenv install 3.6.8

echo "%%% after pyenv install"
which python3
python3 --version

if [ -n "$TRAVIS_TAG" ]; then
    sudo python3 -m pip install twine
fi
