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

#import "screen_state.h"

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

- (NSImage*)image
{
	NSImage *result = [[NSImage alloc] initWithSize:[ScreenState size]];
	[result addRepresentation:[self imageRep]];
	return [result autorelease];
}

- (NSBitmapImageRep*)imageRep
{
	NSBitmapImageRep *image = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
	pixelsWide:DS_SCREEN_WIDTH
	pixelsHigh:DS_SCREEN_HEIGHT*2
	bitsPerSample:8
	samplesPerPixel:3
	hasAlpha:NO
	isPlanar:NO
	colorSpaceName:NSCalibratedRGBColorSpace
	bytesPerRow:DS_SCREEN_WIDTH*3
	bitsPerPixel:24];

	if(image == nil)return nil;

	unsigned char *bitmap_data = [image bitmapData];

	const unsigned short *buffer_16 = (unsigned short*)color_data;

	int i;
	for(i = 0; i < DS_SCREEN_WIDTH * DS_SCREEN_HEIGHT*2; i++)
	{ //this loop we go through pixel by pixel and convert from 16bit to 24bit for the NSImage
		*(bitmap_data++) = (*buffer_16 & 0x001F) <<  3;
		*(bitmap_data++) = (*buffer_16 & 0x03E0) >> 5 << 3;
		*(bitmap_data++) = (*buffer_16 & 0x7C00) >> 10 << 3;
		buffer_16++;
	}

	return [image autorelease];
}
@end