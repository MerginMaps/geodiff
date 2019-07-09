#!/usr/bin/env bash
set -e

choco install miniconda3
ls -la C:/tools/miniconda3

C:/tools/miniconda3/bin/conda install -c anaconda boost
