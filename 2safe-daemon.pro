QT       += core
QT       -= gui

TARGET = 2safe-daemon
CONFIG   += console
CONFIG   += c++11
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    safedaemon.cpp \
    safeservice.cpp

include(qt-solutions/qtservice/src/qtservice.pri)

HEADERS += \
    safedaemon.h \
    safeservice.h
