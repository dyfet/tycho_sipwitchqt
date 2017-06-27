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
!macx:CONFIG += app_install app_config

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
        CONFIG += app_install app_config
        INCLUDEPATH += $${PREFIX}/include
        LIBS += -L$${PREFIX}/lib
    }
    # else a bundled app with Qt runtime will be built
    else {
        system(rm -rf $${OUT_PWD}/$${TARGET}.app)
        TARGET = $${PRODUCT}
        CONFIG += app_config app_bundle
        PREFIX = /usr
        VARPATH=/var/lib
        LOGPATH=/var/log
        ETCPATH=/etc
    }
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

        CONFIG -= app_install app_config
        DEFINES += DEBUG_TESTDATA
    }
}

DEFINES += \
    PROJECT_VERSION=\"$${VERSION}\" \
    PROJECT_COPYRIGHT=\"$${COPYRIGHT}\" \
    PROJECT_NAME=\"$${ARCHIVE}\" \

isEmpty(PROJECT_PREFIX):OPENSSL_PREFIX=$${VARPATH}/$${ARCHIVE}
else {
    CONFIG -= app_config
    DEFINES += PROJECT_PREFIX=$${PROJECT_PREFIX}
    OPENSSL_PREFIX = $$shell_path($${PROJECT_PREFIX})
}

include(src/Common.pri)
include(src/Database.pri)
include(src/Main.pri)
include(src/Stack.pri)

# unix filesystem hierarchy defines

app_config {
    DEFINES += \
        UNISTD_PREFIX=$${PREFIX} \
        UNISTD_ETCPATH=$${ETCPATH} \
        UNISTD_VARPATH=$${VARPATH} \
        UNISTD_LOGPATH=$${LOGPATH} \
}

# generic install, windows and mac may use archive instead...

app_install {
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
macx:docs.commands += && cd doc/html && make docset
unix:docs.commands += && cd ../latex && make
unix:publish.depends += docs
unix:publish.commands += && cp $${OUT_PWD}/doc/latex/refman.pdf $${OUT_PWD}/$${ARCHIVE}-$${VERSION}.pdf

# binary packages, for macosx release builds with Qt bundled
macx:CONFIG(release, release|debug):CONFIG(app_bundle) {
    CONFIG += separate_debug_info force_debug_info
    QMAKE_EXTRA_TARGETS += archive publish_and_archive
    archive.depends = all
    archive.commands += $${PWD}/etc/archive.sh $${TARGET}
    publish_and_archive.depends = publish archive
}

# common extra cleanup
QMAKE_EXTRA_TARGETS += clean extra_clean
clean.depends += extra_clean
extra_clean.commands += rm -f $${ARCHIVE}
macx:extra_clean.commands += && rm -rf $${PRODUCT}.app $${PRODUCT}.app.dSYM 

# clean additional testing files on distclean...
QMAKE_EXTRA_TARGETS += distclean testclean publish_clean
distclean.depends += testclean publish_clean
testclean.commands = rm -rf $${PWD}/testdata/*.db $${PWD}/etc/$${ARCHIVE} $${PWD}/testdata/certs $${PWD}/testdata/private $${PWD}/testdata/*.log 
publish_clean.commands = rm -rf Archive $${ARCHIVE}-*.tar.gz $${ARCHIVE}-*.pdf $${ARCHIVE} doc Doxyfile.out

# other files...
OTHER_FILES += \
    etc/sipwitchqt.conf \
    etc/README.md \
    etc/archive.sh \
    testdata/service.conf \
    README.md \
    CONTRIBUTING.md \
    LICENSE \
    Doxyfile \
    sipwitchqt.spec \

QMAKE_TARGET_COMPANY = "Tycho Softworks"
QMAKE_TARGET_COPYRIGHT = "$${COPYRIGHT} Tycho Softworks"
QMAKE_TARGET_PRODUCT = "$${PRODUCT}"
QMAKE_TARGET_DESCRIPTION = "Tycho SIP Witch Service"
