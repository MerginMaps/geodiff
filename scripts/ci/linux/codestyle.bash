#!/usr/bin/env bash

set -e
cd scripts; ./run_astyle.sh `find ../geodiff -name \*.h* -print -o -name \*.c* -print`
cd ..