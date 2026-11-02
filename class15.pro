#-------------------------------------------------
#
# Project created by QtCreator 2026-10-27T11:52:35
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = class15
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

INCLUDEPATH += $$PWD/util

SOURCES += \
    model/recordmodel.cpp \
        main.cpp \
    util/dbconn.cpp \
    view/datapicker.cpp \
    view/loginwidget.cpp \
    view/mainwidget.cpp \
    view/monitorwidget.cpp \
    view/registerwidget.cpp \
    view/reviewwidget.cpp \
    view/verifycodelabel.cpp \
    model/usermodel.cpp \
    service/userservice.cpp \
    util/winmanager.cpp




HEADERS += \
    model/recordmodel.h \
    service/recordservice.h \
    util/dbconn.h \
    view/datapicker.h \
    view/loginwidget.h \
    view/mainwidget.h \
    view/monitorwidget.h \
    view/registerwidget.h \
    view/reviewwidget.h \
    view/verifycodelabel.h \
    model/usermodel.h \
    service/userservice.h \
    util/winmanager.h




# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DESTDIR = $$PWD/bin

RESOURCES += \
    resources.qrc
SOURCES += util/devdbconnection.cpp
HEADERS += util/devdbconnection.h
SOURCES += model/eventrepository.cpp
HEADERS += model/eventrepository.h service/eventservice.h
SOURCES += service/eventquerytask.cpp
HEADERS += service/eventquerytask.h
