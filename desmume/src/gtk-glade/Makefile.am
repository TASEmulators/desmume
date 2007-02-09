bin_PROGRAMS = desmume-glade
desmume_glade_SOURCES =  \
	globals.h glade-xml.c \
	callbacks.c callbacks.h callbacks_IO.c callbacks_IO.h desmume.c desmume.h \
	keyval_names.c main.c printscreen.c gdk_gl.c gdk_gl.h \
	dTools/callbacks_1_ioregs.c dTools/callbacks_2_memview.c \
	dTools/callbacks_3_palview.c dTools/callbacks_dtools.h \
	dTools/dTools_display.h \
	../sndsdl.h ../sndsdl.c ../ctrlssdl.h ../ctrlssdl.c 

gladedir = $(datadir)/desmume/glade
glade_DATA = glade/DeSmuMe_Dtools.glade glade/DeSmuMe.glade glade/DeSmuME.xpm

desmume_glade_LDADD = ../libdesmume.a $(SDL_LIBS) $(GDKGLEXT_LIBS) $(LIBGLADE_LIBS)
desmume_glade_CFLAGS = $(SDL_CFLAGS) $(GDKGLEXT_CFLAGS) $(LIBGLADE_CFLAGS) -DDATADIR=\"$(gladedir)/\"
desmume_glade_LDFLAGS = -rdynamic

EXTRA_DIST = DeSmuME.dev $(glade_DATA)
