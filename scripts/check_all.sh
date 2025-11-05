DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PWD=`pwd`
cd $DIR
CI=0 ./run_astyle.sh `find ../geodiff/{src,tests} ! -path '*/3rdparty/*' '(' -name \*.h* -o -name \*.c* ')' -print`
./run_cppcheck.sh
./run_black.sh
cd $PWD
