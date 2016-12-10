QT += core dbus
QT -= gui

TARGET = ctp_executer
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ctp_executer.cpp \
    trade_handler.cpp

HEADERS += \
    ctp_executer.h \
    ThostFtdcTraderApi.h \
    ThostFtdcUserApiDataType.h \
    ThostFtdcUserApiStruct.h \
    trade_handler.h

DBUS_ADAPTORS += ctp_executer.xml

DISTFILES +=

unix:LIBS += "$$_PRO_FILE_PWD_/thosttraderapi.so"
win32:LIBS += "$$_PRO_FILE_PWD_/thosttraderapi.lib"
