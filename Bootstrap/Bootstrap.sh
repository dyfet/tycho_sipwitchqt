#!/bin/sh
bootstrap=`pwd`
output="$1"
build="$2"
openssl="$3"
arch="$4"
if test -z "$bootstrap" ; then
    echo "*** no work directories" >&2 ; exit 1 ; fi

if test ! -z "$output" ; then
    cd "$output" ; fi

mkdir -p Bootstrap
cd Bootstrap
case `uname` in
Darwin)
    PATH=$PATH:/usr/local/bin:/opt/local/bin
    CPPFLAGS="$CPPFLAGS -mmacosx-version-min=10.10"
    CC=clang
    CXX=clang++
    export CC CXX
    ;;
*)
    PATH=$PATH:/usr/local/bin
    ;;
esac
PATH="$output/Bootstrap/bin:$PATH"
export PATH

CPPFLAGS="-I$output/Bootstrap/include $CPPFLAGS"
LDFLAGS="-L$output/Bootstrap/lib $LDFLAGS"
export CPPFLAGS LDFLAGS

cmake $bootstrap -DPORTS_BUILD_TYPE="$build" -DOPENSSL_PREFIX="$openssl" -DPORTS_ARCH="$arch" "-GUnix Makefiles"
cmake --build . --config Release
