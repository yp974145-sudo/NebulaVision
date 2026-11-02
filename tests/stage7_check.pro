QT += core gui widgets
CONFIG += console c++11
TEMPLATE = app
TARGET = stage7_check
DESTDIR = ../bin
INCLUDEPATH += ..
SOURCES += stage7_check.cpp ../player/ffmpegplayer.cpp
HEADERS += ../player/videoplayer.h ../player/ffmpegplayer.h

