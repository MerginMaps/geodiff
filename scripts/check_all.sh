DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PWD=`pwd`
cd $DIR
CI=0 ./run_astyle.sh `find ../geodiff -name \*.h* ! -path "*/3rdparty/*" -print -o -name \*.c* ! -path "*/3rdparty/*" -print`
./run_cppcheck.sh
cd $PWD
