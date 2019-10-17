#!/bin/bash
set -e -x

# Install a system package required by our library
yum install -y cmake

# Compile wheels
for PYBIN in /opt/python/*/bin; do
    "${PYBIN}/pip" install setuptools twine scikit-build wheel
    "${PYBIN}/pip" wheel /io/ -w dist/
done

# Bundle external shared libraries into the wheels
for whl in wheelhouse/*.whl; do
    auditwheel repair "$whl" --plat $PLAT -w /io/dist/
done
