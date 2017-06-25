TEMPLATE = app
VERSION = 0.1.0
COPYRIGHT = 2017
ARCHIVE = sipwitchqt
PRODUCT = SipWitchQt
CONFIG += c++11 console
QT -= gui
QT += network sql

unix {
    isEmpty(PREFIX):PREFIX=$$system(echo $$[QT_INSTALL_DATA] | sed s:/[a-z0-9]*/qt5$::)
    !macx:CONFIG += make-install make-defines

    TARGET = $${ARCHIVE}
    QMAKE_CXXFLAGS += -Wno-padded

    QMAKE_EXTRA_TARGETS += distclean testclean
    distclean.depends += testclean
    testclean.commands = rm -f $${PWD}/testdata/*.db $${PWD}/testdata/*.cnf $${PWD}/etc/$${ARCHIVE}

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
}

linux {
    !CONFIG(no-systemd) {
        CONFIG += link_pkgconfig
        PKGCONFIG += libsystemd
        DEFINES += UNISTD_SYSTEMD
    }
}

macx {  
    # check for homebrew or macports...
    equals(PREFIX, "/usr/local")|equals(PREFIX, "/opt/local") {
        CONFIG -= app_bundle
        CONFIG += make-install make-defines
    }
    else {
        QMAKE_MACOSX_DEPLOYMENT_TARGET=10.10
        TARGET = $${PRODUCT}
        CONFIG += make-defines
        PREFIX = /usr
        VARPATH=/var/lib
        LOGPATH=/var/log
        ETCPATH=/etc
        system(rm -rf $${OUT_PWD}/$${TARGET}.app)
    }
}

win32-msvc* {
    TARGET = $${PRODUCT}
    QMAKE_CXXFLAGS_RELEASE += /Zi
    QMAKE_LFLAGS_RELEASE += /debug
    CONFIG -= debug_and_release debug_and_release_target
    CONFIG += skip_target_version_ext
    DEFINES += WIN32_LEAN_AND_MEAN
    PROJECT_PREFIX = C:/ProgramData/TychoSoftworks/$${PRODUCT}
}

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

        CONFIG -= make-install make-defines
        DEFINES += DEBUG_TESTDATA
    }
}

DEFINES += \
    PROJECT_VERSION=\"$${VERSION}\" \
    PROJECT_COPYRIGHT=\"$${COPYRIGHT}\" \
    PROJECT_NAME=\"$${ARCHIVE}\" \

isEmpty(PROJECT_PREFIX):OPENSSL_PREFIX=$${VARPATH}/$${ARCHIVE}
else {
    CONFIG -= make-defines
    DEFINES += PROJECT_PREFIX=$${PROJECT_PREFIX}
    OPENSSL_PREFIX = $$shell_path($${PROJECT_PREFIX})
}

message(OpenSSL Certificates $${OPENSSL_PREFIX})

# primary app support

exists(Bootstrap/Bootstrap.pri):\
    include(Bootstrap/Bootstrap.pri)
exists(Archive/Archive.pri):\
    include(Archive/Archive.pri)

include(src/Common.pri)
include(src/Database.pri)
include(src/Main.pri)
include(src/Stack.pri)

# unix filesystem hierarchy defines

make-defines {
    DEFINES += \
        UNISTD_PREFIX=$${PREFIX} \
        UNISTD_ETCPATH=$${ETCPATH} \
        UNISTD_VARPATH=$${VARPATH} \
        UNISTD_LOGPATH=$${LOGPATH} \
}

# generic install, windows and mac may use archive instead...

make-install {
    DEFINES += \
        UNISTD_PREFIX=$${PREFIX} \
        UNISTD_ETCPATH=$${ETCPATH} \
        UNISTD_VARPATH=$${VARPATH} \
        UNISTD_LOGPATH=$${LOGPATH} \

    INSTALLS += target config

    config.path = $${ETCPATH}
    config.files = etc/sipwitchqt.conf
    config.depends = target

    target.path = $${PREFIX}/sbin
    target.depends = all
}

# other files...

OTHER_FILES += \
    etc/sipwitchqt.conf \
    etc/README.md \
    testdata/service.conf \
    README.md \
    CONTRIBUTING.md \
    LICENSE \
    Doxyfile \
    sipwitchqt.spec \
    qrc/README.md \

QMAKE_TARGET_COMPANY = "Tycho Softworks"
QMAKE_TARGET_COPYRIGHT = "$${COPYRIGHT} Tycho Softworks"
QMAKE_TARGET_PRODUCT = "$${PRODUCT}"
QMAKE_TARGET_DESCRIPTION = "Tycho SIP Witch Service"
