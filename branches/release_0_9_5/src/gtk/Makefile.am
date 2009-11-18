SUBDIRS = doc
include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTK_CFLAGS) $(GTHREAD_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS)

Applicationsdir = $(datadir)/applications
Applications_DATA = desmume.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = DeSmuME.xpm
EXTRA_DIST = DeSmuME.xpm desmume.desktop
bin_PROGRAMS = desmume
desmume_SOURCES = \
	desmume.h desmume.cpp \
	dTool.h dToolsList.cpp \
	tools/ioregsView.cpp tools/ioregsView.h \
	../sndsdl.cpp \
	../ctrlssdl.h ../ctrlssdl.cpp \
	osmesa_3Demu.cpp osmesa_3Demu.h \
	desmume_config.cpp desmume_config.h \
	cheatsGTK.h cheatsGTK.cpp \
	main.cpp main.h
desmume_LDADD = ../libdesmume.a \
	$(SDL_LIBS) $(GTK_LIBS) $(GTHREAD_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS)
if HAVE_GDB_STUB
desmume_LDADD += ../gdbstub/libgdbstub.a
endif

UPDATE_DESKTOP = \
  appsdir=$(DESTDIR)$(datadir)/applications ; \
  if [ -f $$appsdir/mimeinfo.cache ] ; then \
    if test ! "x$(UPDATEDESKTOP)" = "x" ; then \
      $(UPDATEDESKTOP) $$appsdir ; \
    fi \
  fi

install-data-hook:
	$(UPDATE_DESKTOP)

uninstall-hook:
	$(UPDATE_DESKTOP) 
