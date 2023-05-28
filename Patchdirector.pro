#-------------------------------------------------
#
# Project created by QtCreator 2016-09-15T22:21:45
#
#-------------------------------------------------

QT       += core gui sql multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Patchdirector
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
        editordialog.cpp \
    audiorecorder.cpp \
    recpresetsmodel.cpp \
    zipfile.cpp \
    licensedialog.cpp \
    logdialog.cpp

HEADERS  += mainwindow.h \
        editordialog.h \
    audiorecorder.h \
    recpresetsmodel.h \
    zipfile.h \
    licensedialog.h \
    logdialog.h

FORMS    += mainwindow.ui \
        editordialog.ui \
    licensedialog.ui

TRANSLATIONS = translations/patchdirector_de.ts

RESOURCES += \
    patchdirector.qrc

macx: ICON = Patchdirector.icns
win32: RC_FILE = Patchdirector.rc

win32 {
    SOURCES += wasapiloopback.cpp \
        wasapi-loopback/guid.cpp \
        wasapi-loopback/loopback-capture.cpp \
        wasapi-loopback/prefs.cpp

    HEADERS += wasapiloopback.h \
        wasapi-loopback/cleanup.h \
        wasapi-loopback/common.h \
        wasapi-loopback/log.h \
        wasapi-loopback/loopback-capture.h \
        wasapi-loopback/prefs.h

    LIBS += avrt.lib ole32.lib $$PWD/soxr/libsoxr.lib
}

INCLUDEPATH += zip
SOURCES += zip/zip.c zip/unzip.c zip/ioapi.c
macx {
    LIBS += $$PWD/zip/libz.a
    DEFINES += HAVE_UNISTD_H HAVE_STDARG_H
}
win32 {
    CONFIG(debug, debug|release) {
        LIBS += $$PWD/zip/zlibd.lib
    } else {
        LIBS += $$PWD/zip/zlib.lib
    }
}

macx: LIBS += -framework CoreServices -framework CoreAudio $$PWD/lame/libmp3lame.0.dylib
win32: LIBS += $$PWD/lame/libmp3lame.lib

DISTFILES += \
    translations/patchdirector_de.ts
