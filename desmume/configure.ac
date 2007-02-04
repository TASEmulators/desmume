dnl --- Package name is first argument to AC_INIT
dnl --- Release version is second argument to AC_INIT

AC_INIT(desmume, [0.6.0])
AM_INIT_AUTOMAKE

dnl -- find target architecture for some os specific libraries
AC_CANONICAL_TARGET
case $target in
  *linux*) desmume_arch=linux;;
  *mingw*) desmume_arch=windows;;
  *darwin*) desmume_arch=linux;;
  *bsd*) desmume_arch=linux;;
esac
AC_SUBST(desmume_arch)

dnl -- make sure we have a C++ compiler
AC_PROG_CXX

dnl -- use ranlib for libraries
AC_PROG_RANLIB

dnl -- check for endianess
AC_C_BIGENDIAN

dnl --- Other prerequisites ---

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

dnl - Check for GTK and/or libglade
AC_CHECK_TOOL(HAVE_PKG, pkg-config)
if test ! "x$HAVE_PKG" = "x" ; then
  PKG_CHECK_MODULES(GTK,
                    "gtk+-2.0",
                    HAVE_GTK=yes,
                    HAVE_GTK=no)
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)

  PKG_CHECK_MODULES(GDKGLEXT,
                    "gtkglext-1.0",
                    HAVE_GDKGLEXT=yes,
                    HAVE_GDKGLEXT=no)
  AC_SUBST(GDKGLEXT_CFLAGS)
  AC_SUBST(GDKGLEXT_LIBS)

  AC_CHECK_LIB(gdkglext-x11-1.0, gdk_gl_init)

  PKG_CHECK_MODULES(LIBGLADE,
                    "libglade-2.0",
                    HAVE_LIBGLADE=yes,
                    HAVE_LIBGLADE=no)
  AC_SUBST(LIBGLADE_CFLAGS)
  AC_SUBST(LIBGLADE_LIBS)
fi


dnl - Determine which UI's to build
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
  fi
fi

case $target in
  *mingw*)
    LIBS="$LIBS -ldxguid -ldxerr8 -ldsound -mwindows"
    UI_DIR="windows"
    ;;
esac

AC_SUBST(UI_DIR)


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
                 src/Makefile
                 src/cli/Makefile
                 src/gtk/Makefile
                 src/gtk-glade/Makefile
                 src/windows/Makefile
		 autopackage/default.apspec
])
AC_OUTPUT
