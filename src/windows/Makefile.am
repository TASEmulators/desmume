include $(top_srcdir)/src/desmume.mk

EXTRA_DIST = DeSmuME.ico \
	 Makefile.win \
	resources.rc
bin_PROGRAMS = desmume
desmume_SOURCES = \
	afxres.h aviout.h config.h \
        CWindow.cpp CWindow.h \
        disView.cpp disView.h ginfo.cpp ginfo.h \
        IORegView.cpp IORegView.h \
        main.cpp main.h mapView.cpp mapView.h memView.cpp memView.h \
        oamView.cpp oamView.h palView.cpp palView.h \
        resource.h \
        snddx.cpp snddx.h \
        tileView.cpp tileView.h \
	FirmConfig.cpp FirmConfig.h \
	AboutBox.cpp AboutBox.h \
	colorctrl.cpp colorctrl.h \
	lightView.cpp lightView.h \
	matrixView.cpp matrixView.h \
	aviout.cpp console.cpp console.h \
	inputdx.cpp inputdx.h ogl.cpp \
	throttle.cpp throttle.h windriver.h \
	cheatsWin.cpp cheatsWin.h gbaslot_config.cpp gbaslot_config.h \
	mic.cpp hotkey.cpp hotkey.h \
	ram_search.cpp ram_search.h ramwatch.cpp ramwatch.h \
	replay.cpp replay.h soundView.cpp soundView.h \
	DeSmuME_2005.vcproj DeSmuME_2008.vcproj \
	DeSmuME_Intel.icproj DeSmuME_Intel.vcproj DeSmuME_Intel.sln \
	DeSmuME_x64.manifest DeSmuME_x86.manifest
desmume_LDADD = ../libdesmume.a yopyop_private.o
if HAVE_GDB_STUB
desmume_LDADD += ../gdbstub/libgdbstub.a
endif
yopyop_private.o: resources.rc resource.h
	windres resources.rc -o resources.o
