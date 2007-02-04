EXTRA_DIST = DeSmuME.dev glade/DeSmuMe_Dtools.glade glade/DeSmuMe.glade glade/DeSmuME.xpm
bin_PROGRAMS = desmume-glade
desmume_glade_SOURCES =  \
	callbacks.c callbacks.h callbacks_IO.c callbacks_IO.h desmume.c desmume.h \
	globals.h keyval_names.c main.c printscreen.c \
	dTools/callbacks_1_ioregs.c dTools/callbacks_2_memview.c \
	dTools/callbacks_3_palview.c dTools/callbacks_dtools.h \
	dTools/dTools_display.h \
	../sndsdl.h ../sndsdl.c ../ctrlssdl.h ../ctrlssdl.c 
desmume_glade_LDADD = ../libdesmume.a $(SDL_LIBS) $(GDKGLEXT_LIBS) $(LIBGLADE_LIBS)
desmume_glade_CFLAGS = $(SDL_CFLAGS) $(GDKGLEXT_CFLAGS) $(LIBGLADE_CFLAGS)
desmume_glade_LDFLAGS = -rdynamic
