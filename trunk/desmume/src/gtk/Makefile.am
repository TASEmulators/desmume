SUBDIRS = doc
include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTK_CFLAGS) $(GTKGLEXT_CFLAGS) $(GTHREAD_CFLAGS)

Applicationsdir = $(datadir)/applications
Applications_DATA = desmume.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = DeSmuME.xpm
EXTRA_DIST = DeSmuME.xpm dev-cpp/desmume.dev dev-cpp/desmume.layout \
	 dev-cpp/Makefile.win dev-cpp/pthread.h dev-cpp/sched.h \
	 dev-cpp/semaphore.h desmume.desktop
bin_PROGRAMS = desmume
desmume_SOURCES = \
	desmume.h desmume.cpp \
	dTool.h dToolsList.cpp \
	tools/ioregsView.cpp tools/ioregsView.h \
	globals.h \
	../sndsdl.cpp \
	../ctrlssdl.h ../ctrlssdl.cpp \
	gdk_3Demu.cpp gdk_3Demu.h \
	 main.cpp
desmume_LDADD = ../libdesmume.a \
	$(SDL_LIBS) $(GTK_LIBS) $(GTKGLEXT_LIBS) $(GTHREAD_LIBS)
if HAVE_GDB_STUB
desmume_LDADD += ../gdbstub/libgdbstub.a
endif
