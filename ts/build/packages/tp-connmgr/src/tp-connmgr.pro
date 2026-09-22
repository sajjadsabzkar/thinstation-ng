QT       += core gui widgets
CONFIG   += c++11 warn_on
CONFIG   -= debug_and_release

TARGET   = tp-connmgr
TEMPLATE = app

# Keep the binary small: this runs on clients with 1 GB of RAM.
QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_LFLAGS_RELEASE   += -Wl,--as-needed -Wl,-O1

# The registry lives in the tp-registry package, which owns both this C++
# implementation and the tpreg shell tool that reads the same two files.
REGISTRY = $$PWD/../../tp-registry/src
INCLUDEPATH += $$REGISTRY

SOURCES += main.cpp $$REGISTRY/registry.cpp model.cpp mainwindow.cpp editdialog.cpp
HEADERS += $$REGISTRY/registry.h model.h mainwindow.h editdialog.h
