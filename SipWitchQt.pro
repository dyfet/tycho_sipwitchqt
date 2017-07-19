TEMPLATE = app
VERSION = 0.1.0
COPYRIGHT = 2017
ARCHIVE = sipwitchqt
PRODUCT = SipWitchQt
TARGET = $${ARCHIVE}

# basic compile and link config
CONFIG += c++11 console
QT -= gui
QT += network sql
QMAKE_CXXFLAGS += -Wno-padded

# prefix and build env
win32-msvc*:error(*** windows no longer supported...)
isEmpty(PREFIX):PREFIX=$$system(echo $$[QT_INSTALL_DATA] | sed s:/[a-z0-9]*/qt5$::)

# check if normal prefix...
equals(PREFIX, "/usr")|equals(PREFIX, "/usr/local") {
    VARPATH=/var/lib
    LOGPATH=/var/log
    ETCPATH=/etc
}
else {
    VARPATH=$${PREFIX}/var
    ETCPATH=$${PREFIX}/etc
    LOGPATH=$${PREFIX}/var/log
}

# platform specific options
linux {
    !CONFIG(no-systemd) {
        CONFIG += link_pkgconfig
        PKGCONFIG += libsystemd
        DEFINES += UNISTD_SYSTEMD
    }
}

macx {  
    # check for building under homebrew directly...
    equals(PREFIX, "/usr/local") {
        CONFIG -= app_bundle
        INCLUDEPATH += $${PREFIX}/include
        LIBS += -L$${PREFIX}/lib
    }
    # else a bundled app with Qt runtime will be built
    else {
        system(rm -rf $${OUT_PWD}/$${TARGET}.app)
        TARGET = $${PRODUCT}
        VARPATH=/var/lib
        LOGPATH=/var/log
        ETCPATH=/etc
    }
}

# build type specific options
CONFIG(release,release|debug) {
    DEFINES += QT_NO_DEBUG_OUTPUT QT_NO_DEBUG
    unix:!macx:LIBS += -ltcmalloc_minimal
}
else {
    # the debug build target must be in etc for most python utils to work fully
    DEFINES += DEBUG_LOGGING
    !CONFIG(no-testdata) {
        CONFIG(userdata):PROJECT_PREFIX=\"$${PWD}/userdata\"
        else:PROJECT_PREFIX=\"$${PWD}/testdata\"

        # the debug target must be in etc for most python utils to work fully
        macx:CONFIG -= app_bundle
        macx:TARGET = $${ARCHIVE}
        unix:system(ln -sf $${OUT_PWD}/$${TARGET} $${PWD}/etc/$${ARCHIVE})

        DEFINES += DEBUG_TESTDATA
    }
}

# global defines
DEFINES += \
    PROJECT_VERSION=\\\"$${VERSION}\\\" \
    PROJECT_COPYRIGHT=\\\"$${COPYRIGHT}\\\" \
    SERVICE_NAME=\\\"$${TARGET}\\\" \

isEmpty(PROJECT_PREFIX) {
    DEFINES += \
        SERVICE_ETCPATH=\\\"$${ETCPATH}\\\" \
        SERVICE_VARPATH=\\\"$${VARPATH}/$${SERVICE_NAME}\\\" \
        SERVICE_LOGPATH=\\\"$${LOGPATH}\\\" \
        SERVICE_LIBEXEC=\\\"$${PREFIX}/libexec/$${SERVICE_NAME}\\\" \
}
else {
    DEFINES += \
        SERVICE_ETCPATH=\\\"$${PROJECT_PREFIX}\\\" \
        SERVICE_VARPATH=\\\"$${PROJECT_PREFIX}\\\" \
        SERVICE_LOGPATH=\\\"$${PROJECT_PREFIX}\\\" \
        SERVICE_LIBEXEC=\\\"$${PROJECT_PREFIX}\\\" \
        PROJECT_PREFIX=\\\"$${PROJECT_PREFIX}\\\"
}

# project layout and components, this is really the cool part of qmake
include(src/Common.pri)
include(src/Database.pri)
include(src/Main.pri)
include(src/Network.pri)

# extra install targets based on bundle state
!CONFIG(app_bundle) {
    INSTALLS += target config

    config.path = $${ETCPATH}
    config.files = $${PWD}/etc/sipwitchqt.conf
    config.depends = target

    target.path = $${PREFIX}/sbin
    target.depends = all
}
else:CONFIG(release, release|debug) {
    CONFIG += separate_debug_info force_debug_info
    QMAKE_EXTRA_TARGETS += archive publish_and_archive
    archive.depends = all
    archive.commands += mkdir -p ../Archive &&
    archive.commands += rm -rf ../Archive/$${TARGET}.app ../Archive/$${TARGET}.dSYM &&
    archive.commands += cp -a $${TARGET}.app ../Archive &&
    archive.commands += cp -a $${TARGET}.app.dSYM ../Archive/$${TARGET}.dSYM &&
    archive.commands += cd ../Archive && macdeployqt $${TARGET}.app -verbose=0 -always-overwrite
    publish_and_archive.depends = publish archive
}

QMAKE_EXTRA_TARGETS += clean extra_clean
clean.depends += extra_clean
extra_clean.commands += rm -f $${ARCHIVE}
macx:extra_clean.commands += && rm -rf $${PRODUCT}.app $${PRODUCT}.app.dSYM 

# publish support
QMAKE_EXTRA_TARGETS += publish
publish.commands += cd $${PWD} &&
publish.commands += rm -f $${OUT_PWD}/$${ARCHIVE}-${VERSION}.tar.gz &&
publish.commands += git archive --format tar --prefix=$${ARCHIVE}-$${VERSION}/ HEAD |
publish.commands += gzip >$${OUT_PWD}/$${ARCHIVE}-$${VERSION}.tar.gz

# documentation processing
QMAKE_EXTRA_TARGETS += docs
QMAKE_SUBSTITUTES += doxyfile
DOXYPATH = $${PWD}
doxyfile.input = $${PWD}/Doxyfile
doxyfile.output = $${OUT_PWD}/Doxyfile.out
macx:docs.commands += PATH=/usr/local/bin:/usr/bin:/bin:/Library/Tex/texbin:$PATH && export PATH &&
docs.commands += cd $${OUT_PWD} && doxygen Doxyfile.out
macx:docs.commands += && cd doc/html && make docset && cd ../..
unix:docs.commands += && cd doc/latex && make
unix:publish.depends += docs
unix:publish.commands += && cp $${OUT_PWD}/doc/latex/refman.pdf $${OUT_PWD}/$${ARCHIVE}-$${VERSION}.pdf

# clean additional testing files on distclean...
QMAKE_EXTRA_TARGETS += distclean publishclean
distclean.depends += publishclean
publishclean.commands = cd $${PWD} && rm -rf testdata/*.db etc/$${ARCHIVE} testdata/certs testdata/private $testdata/*.log 
publishclean.commands += cd $${OUT_PWD} && rm -rf Archive $${ARCHIVE}-*.tar.gz $${ARCHIVE}-*.pdf $${ARCHIVE} doc Doxyfile.out

# other files...
OTHER_FILES += \
    etc/sipwitchqt.conf \
    etc/README.md \
    testdata/service.conf \
    testdata/ping.xml \
    testdata/siptest.sh \
    README.md \
    CONTRIBUTING.md \
    TODO.md \
    LICENSE \
    Doxyfile \
    sipwitchqt.spec \

# common target properties
QMAKE_TARGET_COMPANY = "Tycho Softworks"
QMAKE_TARGET_COPYRIGHT = "$${COPYRIGHT} Tycho Softworks"
QMAKE_TARGET_PRODUCT = "$${PRODUCT}"
QMAKE_TARGET_DESCRIPTION = "Tycho SIP Witch Service"
