OTHER_FILES += \
    Bootstrap/CMakeLists.txt \
    Bootstrap/Bootstrap.cmd \
    Bootstrap/Bootstrap.sh \
    Bootstrap/mac-clang.sh \
    Bootstrap/libosip2.cmake \
    Bootstrap/libeXosip2.cmake \

CONFIG(release,release|debug):BUILD_TYPE="Release"
else:BUILD_TYPE="Debug"

win32:CONFIG+=bootstrap
macx:!CONFIG(no-bootstrap):CONFIG+=bootstrap

win32:system(cmd /c "$${PWD}/Bootstrap.cmd \"$${OUT_PWD}\"" $${BUILD_TYPE} $${OPENSSL_PREFIX} $${QT_ARCH})
unix:CONFIG(bootstrap):system($${PWD}/Bootstrap.sh $${OUT_PWD} $${BUILD_TYPE} $${OPENSSL_PREFIX} $${QT_ARCH})

CONFIG(bootstrap) {
    Message(*** Bootstrap included ***)
    INCLUDEPATH += $${OUT_PWD}/Bootstrap/include
    LIBS += -L$${OUT_PWD}/Bootstrap/lib
}
