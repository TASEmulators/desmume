SUBDIRS = doc
include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS) $(GLIB_CFLAGS)

bin_PROGRAMS = desmume-cli
desmume_cli_SOURCES = main.cpp ../sndsdl.cpp ../ctrlssdl.h ../ctrlssdl.cpp
desmume_cli_LDADD = ../libdesmume.a $(SDL_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(GLIB_LIBS)
if HAVE_GDB_STUB
desmume_cli_LDADD += ../gdbstub/libgdbstub.a
endif
