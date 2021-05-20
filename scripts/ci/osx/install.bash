#!/usr/bin/env bash

ls -la /usr/local/opt
ls -la /usr/local/Cellar

brew update
brew install sqlite3
brew install ninja
brew install pyenv
for VER in "3.8.7" "3.7.4" "3.6.8"
do
  pyenv install $VER
  pyenv local $VER
  python --version
  python -m pip install nose2
  if [ -n "$TRAVIS_TAG" ]; then
    python -m pip install twine
  fi
done


