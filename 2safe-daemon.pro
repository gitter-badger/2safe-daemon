QT       += core
QT       += sql
QT       -= gui

TARGET = 2safe-daemon
CONFIG   += console
CONFIG   += c++11
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    safedaemon.cpp \
    safefilesystem.cpp \
    safeapifactory.cpp

include(lib2safe/safe.pri)

HEADERS += \
    safedaemon.h \
    safefilesystem.h \
    safecommon.h \
    safeapifactory.h
