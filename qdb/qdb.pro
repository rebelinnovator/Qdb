QT -= gui
QT += dbus network

CONFIG += c++11

TARGET = qdb
win32: CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

include($$PWD/../libusb_setup.pri)
include($$PWD/../version.pri)

HEADERS += \
    client/client.h \
    hostmessages.h \
    server/connection.h \
    server/deviceinformationfetcher.h \
    server/devicemanager.h \
    server/echoservice.h \
    server/handshakeservice.h \
    server/hostserver.h \
    server/hostservlet.h \
    server/logging.h \
    server/networkmanagercontrol.h \
    server/service.h \
    server/usb-host/usbcommon.h \
    server/usb-host/usbconnection.h \
    server/usb-host/usbconnectionreader.h \
    server/usb-host/usbdeviceenumerator.h \
    server/usb-host/usbdevice.h \

SOURCES += \
    client/client.cpp \
    hostmessages.cpp \
    main.cpp \
    server/connection.cpp \
    server/deviceinformationfetcher.cpp \
    server/devicemanager.cpp \
    server/echoservice.cpp \
    server/handshakeservice.cpp \
    server/hostserver.cpp \
    server/hostservlet.cpp \
    server/logging.cpp \
    server/networkmanagercontrol.cpp \
    server/service.cpp \
    server/usb-host/libusbcontext.cpp \
    server/usb-host/usbconnection.cpp \
    server/usb-host/usbconnectionreader.cpp \
    server/usb-host/usbdevice.cpp \
    server/usb-host/usbdeviceenumerator.cpp \

INCLUDEPATH += $$PWD/../

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target

unix {
    LIBS += -L$$OUT_PWD/../libqdb -lqdb
}

win32 {
    CONFIG(debug, debug|release) {
        LIBS += -L$$OUT_PWD/../libqdb/debug
    } else {
        LIBS += -L$$OUT_PWD/../libqdb/release
    }
    LIBS += -lqdb
}
