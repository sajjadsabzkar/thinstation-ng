QT       += core gui widgets
CONFIG   += c++11 warn_on
CONFIG   -= debug_and_release

TARGET   = tp-taskbar
TEMPLATE = app

QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_LFLAGS_RELEASE   += -Wl,--as-needed -Wl,-O1

REGISTRY = $$PWD/../../../tp-registry/build/src
INCLUDEPATH += $$REGISTRY

SOURCES += main.cpp $$REGISTRY/registry.cpp $$REGISTRY/tpstyle.cpp taskbar.cpp
HEADERS += $$REGISTRY/registry.h $$REGISTRY/tpstyle.h taskbar.h
