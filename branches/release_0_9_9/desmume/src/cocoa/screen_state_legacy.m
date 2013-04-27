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

#import "screen_state_legacy.h"
#import "cocoa_util.h"

@implementation ScreenState
+ (NSInteger)width
{
	return DS_SCREEN_WIDTH;
}

+ (NSInteger)height
{
	return DS_SCREEN_HEIGHT*2;
}

+ (NSSize)size
{
	return NSMakeSize(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT*2);
}

+ (ScreenState*)blackScreenState
{
	return [[[ScreenState alloc] initWithBlack] autorelease];
}

+ (ScreenState*)whiteScreenState;
{
	return [[[ScreenState alloc] initWithWhite] autorelease];
}

- (id)init
{
	//make sure we go through through the designated init function
	[self doesNotRecognizeSelector:_cmd];
	return nil;
}

- (id)initWithBlack
{
	self = [super init];
	if(self)memset(color_data, 0, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
	return self;
}

- (id)initWithWhite
{
	self = [super init];
	if(self)memset(color_data, 255, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
	return self;
}

- (id)initWithScreenState:(ScreenState*)state
{
	self = [super init];
	if(self)memcpy(color_data, state->color_data, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
	return self;
}

- (id)initWithColorData:(const unsigned char*)data
{
	self = [super init];
	if(self)memcpy(color_data, data, DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2 * DS_BPP);
	return self;
}

- (const unsigned char*)colorData
{
	return color_data;
}

- (NSImage *)image
{
	NSImage *newImage = [[NSImage alloc] initWithSize:[ScreenState size]];
	if (newImage == nil)
	{
		return newImage;
	}
	
	// Render the frame in an NSBitmapImageRep
	NSBitmapImageRep *newImageRep = [self imageRep];
	if (newImageRep == nil)
	{
		[newImage release];
		newImage = nil;
		return newImage;
	}
	
	// Attach the rendered frame to the NSImageRep
	[newImage addRepresentation:newImageRep];
	
	return newImage;
}

- (NSBitmapImageRep *)imageRep
{
	if (color_data == nil)
	{
		return nil;
	}
	
	NSUInteger w = DS_SCREEN_WIDTH;
	NSUInteger h = DS_SCREEN_HEIGHT * 2;
	NSBitmapImageRep *imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		 pixelsWide:w
																		 pixelsHigh:h
																	  bitsPerSample:8
																	samplesPerPixel:4
																		   hasAlpha:YES
																		   isPlanar:NO
																	 colorSpaceName:NSCalibratedRGBColorSpace
																		bytesPerRow:w * 4
																	   bitsPerPixel:32];
	
	if(imageRep == nil)
	{
		return imageRep;
	}
	
	UInt32 *bitmapData = (UInt32 *)[imageRep bitmapData];
	RGB555ToRGBA8888Buffer((const uint16_t *)color_data, (uint32_t *)bitmapData, (w * h));
	
#ifdef __BIG_ENDIAN__
	UInt32 *bitmapDataEnd = bitmapData + (w * h);
	
	while (bitmapData < bitmapDataEnd)
	{
		*bitmapData++ = CFSwapInt32LittleToHost(*bitmapData);
	}
#endif
	
	return [imageRep autorelease];
}

@end
