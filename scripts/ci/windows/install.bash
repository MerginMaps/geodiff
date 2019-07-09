#!/usr/bin/env bash
set -e

choco install miniconda3
ls -la C:/tools/miniconda3/condabin

C:/tools/miniconda3/condabin/conda.bat install -c anaconda -y boost
