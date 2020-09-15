#-------------------------------------------------
#
# Project created by QtCreator 2015-05-22T19:37:35
#
#-------------------------------------------------

QT       += core gui concurrent axcontainer

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TCeMSComp
TEMPLATE = app


SOURCES += main.cpp\
    rootwindow.cpp \
    ebomcompareview.cpp \
    node.cpp

HEADERS  += \
    rootwindow.h \
    ebomcompareview.h \
    node.h

FORMS    += \
    rootwindow.ui \
    ebomcompareview.ui

RESOURCES += \
    resources.qrc

RC_ICONS = icon.ico

QMAKE_TARGET_DESCRIPTION = "CC import delta config file generator. For use with eMS 14.1 and TCe 11.5"

VERSION = 1.6.0.0
