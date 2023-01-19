#!/bin/bash

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac

BLACK=$(which black)
if [ $? -ne 0 ]; then
	echo "[!] black not installed." >&2
	exit 1
fi
$BLACK --version

echo "running run_black for $SCRIPT_DIR/../pygeodiff"

$BLACK $SCRIPT_DIR/../pygeodiff --check --quiet
RETURN=$?

# this does not return 0 or 1 exit code
$BLACK $SCRIPT_DIR/../pygeodiff --verbose 

exit $RETURN
