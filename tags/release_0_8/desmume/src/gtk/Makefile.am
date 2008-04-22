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
	gtk-compat.c gtk-compat.h \
	desmume.h desmume.c \
	dTool.h dToolsList.c \
	tools/ioregsView.c tools/ioregsView.h \
	globals.h \
	../sndsdl.c \
	../ctrlssdl.h ../ctrlssdl.c \
	gdk_3Demu.c gdk_3Demu.h \
	 main.c
desmume_LDADD = ../libdesmume.a ../gdbstub/libgdbstub.a \
	$(SDL_LIBS) $(GTK_LIBS) $(GTKGLEXT_LIBS) $(GTHREAD_LIBS)
