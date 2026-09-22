QT       += core gui widgets
CONFIG   += c++11 warn_on
CONFIG   -= debug_and_release

TARGET   = tp-connmgr
TEMPLATE = app

# Keep the binary small: this runs on clients with 1 GB of RAM.
QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_LFLAGS_RELEASE   += -Wl,--as-needed -Wl,-O1

SOURCES += main.cpp registry.cpp model.cpp mainwindow.cpp editdialog.cpp
HEADERS += registry.h model.h mainwindow.h editdialog.h
