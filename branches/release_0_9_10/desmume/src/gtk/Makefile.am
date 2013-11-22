SUBDIRS = doc
include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += $(SDL_CFLAGS) $(GTK_CFLAGS) $(GTHREAD_CFLAGS) $(ALSA_CFLAGS) $(LIBAGG_CFLAGS) $(LIBSOUNDTOUCH_CFLAGS)

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
	../driver.h ../driver.cpp \
	osmesa_3Demu.cpp osmesa_3Demu.h \
	glx_3Demu.cpp glx_3Demu.h \
	cheatsGTK.h cheatsGTK.cpp \
	../filter/hq2x.cpp ../filter/hq2x.h \
	../filter/hq4x.cpp ../filter/hq4x.h \
	../filter/2xsai.cpp ../filter/bilinear.cpp \
	../filter/epx.cpp ../filter/lq2x.cpp \
	../filter/scanline.cpp \
	../filter/videofilter.cpp ../filter/videofilter.h \
	main.cpp main.h
desmume_LDADD = ../libdesmume.a \
	$(SDL_LIBS) $(GTK_LIBS) $(GTHREAD_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(LIBSOUNDTOUCH_LIBS)
if HAVE_GDB_STUB
desmume_LDADD += ../gdbstub/libgdbstub.a
endif
if HAVE_LIBOSMESA
desmume_LDADD += $(OSMESA_LIBS)
else
if HAVE_GL_GLX
desmume_LDADD += $(GLX_LIBS)
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
