PROJECT_ROOT = $$clean_path($$PWD/..)
include(../class15.pro)
SOURCES -= main.cpp
for(source, SOURCES): FIX_SOURCES += $$absolute_path($$source, $$PROJECT_ROOT)
SOURCES = $$FIX_SOURCES $$PWD/ui_playback_check.cpp
for(header, HEADERS): FIX_HEADERS += $$absolute_path($$header, $$PROJECT_ROOT)
HEADERS = $$FIX_HEADERS
RESOURCES = $$PROJECT_ROOT/resources.qrc
INCLUDEPATH += $$PROJECT_ROOT
TARGET = ui_playback_check
DESTDIR = $$OUT_PWD/../bin-qt6
CONFIG += console
