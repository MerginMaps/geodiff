#!/usr/bin/env bash
set -e

choco install miniconda3
C:/tools/miniconda3/condabin/conda.bat install -c conda-forge boost
