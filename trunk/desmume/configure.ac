AC_INIT(desmume, 0.33)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(desmume, 0.33)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_LANG(C)

AC_C_BIGENDIAN

AC_PATH_PROGS(HAVE_LIBSDL, [sdl-config sdl11-config])

if test x$HAVE_LIBSDL = x ; then
        AC_MSG_ERROR([You need to install SDL])
else
        SDL_CFLAGS=`$HAVE_LIBSDL --cflags`
        SDL_LIBS=`$HAVE_LIBSDL --libs`
	CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
	LIBS="$LIBS $SDL_LIBS"
fi


AC_CHECK_TOOL(HAVE_PKG, pkg-config)

if test ! x$HAVE_PKG = x ; then
	GTK_CFLAGS=`$HAVE_PKG gtk+-2.0 --cflags`
	GTK_LIBS=`$HAVE_PKG gtk+-2.0 --libs`
	AC_SUBST(GTK_CFLAGS)
	AC_SUBST(GTK_LIBS)
fi

AC_CONFIG_FILES([Makefile
	src/Makefile
	src/gtk/Makefile
])
AC_OUTPUT
