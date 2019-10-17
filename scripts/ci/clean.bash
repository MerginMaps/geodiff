#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

rm -rf $DIR/../../pygeodiff.egg-info
rm -rf $DIR/../../_skbuild
rm -rf $DIR/../../dist
rm -rf $DIR/../../MANIFEST
rm -rf $DIR/../../pygeodiff/tests/tmp
