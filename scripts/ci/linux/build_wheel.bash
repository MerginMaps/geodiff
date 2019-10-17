#!/bin/bash
set -e -x

# Install a system package required by our library
yum install -y cmake

ls -la /opt/python

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    if [[ $PYBIN == *"python/cp2"* ]]; then
      echo "skipping python2 $PYBIN"
    elif [[ $PYBIN == *"python/cp34"* ]]; then
      echo "skipping python3.4 $PYBIN"
    elif [[ $PYBIN == *"python/cp35"* ]]; then
      echo "skipping python3.5 $PYBIN"
    else
      "${PYBIN}/pip" install setuptools twine scikit-build wheel
      "${PYBIN}/pip" wheel /io/ -w wheelhouse/
    fi
done

# Bundle external shared libraries into the wheels
for whl in wheelhouse/*.whl; do
    auditwheel repair "$whl" --plat $PLAT -w /io/dist/
done
