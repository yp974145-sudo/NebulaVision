QT += core gui widgets multimedia multimediawidgets
CONFIG += console c++11
TEMPLATE = app
TARGET = stage5_check
DESTDIR = ../bin
INCLUDEPATH += ..
SOURCES += stage5_check.cpp ../player/qtvideoplayer.cpp
HEADERS += ../player/videoplayer.h ../player/qtvideoplayer.h

greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
    DESTDIR = ../bin-qt6
}
