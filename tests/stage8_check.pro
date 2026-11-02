QT += core gui widgets
CONFIG += console c++11
TEMPLATE = app
TARGET = stage8_check
DESTDIR = ../bin
INCLUDEPATH += ..
SOURCES += stage8_check.cpp ../player/ffmpegplayer.cpp ../view/monitorwidget.cpp
HEADERS += ../player/videoplayer.h ../player/ffmpegplayer.h ../view/monitorwidget.h
RESOURCES += ../resources.qrc

greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
    DESTDIR = ../bin-qt6
}
