/*
	Copyright (C) 2026 DeSmuME team

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

#ifndef _MF_BUILD_H_
#define _MF_BUILD_H_

// Whether the Media Foundation recorder is part of this build.
//
// It used to be a test for _MSC_VER, on the belief that only the Visual Studio
// SDK carries <mfapi.h>.  That is not true: MSYS2's clang64 toolchain ships the
// whole surface -- mfapi.h, mfreadwrite.h, mftransform.h, mfuuid.h -- together
// with libmfplat.a, libmfreadwrite.a and libmfuuid.a, and the recorder below
// only ever uses plain COM and explicit GUID constants (there is no __uuidof
// anywhere in it), so the C++ it is written in is not MSVC specific either.
//
// So the switch is a macro, not a compiler test:
//
//   MSVC          : on by default, exactly as before.
//   clang64       : pass -DDESMUME_ENABLE_MF=1 and link mfplat, mfreadwrite,
//                   mfuuid and ole32.  Nothing else has to change.
//   anything else : off by default, and File > Record Video stays hidden.
//
// Every place that used to be "#ifdef _MSC_VER" around recorder code now tests
// this macro, so one -D turns the whole feature on for a new toolchain instead
// of a hunt through five files.

#if !defined(DESMUME_ENABLE_MF)
#	if defined(_MSC_VER)
#		define DESMUME_ENABLE_MF 1
#	else
#		define DESMUME_ENABLE_MF 0
#	endif
#endif

#endif // _MF_BUILD_H_
