#!/bin/sh

rm -rf doc

# this was probably for my older qmake based stuff...
if test -f Doxyfile ; then
    sed -e "s/[$][$]DOXYPATH/./g" <Doxyfile >/tmp/Doxyfile
    exec doxygen /tmp/Doxyfile
#   cd doc/html ; make docset
#   cd ../latex ; make
fi

PWD=`pwd`
if test -f CMakeLists.txt -a -f Doxyfile.in ; then
    sed -e "s;[$][{]CMAKE_.*_SOURCE_DIR[}];$PWD;" \
        -e "s;[$][{]CMAKE_.*_BINARY_DIR[}]/config.hpp;;" \
        <Doxyfile.in >/tmp/Doxyfile
    doxygen /tmp/Doxyfile
fi
