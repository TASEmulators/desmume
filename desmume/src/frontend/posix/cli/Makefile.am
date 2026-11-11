SUBDIRS = doc
include ../desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS) $(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(LIBSOUNDTOUCH_CFLAGS)

bin_PROGRAMS = desmume-cli
desmume_cli_SOURCES = main.cpp ../shared/sndsdl.cpp ../shared/ctrlssdl.h ../shared/ctrlssdl.cpp
desmume_cli_LDADD = ../libdesmume.a $(X_LIBS) -lX11 $(SDL_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(GLIB_LIBS) $(GTHREAD_LIBS) $(LIBSOUNDTOUCH_LIBS)
