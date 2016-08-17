SUBDIRS = doc
include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTKGLEXT_CFLAGS) $(LIBGLADE_CFLAGS) \
		$(GTHREAD_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS) \
		$(LIBSOUNDTOUCH_CFLAGS) \
		-DDATADIR=\"$(gladedir)/\" -DLOCALEDIR=\"$(localedir)\"
AM_LDFLAGS += -rdynamic $(INTLLIBS)

EXTRA_DIST = $(glade_DATA) desmume-glade.desktop

Applicationsdir = $(datadir)/applications
Applications_DATA = desmume-glade.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = glade/DeSmuME.xpm
gladedir = $(datadir)/desmume/glade
glade_DATA = glade/DeSmuMe_Dtools.glade glade/DeSmuMe.glade glade/DeSmuME.xpm

bin_PROGRAMS = desmume-glade
desmume_glade_SOURCES =  \
	globals.h glade-xml.cpp \
	callbacks.cpp callbacks.h callbacks_IO.cpp callbacks_IO.h \
	desmume.cpp desmume.h \
	keyval_names.cpp keyval_names.h main.cpp gdk_gl.cpp gdk_gl.h \
	dTools/callbacks_1_ioregs.cpp dTools/callbacks_2_memview.cpp \
	dTools/callbacks_3_palview.cpp  dTools/callbacks_4_tileview.cpp \
	dTools/callbacks_dtools.h \
	dTools/dTools_display.h \
	gdk_3Demu.cpp gdk_3Demu.h \
	../sndsdl.h ../sndsdl.cpp ../ctrlssdl.h ../ctrlssdl.cpp \
	../driver.h ../driver.cpp

desmume_glade_LDADD = ../libdesmume.a \
			$(SDL_LIBS) $(GTKGLEXT_LIBS) $(LIBGLADE_LIBS) \
			$(GTHREAD_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) \
			$(LIBSOUNDTOUCH_LIBS)
if HAVE_GDB_STUB
desmume_glade_LDADD += ../gdbstub/libgdbstub.a
endif
