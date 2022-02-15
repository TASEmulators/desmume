SUBDIRS = doc
include ../desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTK_CFLAGS) $(GTHREAD_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS) $(LIBSOUNDTOUCH_CFLAGS)

Applicationsdir = $(datadir)/applications
Applications_DATA = desmume.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = DeSmuME.xpm
EXTRA_DIST = DeSmuME.xpm desmume.desktop
bin_PROGRAMS = desmume
desmume_SOURCES = \
	avout.h \
	avout_pipe_base.cpp avout_pipe_base.h \
	avout_x264.cpp avout_x264.h \
	avout_flac.cpp avout_flac.h \
	config.cpp config.h config_opts.h \
	desmume.h desmume.cpp \
	dTool.h dToolsList.cpp \
	tools/ioregsView.cpp tools/ioregsView.h \
	../shared/sndsdl.cpp \
	../shared/ctrlssdl.h ../shared/ctrlssdl.cpp \
	osmesa_3Demu.cpp osmesa_3Demu.h \
	sdl_3Demu.cpp sdl_3Demu.h \
	cheatsGTK.h cheatsGTK.cpp \
	main.cpp main.h
desmume_LDADD = ../libdesmume.a \
	$(X_LIBS) -lX11 $(SDL_LIBS) $(GTK_LIBS) $(GTHREAD_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(LIBSOUNDTOUCH_LIBS)
if HAVE_LIBOSMESA
desmume_LDADD += $(OSMESA_LIBS)
else
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
