# VoIP stack components that interface directly with eXosip

HEADERS += \
    $${PWD}/context.hpp \
    $${PWD}/stack.hpp \
    $${PWD}/subnet.hpp \
    $${PWD}/address.hpp \
    $${PWD}/registry.hpp \
    $${PWD}/provider.hpp \
    $${PWD}/call.hpp \

SOURCES += \
    $${PWD}/context.cpp \
    $${PWD}/stack.cpp \
    $${PWD}/subnet.cpp \
    $${PWD}/address.cpp \
    $${PWD}/endpoint.cpp \
    $${PWD}/registry.cpp \
    $${PWD}/provider.cpp \
    $${PWD}/segment.cpp \
    $${PWD}/call.cpp \

LIBS += -leXosip2 -losip2 -losipparser2

# required osx homebrew dependencies
macx {
    !exists(/usr/local/opt/libexosip):error(*** brew install libexosip)

    INCLUDEPATH += /usr/local/opt/libosip/include /usr/local/opt/libexosip/include
    LIBS += -L/usr/local/opt/libosip/lib -L/usr/local/opt/libexosip/lib
}
