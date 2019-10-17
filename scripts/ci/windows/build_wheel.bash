#!/bin/bash
set -e

for EXE in "C:/Python36/python.exe" "C:/Python37/python.exe" "C:/Python38/python.exe"
do
    echo "$EXE"
    $EXE setup.py bdist_wheel
done

