QT += core dbus
QT -= gui

greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent

TARGET = ctp_executer
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ctp_executer.cpp \
    trade_handler.cpp \
    order.cpp

HEADERS += \
    ctp_executer.h \
    ThostFtdcTraderApi.h \
    ThostFtdcUserApiDataType.h \
    ThostFtdcUserApiStruct.h \
    trade_handler.h \
    order.h

DBUS_ADAPTORS += ctp_executer.xml

DISTFILES +=

unix:LIBS += "$$_PRO_FILE_PWD_/thosttraderapi.so"
win32:LIBS += "$$_PRO_FILE_PWD_/thosttraderapi.lib"

unix:QMAKE_CXXFLAGS += -std=c++11
