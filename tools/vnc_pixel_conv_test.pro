# Regression test for the loongarch64 VNC 16bpp -> 32bpp pixel conversion.
# Build with the target Qt qmake:
#   qtopiacore/target/bin/qmake tests/vnc_pixel_conv/vnc_pixel_conv.pro \
#       -spec qtopiacore/qt/mkspecs/qws/linux-loongarch64-g++ \
#       QTTREE=<abs path to qt-extended-4.4.3/qtopiacore/qt>
#   make && ./vnc_pixel_conv

TEMPLATE = app
TARGET = vnc_pixel_conv
CONFIG += console
CONFIG -= app_bundle
QT += core gui network

# qscreenvnc_p.h and qscreenproxy_p.h are private QtGui headers.
INCLUDEPATH += $$QTTREE/src/gui/embedded

SOURCES += main.cpp
