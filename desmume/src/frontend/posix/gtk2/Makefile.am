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
	../shared/avout.h \
	../shared/avout_flac.h ../shared/avout_flac.cpp \
	../shared/avout_pipe_base.h ../shared/avout_pipe_base.cpp \
	../shared/avout_x264.h ../shared/avout_x264.cpp \
	../shared/ctrlssdl.h ../shared/ctrlssdl.cpp \
	../shared/sndsdl.cpp \
	config.cpp config.h config_opts.h \
	desmume.h desmume.cpp \
	dTool.h dToolsList.cpp \
	tools/ioregsView.cpp tools/ioregsView.h \
	cheatsGTK.h cheatsGTK.cpp \
	main.cpp main.h

desmume_LDADD = ../libdesmume.a \
	$(X_LIBS) -lX11 $(SDL_LIBS) $(GTK_LIBS) $(GTHREAD_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(LIBSOUNDTOUCH_LIBS)

if ENABLE_OPENGL_ES
desmume_LDADD += $(OPENGLES_LIBS)
else
if ENABLE_OPENGL_STANDARD
desmume_LDADD += $(OPENGL_LIBS)
endif
endif

if ENABLE_GLX
desmume_SOURCES += ../shared/glx_3Demu.h ../shared/glx_3Demu.cpp
else
if ENABLE_OSMESA
desmume_LDADD += $(OSMESA_LIBS)
desmume_SOURCES += ../shared/osmesa_3Demu.h ../shared/osmesa_3Demu.cpp
else
if ENABLE_EGL
desmume_LDADD += $(EGL_LIBS)
desmume_SOURCES += ../shared/egl_3Demu.h ../shared/egl_3Demu.cpp
else
desmume_SOURCES += ../shared/sdl_3Demu.h ../shared/sdl_3Demu.cpp
endif
endif
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
