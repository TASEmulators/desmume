dnl --- Package name is first argument to AC_INIT
dnl --- Release version is second argument to AC_INIT

AC_INIT(desmume, [0.7.2])

dnl -- find target architecture for some os specific libraries
AC_CANONICAL_TARGET
case $target in
  *linux*) desmume_arch=linux;;
  *mingw*) desmume_arch=windows;;
  *darwin*) desmume_arch=linux;;
  *bsd*) desmume_arch=linux;;
esac
AC_SUBST(desmume_arch)

AM_INIT_AUTOMAKE

dnl -- make sure we have a C compiler
AC_PROG_CC

dnl -- use ranlib for libraries
AC_PROG_RANLIB

dnl -- check for endianess
AC_C_BIGENDIAN

dnl --- Other prerequisites ---
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])

dnl - Check for zlib
AC_CHECK_LIB(z, gzopen)

dnl - Check for zziplib
AC_CHECK_LIB(zzip, zzip_open)

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

dnl - Check for GTK and/or libglade
AC_CHECK_TOOL(HAVE_PKG, pkg-config)
AC_PROVIDE_IFELSE([PKG_PROG_PKG_CONFIG], [
  if test ! "x$HAVE_PKG" = "x" ; then
    PKG_CHECK_MODULES(GTK,
                      "gtk+-2.0",
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


    AC_CHECK_LIB(gdkglext-x11-1.0, gdk_gl_init)

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
    # libglade requires SDL too
    UI_DIR="gtk-glade $UI_DIR"

    dnl -- localization for gtk-glade UI
    GETTEXT_PACKAGE=desmume
    AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the gettext package name])
    AC_SUBST(GETTEXT_PACKAGE)
    ALL_LINGUAS="fr"
    AM_GLIB_GNU_GETTEXT
    AC_PROG_INTLTOOL
    PO_DIR="po"
    PO_FILES="intltool-extract intltool-merge intltool-update"
    PO_FILES_IN="intltool-extract.in intltool-merge.in intltool-update.in"
  fi
fi

dnl Set compiler library flags per target.
case $target in
  *linux* | *bsd*)
    LIBS="$LIBS -lGL -lGLU"
    ;;
  *mingw*)
    LIBS="$LIBS -ldxguid -ldxerr8 -ldsound -lopengl32 -lws2_32 -mwindows"
    UI_DIR="windows"
    ;;
  *darwin*)
    LIBS="$LIBS -framework OpenGL"
    ;;
esac

AC_SUBST(UI_DIR)
AC_SUBST(PO_DIR)
AC_SUBST(PO_FILES)
AC_SUBST(PO_FILES_IN)

dnl - Compiler warnings

# for developer use, enable lots of compile warnings,
# but don't require this generally, because some system's
# header files (BSD) can't handle it
#
# NB: must add -Werror after AC_PROG_CC, etc., so do this last
AC_ARG_ENABLE(hardcore,
              [  --enable-hardcore      turn on -W -Wall -Werror],
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
  CPPFLAGS="[$]CPPFLAGS -Werror -W -Wall -Wpointer-arith -Wcast-align -Wwrite-strings"
fi

dnl - Enable debug mode
AC_ARG_ENABLE(debug,
              AC_HELP_STRING(--enable-debug, enable debug information),
              AC_DEFINE(DEBUG))

dnl -- set maintainer mode
AM_MAINTAINER_MODE
AC_SUBST(USE_MAINTAINER_MODE)


dnl --- Finally, output all the makefiles
AC_CONFIG_FILES([Makefile
                 po/Makefile.in
                 src/Makefile
                 src/cli/Makefile
                 src/gtk/Makefile
                 src/gtk/desmume.desktop
                 src/gtk-glade/Makefile
                 src/gtk-glade/desmume-glade.desktop
                 src/windows/Makefile
		 src/gdbstub/Makefile
		 autopackage/default.apspec
])
AC_OUTPUT
