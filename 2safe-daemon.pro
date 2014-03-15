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
    safeapifactory.cpp \
    fswatcher.cpp \
    saferpcqueue.cpp

include(lib2safe/safe.pri)

HEADERS += \
    safedaemon.h \
    safecommon.h \
    safeapifactory.h \
    fswatcher.h \
    saferpcqueue.h

LIBS = -linotifytools
