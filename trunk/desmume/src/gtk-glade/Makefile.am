bin_PROGRAMS = desmume-glade
desmume_glade_SOURCES =  \
	callbacks.c callbacks.h callbacks_IO.c callbacks_IO.h desmume.c desmume.h \
	globals.h keyval_names.c main.c printscreen.c \
	dTools/callbacks_1_ioregs.c dTools/callbacks_dtools.h \
	../sndsdl.h ../sndsdl.c joysdl.h joysdl.c
desmume_glade_LDADD = ../libdesmume.a $(SDL_LIBS) $(LIBGLADE_LIBS)
desmume_glade_CFLAGS = $(SDL_CFLAGS) $(LIBGLADE_CFLAGS)
desmume_glade_LDFLAGS = -rdynamic
