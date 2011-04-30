/*  Copyright (C) 2007 Jeff Bland

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#import <Cocoa/Cocoa.h>

#ifdef __cplusplus
extern "C"
{
#endif

void messageDialog(NSString *title, NSString *text);
BOOL messageDialogYN(NSString *title, NSString *text);
NSString* openDialog(NSArray *file_types);
BOOL CreateAppDirectory(NSString *directoryName);
NSString* GetPathUserAppSupport(NSString *directoryName);

//
#if !defined(__LP64__) && !defined(NS_BUILD_32_LIKE_64)
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

// Taken from CIVector.h of the Mac OS X 10.5 SDK.
// Defines CGFloat for Mac OS X 10.4 and earlier.
#ifndef CGFLOAT_DEFINED
	typedef float CGFloat;
# define CGFLOAT_MIN FLT_MIN
# define CGFLOAT_MAX FLT_MAX
# define CGFLOAT_IS_DOUBLE 0
# define CGFLOAT_DEFINED 1
#endif

#ifdef __cplusplus
}
#endif