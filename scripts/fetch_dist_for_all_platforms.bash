#!/usr/bin/env bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
VER=$1

python3 $DIR/fetch_dist_for_all_platforms.py --version $VER
