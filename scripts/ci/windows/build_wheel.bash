#!/bin/bash
set -e

for EXE in "C:/Python36/python.exe" "C:/Python37/python.exe" "C:/Python38/python.exe" "C:/Python36_32b/python.exe" "C:/Python37_32b/python.exe" "C:/Python38_32b/python.exe"
do
    echo "$EXE"
    $EXE setup.py bdist_wheel
done

