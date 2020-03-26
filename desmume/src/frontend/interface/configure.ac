dnl --- Package name is first argument to AC_INIT
dnl --- Release version is second argument to AC_INIT

AC_INIT(desmume, [git])
LT_INIT

dnl -- find host architecture for some os specific libraries
AC_CANONICAL_HOST
case $host in
  *linux*) desmume_arch=linux;;
  *mingw*) desmume_arch=windows;;
  *darwin*) desmume_arch=linux;;
  *bsd*) desmume_arch=linux;;
esac
AC_SUBST(desmume_arch)

AC_CONFIG_AUX_DIR([.])

AM_INIT_AUTOMAKE([subdir-objects tar-pax foreign])

dnl -- make sure we have a c++ compiler
AC_PROG_CXX

dnl -- check for endianess
AC_C_BIGENDIAN

COMMIT="unknown"
GIT=`which git 2>/dev/null`
if ( test -d ../../../../.git; ) && test "x${GIT}" != "x" -a -x "${GIT}" ; then
  COMMIT=`$GIT rev-parse HEAD`
  echo "$COMMIT"
fi
AC_DEFINE_UNQUOTED([GIT_COMMIT], ["$COMMIT"], [git commit hash])

dnl - Check for zlib
AC_CHECK_LIB(z, gzopen, [], [AC_MSG_ERROR([zlib was not found, we can't go further. Please install it or specify the location where it's installed.])])

dnl - Check for libpcap
AC_CHECK_LIB(pcap, main, [LIBS="$LIBS -lpcap"], [AC_MSG_ERROR([libpcap was not found, we can't go further. Please install it or specify the location where it's installed.])])

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

dnl - Looking for glib
FOUND_GLIB=no
GLIB_VER=2.8

PKG_CHECK_MODULES(GLIB,
                  glib-2.0 >= $GLIB_VER,
                  FOUND_GLIB=yes,
                  FOUND_GLIB=no)

dnl -- force lua disabled
HAVE_LUA=no

PKG_CHECK_MODULES(LIBAGG, libagg >= 2.4.0, FOUND_LIBAGG=yes, FOUND_LIBAGG=no)
AM_CONDITIONAL([HAVE_LIBAGG], [test "x$libagg" = "xyes" -a "${FOUND_LIBAGG}" = "yes"])
if test "x$libagg" = "xyes" ; then
   if test "x$FOUND_LIBAGG" = "xyes" ; then
       AC_SUBST(LIBAGG_CFLAGS)
       AC_SUBST(LIBAGG_LIBS)
       AC_DEFINE([HAVE_LIBAGG])
   else
       AC_MSG_ERROR([HUD rendering enabled, but libagg not found])
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

if test "x$HAVE_OPENAL" = "xno"; then
  AC_DEFINE([FAKE_MIC])
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
              [AC_HELP_STRING(--enable-wifi, enable wifi support)],
              [wifisupport=yes])

if test "x$wifisupport" = "xyes" ; then
    AC_DEFINE(EXPERIMENTAL_WIFI_COMM)
fi

dnl Set compiler library flags per host architecture
case $host in
  *mingw*)
    LIBS="$LIBS -ldxguid -ldxerr8 -ldsound -lopengl32 -lws2_32 -mwindows"
    ;;
  *darwin*)
    dnl - openal is required on mac os x and we are not able to figure out if it's installed or not
    AC_DEFINE(HAVE_OPENAL)
    LIBS="$LIBS -framework OpenGL -framework OpenAL"
    CPPFLAGS="$CPPFLAGS -I/System/Library/Frameworks/OpenAL.framework/Headers"
	AC_SUBST(CPPFLAGS)
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

AC_CHECK_DECL([__SSE2__])
AM_CONDITIONAL([SUPPORT_SSE2], [test "x$ac_cv_have_decl___SSE2__" = xyes])

AC_CHECK_DECL([__AVX2__])
AM_CONDITIONAL([SUPPORT_AVX2], [test "x$ac_cv_have_decl___AVX2__" = xyes])

AC_CHECK_DECL([__ALTIVEC__])
AM_CONDITIONAL([SUPPORT_ALTIVEC], [test "x$ac_cv_have_decl___ALTIVEC__" = xyes])

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

dnl -- set maintainer mode
AM_MAINTAINER_MODE
AC_SUBST(USE_MAINTAINER_MODE)

dnl --- Finally, output the makefile
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
