include $(top_srcdir)/src/desmume.mk

EXTRA_DIST = DeSmuME.dev DeSmuME.ico \
	 DeSmuME.layout Makefile.win \
	resources.rc
bin_PROGRAMS = desmume
desmume_SOURCES = \
        CWindow.cpp CWindow.h \
        disView.cpp disView.h ginfo.cpp ginfo.h \
        IORegView.cpp IORegView.h \
        main.cpp mapView.cpp mapView.h memView.cpp memView.h \
        oamView.cpp oamView.h palView.cpp palView.h \
        resource.h \
        snddx.cpp snddx.h \
        tileView.cpp tileView.h \
	FirmConfig.cpp FirmConfig.h \
	AboutBox.cpp AboutBox.h \
	colorctrl.cpp colorctrl.h \
	lightView.cpp lightView.h \
	matrixView.cpp matrixView.h
desmume_LDADD = ../libdesmume.a yopyop_private.o
if HAVE_GDB_STUB
desmume_LDADD += ../gdbstub/libgdbstub.a
endif
yopyop_private.o: resources.rc resource.h
	windres resources.rc -o resources.o
