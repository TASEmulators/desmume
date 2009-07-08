dnl --- Package name is first argument to AC_INIT
dnl --- Release version is second argument to AC_INIT

AC_INIT(desmume, [0.9.4])

dnl -- find target architecture for some os specific libraries
AC_CANONICAL_TARGET
case $target in
  *linux*) desmume_arch=linux;;
  *mingw*) desmume_arch=windows;;
  *darwin*) desmume_arch=linux;;
  *bsd*) desmume_arch=linux;;
esac
AC_SUBST(desmume_arch)

AM_INIT_AUTOMAKE([1.10])

dnl -- make sure we have a c++ compiler
AC_PROG_CXX

dnl -- use ranlib for libraries
AC_PROG_RANLIB

dnl -- check for endianess
AC_C_BIGENDIAN

# took this from geany, hope it works
REVISION="r0"
SVN=`which svn 2>/dev/null`
if test -d ".svn" -a "x${SVN}" != "x" -a -x "${SVN}" ; then
  REVISION=r`$SVN info|grep 'Last Changed Rev'|cut -d' ' -f4`
fi
AC_DEFINE_UNQUOTED([SVN_REV], "$REVISION", [subversion revision number])


dnl --- Other prerequisites ---
dnl - Check for pkg-config macros
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])
dnl - Check for intltool/gettext macros
m4_ifdef([IT_PROG_INTLTOOL],[IT_PROG_INTLTOOL])

dnl - Check for zlib
AC_CHECK_LIB(z, gzopen, [], [AC_MSG_ERROR([zlib was not found, we can't go further. Please install it or specify the location where it's installed.])])

dnl - Check for zziplib
AC_CHECK_LIB(zzip, zzip_open, [
	LIBS="-lzzip $LIBS"
	AC_DEFINE([HAVE_LIBZZIP])
	AC_MSG_CHECKING([[whether zzip use void * as second parameter]])
	AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <zzip/lib.h>]], [[
		void * v;
		zzip_read(NULL, v, 0);]]),
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
  HAVE_SDL="yes"
else
  HAVE_SDL="no"
fi

dnl - Check for the OpenGL includes
AC_CHECK_HEADERS([GL/gl.h])
AC_CHECK_HEADERS([GL/glu.h])

dnl - if --enable-osmesa is used, check for it
AC_ARG_ENABLE([osmesa],
               [AC_HELP_STRING([--enable-osmesa], [use off-screen mesa])],
               [osmesa=$enableval],
               [osmesa=yes])

if test "x$osmesa" = "xyes" ; then
	AC_CHECK_LIB(dl, main)
	AC_CHECK_LIB([GL], main)
	AC_CHECK_LIB(OSMesa, main,[
		useosmesa=yes
		AC_DEFINE(HAVE_LIBOSMESA)
		LIBS="-lOSMesa $LIBS"
	])
fi

dnl - Check for GTK and/or libglade
FOUND_GLIB=no
GLIB_VER=2.8
GTK_VER=2.6
AC_CHECK_TOOL(HAVE_PKG, pkg-config)
AC_PROVIDE_IFELSE([PKG_PROG_PKG_CONFIG], [
  if test ! "x$HAVE_PKG" = "x" ; then
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

    PKG_CHECK_MODULES(GTKGLEXT,
                      "gtkglext-1.0",
                      HAVE_GTKGLEXT=yes,
                      HAVE_GTKGLEXT=no)
    AC_SUBST(GTKGLEXT_CFLAGS)
    AC_SUBST(GTKGLEXT_LIBS)

    PKG_CHECK_MODULES(GTHREAD,
                      "gthread-2.0",
                      HAVE_GTHREAD=yes,
                      HAVE_GTHREAD=no)
    AC_SUBST(GTHREAD_CFLAGS)
    AC_SUBST(GTHREAD_LIBS)

    PKG_CHECK_MODULES(LIBGLADE,
                      "libglade-2.0",
                      HAVE_LIBGLADE=yes,
                      HAVE_LIBGLADE=no)
    AC_SUBST(LIBGLADE_CFLAGS)
    AC_SUBST(LIBGLADE_LIBS)

    dnl uninstalled glade ui dir
    AC_DEFINE_UNQUOTED(GLADEUI_UNINSTALLED_DIR,"`pwd`/src/gtk-glade/glade/",[path to glade ui dir])
    AC_SUBST(GLADEUI_UNINSTALLED_DIR)
  fi
],[
    echo "WARNING: pkg-config is not available therefore gtk and gtk-glade UIs are not available either."
])

dnl - set conditional for glib, needed to avoid commandline.cpp compilation for cli frontend since it depends on glib
AM_CONDITIONAL([HAVE_GLIB], [test "${FOUND_GLIB}" = "yes"])

dnl - If the gtkGLext package is available define the corresponding C macro
if test "x$HAVE_GTKGLEXT" = "xyes"; then
   AC_DEFINE([GTKGLEXT_AVAILABLE], [1])
fi

dnl - Determine which UIs to build and if po/ should be included
PO_DIR=""
PO_FILES=""
PO_FILES_IN=""
if test "x$HAVE_SDL" = "xyes"; then
  # SDL adds just a cli
  UI_DIR="cli $UI_DIR"
  if test "x$HAVE_GTK" = "xyes"; then
    # GTK requires SDL
    UI_DIR="gtk $UI_DIR"
  fi

  if test "x$HAVE_LIBGLADE" = "xyes"; then
    AC_PROVIDE_IFELSE([IT_PROG_INTLTOOL],[
      # libglade requires SDL too
      UI_DIR="gtk-glade $UI_DIR"

      dnl -- localization for gtk-glade UI
      GETTEXT_PACKAGE=desmume
      AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the gettext package name])
      AC_SUBST(GETTEXT_PACKAGE)
      ALL_LINGUAS="fr pt_BR"
      AM_GLIB_GNU_GETTEXT
      PO_DIR="po"
      PO_FILES="intltool-extract intltool-merge intltool-update"
      PO_FILES_IN="intltool-extract.in intltool-merge.in intltool-update.in"
      PO_MAKEFILE="po/Makefile.in"
    ],[
      echo "WARNING: intltool and/or gettext are not available therefore the gtk-glade interface won't be installed. The gtk-glade UI requires intltool and gettext."
    ])
  fi
fi

dnl - wifi support
AC_ARG_ENABLE(wifi,
	      [AC_HELP_STRING(--enable-wifi, enable experimental wifi support)],
	      [
		AC_CHECK_LIB(pcap, main,[
		  AC_DEFINE(EXPERIMENTAL_WIFI)
		  LIBS="$LIBS -lpcap"
		])
	      ])

dnl Set compiler library flags per target.
case $target in
  *linux* | *bsd*)
    LIBS="$LIBS -lGLU"
    ;;
  *mingw*)
    LIBS="$LIBS -ldxguid -ldxerr8 -ldsound -lopengl32 -lws2_32 -mwindows"
    UI_DIR="windows"
    ;;
  *darwin*)
    LIBS="$LIBS -framework OpenGL"
    dnl - extra hackery needed for X includes
    AC_PATH_XTRA
    ;;
esac

AC_SUBST(UI_DIR)
AC_SUBST(PO_DIR)
AC_SUBST(PO_FILES)
AC_SUBST(PO_FILES_IN)

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
   CPPFLAGS="[$]CPPFLAGS -Wall -Wextra -D_FORTIFY_SOURCE=2 -Wno-missing-field-initializers -Wpointer-arith -Wcast-align -Wwrite-strings -Wno-unused-parameter -Wmissing-declarations -Wundef -Wmissing-noreturn -Wshadow -Wredundant-decls -Wformat-nonliteral -Wformat-security -Winit-self"
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

dnl - Enable memory profiling
AC_ARG_ENABLE(memory-profiling,
              AC_HELP_STRING(--enable-memory-profiling, enable memory profiling information),
              AC_DEFINE(PROFILE_MEMORY_ACCESS))

dnl -- set maintainer mode
AM_MAINTAINER_MODE
AC_SUBST(USE_MAINTAINER_MODE)


dnl --- Finally, output all the makefiles
AC_CONFIG_FILES([Makefile
                 ${PO_MAKEFILE}
                 src/Makefile
                 src/cli/Makefile
                 src/cli/doc/Makefile
                 src/cocoa/Makefile
                 src/gtk/Makefile
                 src/gtk/doc/Makefile
                 src/gtk-glade/Makefile
                 src/gtk-glade/doc/Makefile
                 src/windows/Makefile
		 src/gdbstub/Makefile
		 autopackage/default.apspec
])
AC_OUTPUT
