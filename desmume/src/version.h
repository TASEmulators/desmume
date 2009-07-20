/*  Copyright (C) 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/


//todo - everyone will want to support this eventually, i suppose
#ifdef _MSC_VER
#include "svnrev.h"
#else
#ifdef SVN_REV
#define SVN_REV_STR SVN_REV
#else
#define SVN_REV_STR ""
#endif
#endif

#define DESMUME_NAME "DeSmuME"

#ifdef _WIN64
#define DESMUME_PLATFORM_STRING " x64"
#else
#ifdef _WIN32
#define DESMUME_PLATFORM_STRING " x86"
#else
#define DESMUME_PLATFORM_STRING ""
#endif
#endif

#ifndef ENABLE_SSE2
	#ifndef ENABLE_SSE
		#define DESMUME_CPUEXT_STRING " NOSSE"
	#else
		#define DESMUME_CPUEXT_STRING " NOSSE2"
	#endif
#else
#define DESMUME_CPUEXT_STRING ""
#endif

#ifdef DEVELOPER
#define DESMUME_FEATURE_STRING " dev+"
#else
#define DESMUME_FEATURE_STRING ""
#endif

#ifdef DEBUG
#define DESMUME_SUBVERSION_STRING " debug"
#elif defined(PUBLIC_RELEASE)
#define DESMUME_SUBVERSION_STRING ""
#else
#define DESMUME_SUBVERSION_STRING " svn" SVN_REV_STR
#endif

#ifdef __INTEL_COMPILER
#define DESMUME_COMPILER " (Intel) "
#define DESMUME_COMPILER_DETAIL " (Intel) "
#elif defined(_MSC_VER)
#define DESMUME_COMPILER ""
#define DESMUME_COMPILER_DETAIL " msvc " _Py_STRINGIZE(_MSC_VER)
#define _Py_STRINGIZE(X) _Py_STRINGIZE1((X))
#define _Py_STRINGIZE1(X) _Py_STRINGIZE2 ## X
#define _Py_STRINGIZE2(X) #X
//re: http://72.14.203.104/search?q=cache:HG-okth5NGkJ:mail.python.org/pipermail/python-checkins/2002-November/030704.html+_msc_ver+compiler+version+string&hl=en&gl=us&ct=clnk&cd=5
#else
// TODO: make for others compilers
#define DESMUME_COMPILER ""
#define DESMUME_COMPILER_DETAIL ""
#endif

#define DESMUME_VERSION_NUMERIC 90500
#define DESMUME_VERSION_STRING " " "0.9.5" DESMUME_SUBVERSION_STRING DESMUME_FEATURE_STRING DESMUME_PLATFORM_STRING DESMUME_CPUEXT_STRING DESMUME_COMPILER
#define DESMUME_NAME_AND_VERSION " " DESMUME_NAME DESMUME_VERSION_STRING
