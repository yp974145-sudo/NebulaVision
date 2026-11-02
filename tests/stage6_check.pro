QT += core gui widgets
CONFIG += console c++11
TEMPLATE = app
TARGET = stage6_check
DESTDIR = ../bin
INCLUDEPATH += ..
SOURCES += stage6_check.cpp ../player/ffmpegplayer.cpp
HEADERS += ../player/videoplayer.h ../player/ffmpegplayer.h

greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
    DESTDIR = ../bin-qt6
}
