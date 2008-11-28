include $(top_srcdir)/src/desmume.mk

EXTRA_DIST = DeSmuME.dev DeSmuME.exe.Manifest DeSmuME.ico \
	 DeSmuME.layout DeSmuME.vcproj Makefile.win \
	resources.rc yopyop_private.rc
bin_PROGRAMS = desmume
desmume_SOURCES = \
        CWindow.c CWindow.h \
        disView.c disView.h ginfo.c ginfo.h \
        IORegView.c IORegView.h \
        main.c mapView.c mapView.h memView.c memView.h \
        oamView.c oamView.h palView.c palView.h \
        resource.h \
        snddx.c snddx.h \
        tileView.c tileView.h yopyop_private.h \
        ConfigKeys.c ConfigKeys.h \
	FirmConfig.c FirmConfig.h \
	OGLRender.c OGLRender.h \
	AboutBox.c AboutBox.h \
	colorconv.h colorctrl.c colorctrl.h \
	lightView.c lightView.h \
	matrixView.c matrixView.h
desmume_LDADD = ../libdesmume.a yopyop_private.o
if HAVE_GDB_STUB
desmume_LDADD += ../gdbstub/libgdbstub.a
endif
yopyop_private.o: yopyop_private.rc resources.rc resource.h
	windres yopyop_private.rc -o yopyop_private.o
