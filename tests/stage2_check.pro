QT += core gui widgets sql
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = stage2_check
DESTDIR = ../bin
INCLUDEPATH += ..
SOURCES += stage2_check.cpp ../view/reviewwidget.cpp ../view/datapicker.cpp ../model/recordmodel.cpp ../model/eventrepository.cpp ../util/dbconn.cpp ../util/devdbconnection.cpp
HEADERS += ../view/reviewwidget.h ../view/datapicker.h ../model/eventrepository.h ../util/devdbconnection.h

SOURCES += ../service/eventquerytask.cpp
HEADERS += ../service/eventquerytask.h

greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
    DESTDIR = ../bin-qt6
}
