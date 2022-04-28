DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PWD=`pwd`
cd $DIR
CI=0 ./run_astyle.sh `find ../geodiff -name \*.h* -print -o -name \*.c* -print`
./run_cppcheck.sh
cd $PWD
