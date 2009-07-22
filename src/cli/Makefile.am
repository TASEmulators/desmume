SUBDIRS = doc
include $(top_srcdir)/src/desmume.mk

AM_CFLAGS += $(SDL_CFLAGS)

bin_PROGRAMS = desmume-cli
desmume_cli_SOURCES = main.cpp ../sndsdl.cpp ../ctrlssdl.h ../ctrlssdl.cpp
desmume_cli_LDADD = ../libdesmume.a $(SDL_LIBS)
if HAVE_GDB_STUB
desmume_cli_LDADD += ../gdbstub/libgdbstub.a
endif
