include $(top_srcdir)/src/desmume.mk

AM_CPPFLAGS += -DHAVE_WX=1 -DHAVE_X11=1 $(WX_CPPFLAGS) $(SDL_CFLAGS) $(LIBAGG_CFLAGS)

Applicationsdir = $(datadir)/applications
Applications_DATA = wxdesmume.desktop
pixmapdir = $(datadir)/pixmaps
pixmap_DATA = DeSmuME.xpm
EXTRA_DIST = wxdesmume.desktop DeSmuME.xpm
bin_PROGRAMS = wxdesmume
wxdesmume_SOURCES = \
	../sndsdl.cpp \
	../driver.h ../driver.cpp \
	../wxdlg/wxdlg3dViewer.h ../wxdlg/wxdlg3dViewer.cpp \
	../wxdlg/wxcontrolsconfigdialog.h ../wxdlg/wxcontrolsconfigdialog.cpp \
	PadSimple/pluginspecs_pad.h \
	PadSimple/PadSimple.h PadSimple/PadSimple.cpp \
	PadSimple/GUI/ConfigDlg.h PadSimple/GUI/ConfigDlg.cpp \
	InputCommon/X11InputBase.h InputCommon/X11InputBase.cpp \
	InputCommon/XInput.h InputCommon/XInput.cpp \
	InputCommon/InputCommon.h InputCommon/InputCommon.cpp \
	InputCommon/Event.hpp \
	InputCommon/EventHandler.h InputCommon/EventHandler.cpp \
	InputCommon/Configuration.cpp \
	InputCommon/SDL.h InputCommon/SDL.cpp \
	LuaWindow.h \
	StringUtil.h StringUtil.cpp \
	wxMain.cpp
wxdesmume_LDADD = ../libdesmume.a \
	$(SDL_LIBS) $(GTHREAD_LIBS) $(GLIB_LIBS) $(ALSA_LIBS) $(LIBAGG_LIBS) $(WX_LIBS) -lX11
if HAVE_GDB_STUB
wxdesmume_LDADD += ../gdbstub/libgdbstub.a
endif
