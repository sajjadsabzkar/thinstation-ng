QT       += core gui widgets
CONFIG   += c++11 warn_on
CONFIG   -= debug_and_release

TARGET   = tp-controlpanel
TEMPLATE = app

# Same budget as the connection manager: this has to be comfortable on a
# 1 GB DDR2 client.
QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_LFLAGS_RELEASE   += -Wl,--as-needed -Wl,-O1

REGISTRY = $$PWD/../../../tp-registry/build/src
INCLUDEPATH += $$REGISTRY

SOURCES += main.cpp $$REGISTRY/registry.cpp $$REGISTRY/tpstyle.cpp panelentry.cpp cpwindow.cpp
HEADERS += $$REGISTRY/registry.h $$REGISTRY/tpstyle.h panelentry.h cpwindow.h
