include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += -DHAVE_WX=1 -DHAVE_X11=1 $(WX_CPPFLAGS) $(SDL_CFLAGS) $(LIBAGG_CFLAGS)

Applicationsdir = $(datadir)/applications
Applications_DATA = wxdesmume.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = DeSmuME.xpm
EXTRA_DIST = wxdesmume.desktop
bin_PROGRAMS = wxdesmume
wxdesmume_SOURCES = \
	../sndsdl.cpp \
	../driver.h ../driver.cpp \
	../wxdlg/wxdlg3dViewer.h ../wxdlg/wxdlg3dViewer.cpp \
	PadSimple/PadSimple.cpp PadSimple/GUI/ConfigDlg.cpp \
	InputCommon/X11InputBase.cpp \
	InputCommon/XInput.cpp \
	InputCommon/InputCommon.cpp \
	InputCommon/EventHandler.cpp \
	InputCommon/Configuration.cpp \
	InputCommon/SDL.cpp \
	StringUtil.cpp \
	wxMain.cpp
wxdesmume_LDADD = ../libdesmume.a \
	$(SDL_LIBS) $(GTHREAD_LIBS) $(GLIB_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(WX_LIBS)
if HAVE_GDB_STUB
wxdesmume_LDADD += ../gdbstub/libgdbstub.a
endif
