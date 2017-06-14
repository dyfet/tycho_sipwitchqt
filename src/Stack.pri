# VoIP stack components that interface directly with eXosip

HEADERS += \
    $${PWD}/context.hpp \
    $${PWD}/stack.hpp \
    $${PWD}/subnet.hpp \
    $${PWD}/address.hpp \
    $${PWD}/endpoint.hpp \

SOURCES += \
    $${PWD}/context.cpp \
    $${PWD}/stack.cpp \
    $${PWD}/subnet.cpp \
    $${PWD}/address.cpp \
    $${PWD}/endpoint.cpp \

LIBS += -leXosip2 -losip2 -losipparser2

macx: LIBS += -lcares -framework Security -framework CoreServices -framework CoreFoundation
unix: LIBS += -lresolv -lssl -lcrypto
