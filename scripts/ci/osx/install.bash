#!/usr/bin/env bash

ls -la /usr/local/opt
ls -la /usr/local/Cellar

brew update
brew install python3
sudo python3 -m pip install nose2

brew install ninja
brew install pyenv
pyenv install 3.8.7
pyenv install 3.7.4
pyenv install 3.6.8

which python3

if [ -n "$TRAVIS_TAG" ]; then
    sudo python3 -m pip install twine
fi
