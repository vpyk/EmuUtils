TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    bin2tape.cpp

HEADERS += \
    bin2tape.h

QMAKE_LFLAGS += -static -static-libgcc
