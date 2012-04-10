/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#import <Cocoa/Cocoa.h>
#include "videofilter.h"


/********************************************************************************************
	CocoaVideoFilter - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for the video filter C++ object.
	
	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
@interface CocoaVideoFilter : NSObject
{
	VideoFilter *vf;
}

- (id) initWithSize:(NSSize)theSize;
- (id) initWithSize:(NSSize)theSize typeID:(VideoFilterTypeID)typeID;
- (BOOL) setSourceSize:(NSSize)theSize;
- (BOOL) changeFilter:(VideoFilterTypeID)typeID;
- (UInt32 *) runFilter;
- (NSImage *) image;
- (NSBitmapImageRep *) bitmapImageRep;
- (VideoFilterTypeID) typeID;
- (NSString *) typeString;
- (UInt32 *) srcBufferPtr;
- (UInt32 *) destBufferPtr;
- (NSSize) srcSize;
- (NSSize) destSize;
+ (NSString *) typeStringByID:(VideoFilterTypeID)typeID;

@end
