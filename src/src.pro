TEMPLATE = lib
TARGET = obexdbus
CONFIG += plugin hide_symbols

QT += core
QT -= gui

CONFIG += link_pkgconfig
LIBS += -lqmfmessageserver5 -lqmfclient5
PKGCONFIG += qmfclient5 qmfmessageserver5
# accounts-qt5

OTHER_FILES += rpm/qmf-obex-plugin.spec

target.path = $$QMF_INSTALL_ROOT/lib/qmf/plugins5/messageserverplugins

INSTALLS += target

HEADERS += \
    obexdbusplugin.h \
    obexdbusinterface.h

SOURCES += \
    obexdbusplugin.cpp \
    obexdbusinterface.cpp

INCLUDEPATH += /home/ruff/co/messagingframework/qmf/src/libraries/qmfclient
