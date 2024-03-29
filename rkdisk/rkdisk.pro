TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    rkdisk.cpp \
    rkimage/imagefile.cpp \
    rkimage/rkvolume.cpp \
    rkimage/volume.cpp

HEADERS += \
    rkimage/imagefile.h \
    rkimage/rkvolume.h \
    rkimage/volume.h

QMAKE_LFLAGS += -static -static-libgcc
