#!/bin/sh

set -eu

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

LOG_FILE=/tmp/cppcheck_geodiff.txt

rm -f ${LOG_FILE}
echo "cppcheck for ${SCRIPT_DIR}/../geodiff"

cppcheck --inline-suppr \
         --template='{file}:{line},{severity},{id},{message}' \
         --enable=all --inconclusive --std=c++17 \
         -j $(nproc) \
	     -igeodiff/src/3rdparty \
         ${SCRIPT_DIR}/../geodiff/src \
         >>${LOG_FILE} 2>&1 &

PID=$!
while kill -0 $PID 2>/dev/null; do
    printf "."
    sleep 1
done
echo " done"
if ! wait $PID; then
    echo "cppcheck could not be started"
    exit 1
fi

ret_code=0

for category in "error" "style" "performance" "warning" "clarifyCalculation" "portability"; do
    if grep "${category}," ${LOG_FILE}  >/dev/null; then
        echo "ERROR: Issues in '${category}' category found:"
        grep "${category}," ${LOG_FILE}
        echo ""
        echo "${category} check failed !"
        ret_code=1
    fi
done

if [ ${ret_code} = 0 ]; then
    echo "cppcheck succeeded"
fi

exit ${ret_code}
