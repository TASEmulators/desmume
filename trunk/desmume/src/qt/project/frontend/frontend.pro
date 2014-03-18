#-------------------------------------------------
#
# Project created by QtCreator 2014-03-08T21:04:19
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += multimedia

TARGET = desmume-qt
TEMPLATE = app

DEFINES += DESMUME_QT

win32 {
	!win32-g++: DEFINES += NOMINMAX \
		_CRT_SECURE_NO_DEPRECATE \
		WIN32 \
		HAVE_LIBZ

	DEFINES -= UNICODE

	RC_ICONS = resources/DeSmuME.ico

	# Windows glib
	contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += -L$$PWD/../../../windows/.libs/ -lglib-vc8-x64
	} else {
		LIBS += -L$$PWD/../../../windows/.libs/ -lglib-vc8-Win32
	}
	LIBS += -lole32 -lshell32 -ladvapi32
	INCLUDEPATH += $$PWD/../../../windows/glib-2.20.1/build/glib $$PWD/../../../windows/glib-2.20.1/build
	DEPENDPATH += $$PWD/../../../windows/glib-2.20.1/build/glib

	# Windows zlib
	contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += -L$$PWD/../../../windows/zlib123/ -lzlib-vc8-x64
	} else {
		LIBS += -L$$PWD/../../../windows/zlib123/ -lzlib-vc8-Win32
	}
	INCLUDEPATH += $$PWD/../../../windows/zlib123
	DEPENDPATH += $$PWD/../../../windows/zlib123

	# Windows libagg
	contains(QMAKE_TARGET.arch, x86_64) {
		LIBS += -L$$PWD/../../../windows/agg/ -lagg-2.5-x64
	} else {
		LIBS += -L$$PWD/../../../windows/agg/ -lagg-2.5
	}
	INCLUDEPATH += $$PWD/../../../windows/agg/include
	DEPENDPATH += $$PWD/../../../windows/agg/include
} else:unix:!macx {
	DESMUME_ARCH = $$QMAKE_HOST.arch
	linux-*-64: DESMUME_ARCH = x86_64
	linux-*-32: DESMUME_ARCH = x86

	# Note: If you plan to distribute the binary, remove -march=native
	QMAKE_CXXFLAGS_RELEASE += -O3 -flto=4 -fuse-linker-plugin -funroll-loops -march=native -minline-all-stringops
	QMAKE_LFLAGS_RELEASE += -O3 -flto=4 -fuse-linker-plugin -funroll-loops -march=native -minline-all-stringops
} else:macx {
	error("Mac OS X not supported")
}

INCLUDEPATH += ./

SOURCES += main.cpp\
    ui/mainwindow.cpp \
    ui/screenwidget.cpp \
    mainloop.cpp \
    sndqt.cpp \
    ds.cpp \
    video.cpp \
    ui/controlconfigdialog.cpp \
    ds_input.cpp \
    keyboardinput.cpp

HEADERS  += ui/mainwindow.h \
    ui/screenwidget.h \
    mainloop.h \
    sndqt.h \
    ds.h \
    video.h \
    ui/controlconfigdialog.h \
    keyboardinput.h \
    ds_input.h

RESOURCES += \
    resources/resources.qrc

FORMS    += ui/mainwindow.ui \
    ui/controlconfigdialog.ui

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../core/release/ -ldesmume
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../core/debug/ -ldesmume
else:unix: LIBS += -L$$OUT_PWD/../core/ -ldesmume

INCLUDEPATH += $$PWD/../../..
DEPENDPATH += $$PWD/../../..

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/release/libdesmume.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/debug/libdesmume.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/release/desmume.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/debug/desmume.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../core/libdesmume.a

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += glib-2.0 libagg zlib
