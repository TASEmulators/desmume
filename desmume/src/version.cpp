/*
	Copyright (C) 2009-2016 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "types.h"
#include "version.h"

// Helper macros to convert numerics to strings
#if defined(_MSC_VER)
	//re: http://72.14.203.104/search?q=cache:HG-okth5NGkJ:mail.python.org/pipermail/python-checkins/2002-November/030704.html+_msc_ver+compiler+version+string&hl=en&gl=us&ct=clnk&cd=5
	#define _Py_STRINGIZE(X) _Py_STRINGIZE1((X))
	#define _Py_STRINGIZE1(X) _Py_STRINGIZE2 ## X
	#define _Py_STRINGIZE2(X) #X

	#define TOSTRING(X) _Py_STRINGIZE(X) // Alias _Py_STRINGIZE so that we have a common macro name
#else
	#define STRINGIFY(x) #x
	#define TOSTRING(x) STRINGIFY(x)
#endif

//TODO - it isn't possible to build a core without a frontend, so really this belongs with frontend modules
//the only stuff that belongs in the core is major/minor/build versions which are (in principle) used for versioning savestates and movies and such..
#if defined(HOST_WINDOWS) || defined(DESMUME_COCOA)
	#include "scmrev.h"
	#define SVN_REV_STR SCM_DESC_STR
#else
	#ifndef SVN_REV
		#define SVN_REV_STR "0"
	#endif
#endif

#define DESMUME_NAME "DeSmuME"

#if defined(__x86_64__) || defined(__LP64) || defined(__IA64__) || defined(_M_X64) || defined(_WIN64) 
	#define DESMUME_PLATFORM_STRING " x64"
#elif defined(__i386__) || defined(_M_IX86) || defined(_WIN32)
	#define DESMUME_PLATFORM_STRING " x86"
#elif defined(__arm__)
	#define DESMUME_PLATFORM_STRING " ARM"
#elif defined(__thumb__)
	#define DESMUME_PLATFORM_STRING " ARM-Thumb"
#elif defined(__ppc64__)
	#define DESMUME_PLATFORM_STRING " PPC64"
#elif defined(__ppc__) || defined(_M_PPC)
	#define DESMUME_PLATFORM_STRING " PPC"
#else
	#define DESMUME_PLATFORM_STRING ""
#endif

#define DESMUME_CPUEXT_PRIMARY_STRING ""
#define DESMUME_CPUEXT_SECONDARY_STRING ""

#if defined(ENABLE_SSE4_2)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " SSE4.2"
#elif defined(ENABLE_SSE4_1)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " SSE4.1"
#elif defined(ENABLE_SSSE3)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " SSSE3"
#elif defined(ENABLE_SSE3)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " SSE3"
#elif defined(ENABLE_SSE2)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " SSE2"
#elif defined(ENABLE_SSE)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " SSE"
#elif defined(ENABLE_ALTIVEC)
	#undef DESMUME_CPUEXT_PRIMARY_STRING
	#define DESMUME_CPUEXT_PRIMARY_STRING " AltiVec"
#endif

#if defined(ENABLE_AVX2)
	#undef DESMUME_CPUEXT_SECONDARY_STRING
	#define DESMUME_CPUEXT_SECONDARY_STRING "+AVX2"
#elif defined(ENABLE_AVX)
	#undef DESMUME_CPUEXT_SECONDARY_STRING
	#define DESMUME_CPUEXT_SECONDARY_STRING "+AVX"
#endif

#define DESMUME_CPUEXT_STRING DESMUME_CPUEXT_PRIMARY_STRING DESMUME_CPUEXT_SECONDARY_STRING

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
	#define DESMUME_SUBVERSION_STRING " git#" SVN_REV_STR
#endif

#ifdef __INTEL_COMPILER
	#define DESMUME_COMPILER " (Intel)"
	#define DESMUME_COMPILER_DETAIL " (Intel v" TOSTRING(__INTEL_COMPILER) ")"
#elif defined(_MSC_VER)
	#define DESMUME_COMPILER " (MSVC)"
	#define DESMUME_COMPILER_DETAIL " (MSVC v" TOSTRING(_MSC_VER) ")"
#elif defined(__clang__)
	#define DESMUME_COMPILER " (LLVM-Clang)"
	#define DESMUME_COMPILER_DETAIL " (LLVM-Clang v" TOSTRING(__clang_major__) "." TOSTRING(__clang_minor__) "." TOSTRING(__clang_patchlevel__) ")"
#elif defined(__llvm__)
	#define DESMUME_COMPILER " (LLVM)"
	#define DESMUME_COMPILER_DETAIL " (LLVM)"
#elif defined(__GNUC__) // Always make GCC the last check, since other compilers, such as Clang, may define __GNUC__ internally.
	#define DESMUME_COMPILER " (GCC)"
	
	#if defined(__GNUC_PATCHLEVEL__)
		#define DESMUME_COMPILER_DETAIL " (GCC v" TOSTRING(__GNUC__) "." TOSTRING(__GNUC_MINOR__) "." TOSTRING(__GNUC_PATCHLEVEL__) ")"
	#else
		#define DESMUME_COMPILER_DETAIL " (GCC v" TOSTRING(__GNUC__) "." TOSTRING(__GNUC_MINOR__) ")"
	#endif
#else
	#define DESMUME_COMPILER ""
	#define DESMUME_COMPILER_DETAIL ""
#endif

#if defined(HAVE_JIT) && !defined(PUBLIC_RELEASE)
	#define DESMUME_JIT "-JIT"
#else
	#define DESMUME_JIT ""
#endif

const u8 DESMUME_VERSION_MAJOR = 0;
const u8 DESMUME_VERSION_MINOR = 9;
const u8 DESMUME_VERSION_BUILD = 12;

#define DESMUME_VERSION_NUMERIC 91200
#define DESMUME_VERSION_STRING " " "0.9.12" DESMUME_SUBVERSION_STRING DESMUME_FEATURE_STRING DESMUME_PLATFORM_STRING DESMUME_JIT DESMUME_CPUEXT_STRING
#define DESMUME_NAME_AND_VERSION DESMUME_NAME DESMUME_VERSION_STRING

u32 EMU_DESMUME_VERSION_NUMERIC() { return DESMUME_VERSION_NUMERIC; }
const char* EMU_DESMUME_VERSION_STRING() { return DESMUME_VERSION_STRING; }
const char* EMU_DESMUME_SUBVERSION_STRING() { return DESMUME_SUBVERSION_STRING; }
const char* EMU_DESMUME_NAME_AND_VERSION() { return DESMUME_NAME_AND_VERSION; }
const char* EMU_DESMUME_COMPILER_DETAIL() { return DESMUME_COMPILER_DETAIL; }
