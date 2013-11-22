/*
	Copyright (C) 2007 Jeff Bland
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

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif

#define DS_BPP 2 //bytes per pixel
#define DS_SCREEN_X_RATIO (256.0 / (192.0 * 2.0))
#define DS_SCREEN_Y_RATIO ((192.0 * 2.0) / 256.0)
#define DS_SCREEN_WIDTH 256
#define DS_SCREEN_HEIGHT 192

//This class is used to return screen data at the end of a frame
//we wrap it in a obj-c class so it can be passed to a selector
//and so we get retain/release niftyness
@interface ScreenState : NSObject
{
	@private
	unsigned char color_data[DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP];
}
+ (NSInteger)width;
+ (NSInteger)height;
+ (NSSize)size;
+ (ScreenState*)blackScreenState;
+ (ScreenState*)whiteScreenState;
- (id)initWithBlack;
- (id)initWithWhite;
- (id)initWithScreenState:(ScreenState*)state;
- (id)initWithColorData:(const unsigned char*)data;
- (const unsigned char*)colorData;
- (NSImage*)image;
- (NSBitmapImageRep*)imageRep;
@end