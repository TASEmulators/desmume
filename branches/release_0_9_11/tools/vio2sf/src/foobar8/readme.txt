foobar2000 0.8 SDK readme

All components need to be linked to pfc, foobar2000_SDK, utf8api and to component_client - hint: use dependencies in msvc, *do*not* include all .cpp files in your project - that will drastically increase time needed to compile and cause weird errors to occur.
Included workspace has all project dependencies set up, have a look at it if you can't figure how to make things compile correctly.

This SDK is intended to work with 0.8 versions of foobar2000. It will not work with any earlier versions. You may need to recompile your components with newer version of this SDK in order to get them to work with post-0.8 versions of foobar2000. 

latest version of this SDK is available at http://www.foobar2000.org/

copyright stuff:

SSRC SuperEQ libraries (c) Naoki Shibata, http://shibatch.sourceforge.net/

mpglib library (c) 1995-99 Michael Hipp

mpc decoding library - (C) 1999-2002 Buschmann/Klemm/Piecha/Wolf

tagread.cpp tag manipulating routines by Case

SPC player sourcecode available at http://www.foobar2000.org/foo_spc_src.zip

akrip.dll source Copyright (C) 1999 Jay A. Key

JNetLib copyright (C) 2000-2001 Nullsoft, Inc.
