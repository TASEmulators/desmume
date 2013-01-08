/*
	Copyright (C) 2013 DeSmuME team

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

#ifndef MACOSX_10_4_COMPATIBILITY_H
#define MACOSX_10_4_COMPATIBILITY_H

// Taken from NSObjCRuntime.h of the Mac OS X v10.5 SDK.
// Defines NSInteger and NSUInteger for Mac OS X v10.4 and earlier.
#ifndef NSINTEGER_DEFINED
#define NSINTEGER_DEFINED 1
typedef int NSInteger;
typedef unsigned int NSUInteger;
#define NSIntegerMax    LONG_MAX
#define NSIntegerMin    LONG_MIN
#define NSUIntegerMax   ULONG_MAX	
#endif

// Taken from CIVector.h of the Mac OS X v10.5 SDK.
// Defines CGFloat for Mac OS X v10.4 and earlier.
#ifndef CGFLOAT_DEFINED
#define CGFLOAT_DEFINED 1
typedef float CGFloat;
#define CGFLOAT_MIN FLT_MIN
#define CGFLOAT_MAX FLT_MAX
#define CGFLOAT_IS_DOUBLE 0
#endif

#endif // MACOSX_10_4_COMPATIBILITY_H
