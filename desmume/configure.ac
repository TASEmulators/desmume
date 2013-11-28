dnl --- Package name is first argument to AC_INIT
dnl --- Release version is second argument to AC_INIT

AC_INIT(desmume, [0.9.10])

dnl -- find host architecture for some os specific libraries
AC_CANONICAL_HOST
case $host in
  *linux*) desmume_arch=linux;;
  *mingw*) desmume_arch=windows;;
  *darwin*) desmume_arch=linux;;
  *bsd*) desmume_arch=linux;;
esac
AC_SUBST(desmume_arch)

AM_INIT_AUTOMAKE([1.10 subdir-objects tar-pax])

dnl -- make sure we have a c++ compiler
AC_PROG_CXX

dnl -- use ranlib for libraries
AC_PROG_RANLIB

dnl -- check for endianess
AC_C_BIGENDIAN

dnl -- since svn 1.7 theres only a single .svn folder in the root dir of the checkout
dnl -- depending on what was checked out that might be .svn or ../.svn
REVISION=0
SVN=`which svn 2>/dev/null`
if ( test -d .svn || test -d ../.svn ; ) && test "x${SVN}" != "x" -a -x "${SVN}" ; then
  REVISION=`$SVN info|grep 'Last Changed Rev'|cut -d' ' -f4`
  echo "$REVISION"
fi
AC_DEFINE_UNQUOTED([SVN_REV], [$REVISION], [subversion revision number])
AC_DEFINE_UNQUOTED([SVN_REV_STR], ["$REVISION"], [subversion revision number string])

dnl - Check for intltool/gettext macros
IT_PROG_INTLTOOL

dnl - Check for zlib
AC_CHECK_LIB(z, gzopen, [], [AC_MSG_ERROR([zlib was not found, we can't go further. Please install it or specify the location where it's installed.])])

dnl - Check for zziplib
AC_CHECK_LIB(zzip, zzip_open, [
	LIBS="-lzzip $LIBS"
	AC_DEFINE([HAVE_LIBZZIP])
	AC_MSG_CHECKING([[whether zzip use void * as second parameter]])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <zzip/lib.h>]], [[
		void * v;
		zzip_read(NULL, v, 0);]])],
		AC_MSG_RESULT(yes),
		AC_DEFINE([ZZIP_OLD_READ])
		AC_MSG_RESULT(no))
])

dnl - Check for SDL
AC_PATH_PROGS(SDLCONFIG, [sdl-config sdl11-config])
if test ! "x$SDLCONFIG" = "x" ; then
  SDL_CFLAGS=`$SDLCONFIG --cflags`
  SDL_LIBS=`$SDLCONFIG --libs`
  AC_SUBST(SDL_CFLAGS)
  AC_SUBST(SDL_LIBS)
else
  AC_MSG_ERROR([sdl is required to build desmume])
fi

dnl - Check for the OpenGL includes
AC_CHECK_HEADERS([GL/gl.h],
		 [AC_CHECK_HEADERS([GL/glu.h], [have_gl_h=yes LIBS="$LIBS -lGL -lGLU"], [have_gl_h=no])],
		 [have_gl_h=no])
if test "have_gl_h" = "no" ; then
	AC_MSG_WARN([Building without GL support because of missing headers.])
fi
AM_CONDITIONAL([HAVE_GL], [test "${have_gl_h}" = "yes"])

dnl - if --enable-osmesa is used, check for it
AC_ARG_ENABLE([osmesa],
               [AC_HELP_STRING([--enable-osmesa], [use off-screen mesa])],
               [osmesa=$enableval],
               [osmesa=no])

dnl - GLX is the default renderer
AC_ARG_ENABLE([glx],
               [AC_HELP_STRING([--enable-glx], [use hw accelerated rendering])],
               [glx=$enableval],
               [glx=yes])

if test "x$osmesa" = "xyes" ; then
	AC_CHECK_LIB(dl, main)
	AC_CHECK_LIB([GL], main)
	AC_CHECK_LIB(OSMesa, main,[
		useosmesa=yes
		AC_DEFINE(HAVE_LIBOSMESA)
		OSMESA_LIBS="-lOSMesa"
		AC_SUBST(OSMESA_LIBS)
	])
else
  if test "x$glx" = "xyes" ; then
	AC_CHECK_LIB(dl, main)
	AC_CHECK_LIB([GL], main)
	AC_CHECK_HEADERS([GL/glx.h], [
	  useglx=yes
	  AC_DEFINE(HAVE_GL_GLX)
	  GLX_LIBS="-lX11"
	  AC_SUBST(GLX_LIBS)
	])
  fi
fi
AM_CONDITIONAL([HAVE_LIBOSMESA],  [test "${useosmesa}" = "yes"])
AM_CONDITIONAL([HAVE_GL_GLX],	  [test "${useglx}" = "yes"])

dnl - make the usage of libagg for HUD rendering configurable
AC_ARG_ENABLE([hud],
              [AC_HELP_STRING([--enable-hud], [Enable HUD rendering, requires libagg])],
              [libagg=yes])

HAVE_OPENAL=no
dnl - openal support
AC_ARG_ENABLE(openal,
	      [AC_HELP_STRING(--enable-openal, enable experimental OpenAL microphone input)],
	      [openal=yes])

if test "x$openal" = "xyes" ; then
	AC_CHECK_LIB([openal], [main],[
                HAVE_OPENAL=yes
		LIBS="$LIBS -lopenal"
	])
fi
AM_CONDITIONAL([HAVE_OPENAL], [test "${HAVE_OPENAL}" = "yes"])

dnl - Check for GTK and/or libglade
FOUND_GLIB=no
HAVE_ALSA=no
GLIB_VER=2.8
GTK_VER=2.14

PKG_CHECK_MODULES(GLIB,
                  glib-2.0 >= $GLIB_VER,
                  FOUND_GLIB=yes,
                  FOUND_GLIB=no)

PKG_CHECK_MODULES(GTK,
                  gtk+-2.0 >= $GTK_VER,
                  HAVE_GTK=yes,
                  HAVE_GTK=no)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)


PKG_CHECK_MODULES(GTHREAD,
                  "gthread-2.0",
                  HAVE_GTHREAD=yes,
                  HAVE_GTHREAD=no)
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

AC_ARG_ENABLE([glade],
               [AC_HELP_STRING([--enable-glade], [enable glade frontend])],
               [glade=$enableval],
               [glade=no])

if test "x$glade" = "xyes" ; then
    PKG_CHECK_MODULES(LIBGLADE,
                      "libglade-2.0",
                      HAVE_LIBGLADE=yes,
                      HAVE_LIBGLADE=no)
    AC_SUBST(LIBGLADE_CFLAGS)
    AC_SUBST(LIBGLADE_LIBS)

    dnl uninstalled glade ui dir
    AC_DEFINE_UNQUOTED(GLADEUI_UNINSTALLED_DIR,"`pwd`/src/gtk-glade/glade/",[path to glade ui dir])
    AC_SUBST(GLADEUI_UNINSTALLED_DIR)

    PKG_CHECK_MODULES(GTKGLEXT,
                      "gtkglext-1.0",
                      AC_DEFINE([GTKGLEXT_AVAILABLE], [1])
                      [])
    AC_SUBST(GTKGLEXT_CFLAGS)
    AC_SUBST(GTKGLEXT_LIBS)
fi

AC_PATH_PROG(UPDATEDESKTOP, [update-desktop-database])

# Need check for both lua and lua5.1 to run on debian, see mysql bug #29945
PKG_CHECK_MODULES(LUA,
                  lua >= 5.1,
                  HAVE_LUA=yes,
                  HAVE_LUA=no)
PKG_CHECK_MODULES(LUA,
                  lua5.1 >= 5.1,
                  HAVE_LUA=yes,
                  HAVE_LUA=no)
AC_SUBST(LUA_CFLAGS)
AC_SUBST(LUA_LIBS)
dnl -- force lua disabled
AM_CONDITIONAL([HAVE_LUA], [test "${HAVE_LUA}x" = "yes"])

if test ! "x$HAVE_OPENAL" = "xyes" ; then
  PKG_CHECK_MODULES(ALSA, alsa >= 1.0, HAVE_ALSA=yes, HAVE_ALSA=no)
  AC_SUBST(ALSA_CFLAGS)
  AC_SUBST(ALSA_LIBS)
fi
AM_CONDITIONAL([HAVE_ALSA], [test "${HAVE_ALSA}" = "yes"])

PKG_CHECK_MODULES(LIBAGG, libagg >= 2.5.0, FOUND_LIBAGG=yes, FOUND_LIBAGG=no)
AM_CONDITIONAL([HAVE_LIBAGG], [test "x$libagg" = "xyes" -a "${FOUND_LIBAGG}" = "yes"])
if test "x$libagg" = "xyes" ; then
   if test "x$FOUND_LIBAGG" = "xyes" ; then
       AC_SUBST(LIBAGG_CFLAGS)
       AC_SUBST(LIBAGG_LIBS)
       AC_DEFINE([HAVE_LIBAGG])
   else
       AC_MSG_WARN([HUD rendering enabled, but libagg not found])
   fi
fi

PKG_CHECK_MODULES(LIBSOUNDTOUCH, soundtouch >= 1.5.0, HAVE_LIBSOUNDTOUCH=yes, HAVE_LIBSOUNDTOUCH=no)
AC_SUBST(LIBSOUNDTOUCH_CFLAGS)
AC_SUBST(LIBSOUNDTOUCH_LIBS)
AM_CONDITIONAL([HAVE_LIBSOUNDTOUCH], [test "${HAVE_LIBSOUNDTOUCH}" = "yes"])
if test "x$HAVE_LIBSOUNDTOUCH" = "xyes"; then
   AC_DEFINE([HAVE_LIBSOUNDTOUCH])
else
   AC_MSG_WARN([SoundTouch library not found, pcsx2 resampler will be disabled])
fi

if test "x$HAVE_ALSA" = "xno"; then
   if test "x$HAVE_OPENAL" = "xno"; then
      AC_DEFINE([FAKE_MIC])
   fi
fi

dnl - Determine which UIs to build and if po/ should be included
PO_DIR="po"
PO_MAKEFILE="po/Makefile.in"
UI_DIR="cli $UI_DIR"
if test "x$HAVE_GTK" = "xyes"; then
  UI_DIR="gtk $UI_DIR"
fi

if test "x$HAVE_LIBGLADE" = "xyes"; then
    UI_DIR="gtk-glade $UI_DIR"

    dnl -- localization for gtk-glade UI
    GETTEXT_PACKAGE=desmume
    AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the gettext package name])
    AC_SUBST(GETTEXT_PACKAGE)
    ALL_LINGUAS="fr nb pt_BR sv zh_CN zh_TW"
    AM_GLIB_GNU_GETTEXT
fi

dnl - jit support
case $host_cpu in
  x86|x86_64|i386|i486|i586|i686)
    HAVE_JIT=yes
    AC_DEFINE(HAVE_JIT)
    ;;
esac
AM_CONDITIONAL([HAVE_JIT], [test "x$HAVE_JIT" = "xyes"])

dnl - wifi support
AC_ARG_ENABLE(wifi,
	      [AC_HELP_STRING(--enable-wifi, enable experimental wifi comm support)],
	      [
		AC_CHECK_LIB(pcap, main,[
		  AC_DEFINE(EXPERIMENTAL_WIFI_COMM)
		  LIBS="$LIBS -lpcap"],
		  [AC_MSG_WARN([pcap library not found, wifi will not work])])
	      ])

dnl Set compiler library flags per host architecture
case $host in
  *mingw*)
    LIBS="$LIBS -ldxguid -ldxerr8 -ldsound -lopengl32 -lws2_32 -mwindows"
    UI_DIR="windows"
    ;;
  *darwin*)
    dnl - openal is required on mac os x and we are not able to figure out if it's installed or not
    AC_DEFINE(HAVE_OPENAL)
    LIBS="$LIBS -framework OpenGL -framework OpenAL"
    CPPFLAGS="$CPPFLAGS -I/System/Library/Frameworks/OpenAL.framework/Headers"
	AC_SUBST(CPPFLAGS)
    dnl - extra hackery needed for X includes
    AC_PATH_XTRA
    ;;
esac

# Detect the host platform and architecture and feed them to the compiler as
# defines
AS_CASE([$host],
		[*linux*], [AC_DEFINE(HOST_LINUX)],
		[*bsd*]  , [AC_DEFINE(HOST_BSD)],
		[*mingw*], [AC_DEFINE(HOST_WINDOWS)],
		[*darwin*],[AC_DEFINE(HOST_DARWIN)],
		[AC_DEFINE(HOST_UNK)]
)

AS_CASE([$host_cpu],
		[x86_64], [AC_DEFINE(HOST_64)],
		[amd64],  [AC_DEFINE(HOST_64)],
		[AC_DEFINE(HOST_32)]
)

AC_SUBST(UI_DIR)
AC_SUBST(PO_DIR)

dnl - Gdb stub
AC_ARG_ENABLE(gdb-stub,
              [AC_HELP_STRING(--enable-gdb-stub, enable gdb stub)],
              [
			AC_DEFINE(GDB_STUB)
			wantgdbstub=yes
		])
AM_CONDITIONAL([HAVE_GDB_STUB], [test "${wantgdbstub}" = "yes"])

dnl - Compiler warnings

# for developer use, enable lots of compile warnings,
# but don't require this generally, because some system's
# header files (BSD) can't handle it
#
# NB: must add -Werror after AC_PROG_CC, etc., so do this last
AC_ARG_ENABLE(hardcore,
              [AC_HELP_STRING(--enable-hardcore, turn on -W -Wall -Werror)],
              [case "${enableval}" in
                yes) ENABLE_HARDCORE=1 ;;
                no) ENABLE_HARDCORE=0 ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-hardcore) ;;              esac],
              [ENABLE_HARDCORE=0])

if test "x[$]ENABLE_HARDCORE" = "x1"; then
  AC_MSG_WARN(enable hardcore compile warnings)
  if test "x$CXX" = "x"; then
    dnl - only valid for C with newer gcc's
    CPPFLAGS="[$]CPPFLAGS -Wmissing-prototypes"
  fi
   dnl - -Wshadow
   CPPFLAGS="[$]CPPFLAGS -Wall -Wextra -D_FORTIFY_SOURCE=2 -Wno-missing-field-initializers -Wpointer-arith -Wcast-align -Wwrite-strings -Wno-unused-parameter -Wmissing-declarations -Wundef -Wmissing-noreturn -Wredundant-decls -Wformat-nonliteral -Wformat-security -Winit-self -Wno-reorder"
fi

dnl - Enable debug mode
AC_ARG_ENABLE(debug,
              AC_HELP_STRING(--enable-debug, enable debug information),
              AC_DEFINE(DEBUG))
AC_ARG_ENABLE(gpu-debug,
              AC_HELP_STRING(--enable-gpu-debug, enable gpu debug information),
              AC_DEFINE(GPUDEBUG))
AC_ARG_ENABLE(div-debug,
              AC_HELP_STRING(--enable-div-debug, enable div debug information),
              AC_DEFINE(DIVDEBUG))
AC_ARG_ENABLE(sqrt-debug,
              AC_HELP_STRING(--enable-sqrt-debug, enable sqrt debug information),
              AC_DEFINE(SQRTDEBUG))
AC_ARG_ENABLE(dma-debug,
              AC_HELP_STRING(--enable-dma-debug, enable dma debug information),
              AC_DEFINE(DMADEBUG))

dnl - Enable memory profiling (disabled)
dnl - AC_ARG_ENABLE(memory-profiling,
dnl -              AC_HELP_STRING(--enable-memory-profiling, enable memory profiling information),
dnl -              AC_DEFINE(PROFILE_MEMORY_ACCESS))

dnl -- set maintainer mode
AM_MAINTAINER_MODE
AC_SUBST(USE_MAINTAINER_MODE)


dnl --- Finally, output all the makefiles
AC_CONFIG_FILES([Makefile
                 ${PO_MAKEFILE}
                 src/Makefile
                 src/cli/Makefile
                 src/cli/doc/Makefile
                 src/gtk/Makefile
                 src/gtk/doc/Makefile
                 src/gtk-glade/Makefile
                 src/gtk-glade/doc/Makefile
		 src/gdbstub/Makefile
		 autopackage/default.apspec
])
AC_OUTPUT
