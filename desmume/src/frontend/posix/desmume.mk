AM_CFLAGS = 
# Baseline C++ standard for the core.  Kept in AM_CXXFLAGS rather than CXXFLAGS
# so the JIT target can override it to -std=c++20 via a per-target flag.
AM_CXXFLAGS = -std=c++14
AM_CPPFLAGS = -I$(top_srcdir)/../../../src/ -I$(top_srcdir)/../../../src/libretro-common/include

#add this so that frontends can use "../types.h" for instance (they were built expecting to be in subdirectories of the main desmume dir
AM_CPPFLAGS += -I$(top_srcdir)/../../../src/frontend

AM_LDFLAGS =
