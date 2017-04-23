# Common service code, may become stand-alone ServiceQt library in the future

HEADERS += \
    $${PWD}/compiler.hpp \
    $${PWD}/inline.hpp \
    $${PWD}/args.hpp \
    $${PWD}/util.hpp \
    $${PWD}/control.hpp \
    $${PWD}/logging.hpp \
    $${PWD}/server.hpp \
    $${PWD}/system.hpp \
    $${PWD}/crashhandler.hpp \

SOURCES += \
    $${PWD}/util.cpp \
    $${PWD}/control.cpp \
    $${PWD}/logging.cpp \
    $${PWD}/server.cpp \
    $${PWD}/system.cpp \
    $${PWD}/args.cpp \
    $${PWD}/crashhandler.cpp \
