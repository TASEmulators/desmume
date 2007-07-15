include $(top_srcdir)/src/desmume.mk

localedir = $(datadir)/locale

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTKGLEXT_CFLAGS) $(LIBGLADE_CFLAGS) \
		$(GTHREAD_CFLAGS) -DDATADIR=\"$(gladedir)/\" \
		-DLOCALEDIR=\"$(localedir)\"
AM_LDFLAGS += -rdynamic $(INTLLIBS)

EXTRA_DIST = DeSmuME.dev $(glade_DATA)

Applicationsdir = $(datadir)/applications
Applications_DATA = desmume-glade.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = glade/DeSmuME.xpm
gladedir = $(datadir)/desmume/glade
glade_DATA = glade/DeSmuMe_Dtools.glade glade/DeSmuMe.glade glade/DeSmuME.xpm

bin_PROGRAMS = desmume-glade
desmume_glade_SOURCES =  \
	globals.h glade-xml.c \
	callbacks.c callbacks.h callbacks_IO.c callbacks_IO.h \
	desmume.c desmume.h \
	keyval_names.c main.c printscreen.c gdk_gl.c gdk_gl.h \
	dTools/callbacks_1_ioregs.c dTools/callbacks_2_memview.c \
	dTools/callbacks_3_palview.c  dTools/callbacks_4_tileview.c \
	dTools/callbacks_dtools.h \
	dTools/dTools_display.h \
	gdk_3Demu.c gdk_3Demu.h \
	../sndsdl.h ../sndsdl.c ../ctrlssdl.h ../ctrlssdl.c 

desmume_glade_LDADD = ../libdesmume.a ../gdbstub/libgdbstub.a \
			$(SDL_LIBS) $(GTKGLEXT_LIBS) $(LIBGLADE_LIBS) \
			$(GTHREAD_LIBS)
