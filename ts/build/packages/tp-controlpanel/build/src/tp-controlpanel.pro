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
PANEL    = $$PWD/../src-panel
INCLUDEPATH += $$REGISTRY $$PANEL

# The form renderer is shared with tp-panel. ThinPro's control panel hosts
# each setting inside its own window rather than launching one, so the window
# needs the same widget the standalone program draws.
SOURCES += main.cpp $$REGISTRY/registry.cpp $$REGISTRY/tpstyle.cpp \
           $$PANEL/panelform.cpp panelentry.cpp cpwindow.cpp
HEADERS += $$REGISTRY/registry.h $$REGISTRY/tpstyle.h \
           $$PANEL/panelform.h panelentry.h cpwindow.h
