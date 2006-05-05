AC_INIT(desmume, 0.3.3)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(desmume, 0.3.3)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_LANG(C++)

AC_C_BIGENDIAN

AC_PATH_PROGS(HAVE_LIBSDL, [sdl-config sdl11-config])

case $target in
	*linux*)
		AC_CHECK_TOOL(HAVE_PKG, pkg-config)

		if test ! x$HAVE_PKG = x ; then
			GTK_CFLAGS=`$HAVE_PKG gtk+-2.0 --cflags`
			GTK_LIBS=`$HAVE_PKG gtk+-2.0 --libs`
		fi

		if test x$HAVE_LIBSDL = x ; then
        		AC_MSG_ERROR([You need to install SDL])
		else
		        SDL_CFLAGS=`$HAVE_LIBSDL --cflags`
		        SDL_LIBS=`$HAVE_LIBSDL --libs`
			GTK_CFLAGS="$GTK_CFLAGS $SDL_CFLAGS"
			GTK_LIBS="$GTK_LIBS $SDL_LIBS"
		fi
		AC_SUBST(SDL_CFLAGS)
		AC_SUBST(SDL_LIBS)
		AC_SUBST(GTK_CFLAGS)
		AC_SUBST(GTK_LIBS)

		UI_DIR="gtk cli"
		AC_SUBST([UI_DIR])
		;;
	*mingw*)
		LIBS="$LIBS -mwindows"
		UI_DIR=windows
		AC_SUBST([UI_DIR])
		;;
esac

AC_ARG_ENABLE(debug, AC_HELP_STRING(--enable-debug, enable debug information) , AC_DEFINE(DEBUG))

AC_CONFIG_FILES([Makefile
	src/Makefile
	src/cli/Makefile
	src/gtk/Makefile
	src/windows/Makefile
])
AC_OUTPUT
