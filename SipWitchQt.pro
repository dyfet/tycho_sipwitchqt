PRODUCT = SipWitchQt
TEMPLATE = app
VERSION = 0.1.0
COPYRIGHT = 2017
ARCHIVE = sipwitchqt
TARGET = $${ARCHIVE}

# basic compile and link config
CONFIG += c++11 console
CONFIG -= app_bundle
QT -= gui
QT += network sql
QMAKE_CXXFLAGS += -Wno-padded

exists(Custom.pri):include(Custom.pri)

# build type specific options
CONFIG(release,release|debug):DEFINES += QT_NO_DEBUG_OUTPUT QT_NO_DEBUG
else {
    # the debug build target must be in etc for most python utils to work fully
    DEFINES += DEBUG_LOGGING
    !CONFIG(no-testdata) {
        CONFIG(userdata):PROJECT_PREFIX=\"$${PWD}/userdata\"
        else:PROJECT_PREFIX=\"$${PWD}/testdata\"
    }
}

# platform specific options
linux {
    !CONFIG(no-systemd) {
        CONFIG += link_pkgconfig
        PKGCONFIG += libsystemd
        DEFINES += UNISTD_SYSTEMD
    }
}

# prefix options
macx:CONFIG += std_prefix                           # brew assumes /usr/local...
isEmpty(PREFIX) {                                   # find prefix if not set...
    PREFIX=$$[QT_INSTALL_PREFIX]                    # qt prefix is a default...
    CONFIG(sys_prefix):PREFIX="/usr"                # system daemon...
    else:CONFIG(pkg_prefix):PREFIX="/usr/pkg"       # bsd package prefix...
    else:CONFIG(std_prefix):PREFIX="/usr/local"     # generic unix install...
}

# system prefix will use system directories
CONFIG(opt_prefix) {
    isEmpty(VENDOR):VENDOR="tychosoft"
    PREFIX="/opt/$${VENDOR}"
    VARBASE="/var/opt/$${VENDOR}"
    ETCPATH="/etc/opt/$${VENDOR}"
} else:equals(PREFIX, "/usr") {
    VARBASE="/var"
    ETCPATH=/etc
}
else {
    macx:VARBASE="$${PREFIX}/var"
    else:VARBASE="/var"
    ETCPATH=$${PREFIX}/etc
}
VARPATH="$${VARBASE}/sipwitchqt"
LOGPATH="$${VARBASE}/log"

#message("Installation Prefix: $${PREFIX}")
#message("Installation Var:    $${VARBASE}")
#message("Installation Etc:    $${ETCPATH}")

# global platform options
win32-msvc*:error(*** windows not supported...)
system(rm -f "$${OUT_PWD}/$${TARGET}")

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
OBJECTS_DIR = objects
RCC_DIR = generated
MOC_DIR = generated

HEADERS += \
    $$files(Common/*.hpp) \
    $$files(Database/*.hpp) \
    $$files(Server/*.hpp) \

SOURCES += \
    $$files(Common/*.cpp) \
    $$files(Database/*.cpp) \
    $$files(Server/*.cpp) \

macx: LIBS += -framework CoreFoundation
CONFIG(release, release|debug):unix:!macx:LIBS += -ltcmalloc_minimal

LIBS += -leXosip2 -losip2 -losipparser2

# required osx homebrew dependencies
macx {
    !exists(/usr/local/opt/libexosip):error(*** brew install libexosip)

    INCLUDEPATH += /usr/local/opt/libosip/include /usr/local/opt/libexosip/include
    LIBS += -L/usr/local/opt/libosip/lib -L/usr/local/opt/libexosip/lib
}

# extra install targets, release only
CONFIG(release, release|debug) {
    INSTALLS += target config runit

    config.path = "$${ETCPATH}"
    config.files = etc/sipwitchqt.conf
    config.depends = target

    runit.path = "$${ETCPATH}/sv/sipwitchqt"
    runit.files = etc/run
    runit.depends = config
    runit.commands += ln -s "$${PREFIX}/sbin/${TARGET}" "$${PREFIX}/sbin/${TARGET}-daemon"

    target.path = "$${PREFIX}/sbin"
    target.depends = all
}

# publish support
QMAKE_EXTRA_TARGETS += source
source.commands += $$QMAKE_DEL_FILE *.tar.gz &&
source.commands += cd $${PWD} &&
source.commands += git archive --output="$${OUT_PWD}/$${ARCHIVE}-$${VERSION}.tar.gz" --format tar.gz  --prefix=$${ARCHIVE}-$${VERSION}/ v$${VERSION} ||
source.commands += git archive --output="$${OUT_PWD}/$${ARCHIVE}-$${VERSION}.tar.gz" --format tar.gz  --prefix=$${ARCHIVE}-$${VERSION}/ HEAD

linux:exists("/usr/bin/rpmbuild") {
    QMAKE_EXTRA_TARGETS += srpm
    srpm.depends = source
    srpm.commands += && rm -f *.src.rpm && rpmbuild --define \"_tmppath /tmp\" --define \"_sourcedir .\" --define \"_srcrpmdir .\" --nodeps -bs $${ARCHIVE}.spec
}

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

# clean additional testing files on distclean...
QMAKE_EXTRA_TARGETS += distclean publishclean
distclean.depends += publishclean
publishclean.commands += cd $${PWD} && rm -rf testdata/*.db etc/$${ARCHIVE} testdata/certs testdata/private $testdata/*.log &&
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
    DOCKER.md \
    TODO.md \
    LICENSE \
    Doxyfile \
    sipwitchqt.spec \

# common target properties
QMAKE_TARGET_COMPANY = "Tycho Softworks"
QMAKE_TARGET_COPYRIGHT = "$${COPYRIGHT} Tycho Softworks"
QMAKE_TARGET_PRODUCT = "$${PRODUCT}"
QMAKE_TARGET_DESCRIPTION = "Tycho SIP Witch Service"
