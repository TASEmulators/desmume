EXTRA_DIST = DeSmuME.xpm dev-cpp/desmume.dev dev-cpp/desmume.layout \
	 dev-cpp/Makefile.win dev-cpp/pthread.h dev-cpp/sched.h \
	 dev-cpp/semaphore.h
bin_PROGRAMS = desmume
desmume_SOURCES = \
	gtk-compat.c gtk-compat.h \
	desmume.h desmume.c \
	dTool.h dToolsList.c \
	tools/ioregsView.c tools/ioregsView.h \
	globals.h \
	../sndsdl.c \
	../ctrlssdl.h ../ctrlssdl.c \
	 main.c
desmume_LDADD = ../libdesmume.a $(SDL_LIBS) $(GTK_LIBS)
desmume_CFLAGS = $(SDL_CFLAGS) $(GTK_CFLAGS)
#desmume_LDFLAGS = -rdynamic
