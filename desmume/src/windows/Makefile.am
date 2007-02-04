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
        ConfigKeys.c ConfigKeys.h
desmume_LDADD = ../libdesmume.a yopyop_private.o
yopyop_private.o: yopyop_private.rc resources.rc resource.h
	windres yopyop_private.rc -o yopyop_private.o
