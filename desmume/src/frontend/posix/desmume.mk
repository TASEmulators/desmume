AM_CFLAGS = 
AM_CPPFLAGS = -I$(top_srcdir)/../../../src/ -I$(top_srcdir)/../../../src/libretro-common/include

#add this so that frontends can use "../types.h" for instance (they were built expecting to be in subdirectories of the main desmume dir
AM_CPPFLAGS += -I$(top_srcdir)/../../../src/frontend

AM_LDFLAGS =
