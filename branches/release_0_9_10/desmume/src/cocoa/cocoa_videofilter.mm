/*
	Copyright (C) 2011 Roger Manuel
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

#import "cocoa_videofilter.h"
#import <Cocoa/Cocoa.h>
#include "utilities.h"

@implementation CocoaVideoFilter

- (id)init
{
	return [self initWithSize:NSMakeSize(1, 1) typeID:VideoFilterTypeID_None numberThreads:0];
}

- (id) initWithSize:(NSSize)theSize
{
	return [self initWithSize:theSize typeID:VideoFilterTypeID_None numberThreads:0];
}

- (id) initWithSize:(NSSize)theSize typeID:(VideoFilterTypeID)typeID
{
	return [self initWithSize:theSize typeID:typeID numberThreads:0];
}

- (id) initWithSize:(NSSize)theSize typeID:(VideoFilterTypeID)typeID numberThreads:(NSUInteger)numThreads
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	vf = new VideoFilter((size_t)theSize.width, (size_t)theSize.height, typeID, numThreads);
	currentFilterType = typeID;
	
	return self;
}

- (void)dealloc
{
	delete vf;
	[super dealloc];
}

- (BOOL) setSourceSize:(NSSize)theSize
{
	BOOL result = NO;
	
	bool cResult = vf->SetSourceSize((size_t)theSize.width, (size_t)theSize.height);
	if (cResult)
	{
		result = YES;
	}
	
	return result;
}

- (BOOL) changeFilter:(VideoFilterTypeID)typeID
{
	BOOL result = NO;
	
	if (typeID == currentFilterType)
	{
		result = YES;
	}
	else
	{
		bool cResult = vf->ChangeFilterByID(typeID);
		if (cResult)
		{
			result = YES;
			currentFilterType = typeID;
		}
	}
	
	return result;
}

- (UInt32 *) runFilter
{
	return (UInt32 *)vf->RunFilter();
}

- (NSImage *) image
{
	NSImage *newImage = [[NSImage alloc] initWithSize:[self destSize]];
	if (newImage == nil)
	{
		return newImage;
	}
	
	NSBitmapImageRep *newImageRep = [self bitmapImageRep];
	if (newImageRep == nil)
	{
		[newImage release];
		newImage = nil;
		return newImage;
	}
	
	[newImage addRepresentation:newImageRep];
	
	return [newImage autorelease];
}

- (NSBitmapImageRep *) bitmapImageRep
{
	NSUInteger w = (NSUInteger)vf->GetDstWidth();
	NSUInteger h = (NSUInteger)vf->GetDstHeight();
	
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
	
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	RGBA8888ForceOpaqueBuffer((const uint32_t *)[self runFilter], bitmapData, (w * h));
	
#ifdef __BIG_ENDIAN__
	uint32_t *bitmapDataEnd = bitmapData + (w * h);
	while (bitmapData < bitmapDataEnd)
	{
		*bitmapData++ = CFSwapInt32LittleToHost(*bitmapData);
	}
#endif
	
	return [imageRep autorelease];
}

- (VideoFilterTypeID) typeID
{
	return vf->GetTypeID();
}

- (NSString *) typeString
{
	return [NSString stringWithCString:vf->GetTypeString() encoding:NSUTF8StringEncoding];
}

- (UInt32 *) srcBufferPtr
{
	return (UInt32 *)vf->GetSrcBufferPtr();
}

- (UInt32 *) dstBufferPtr
{
	return (UInt32 *)vf->GetDstBufferPtr();
}

- (NSSize) srcSize
{
	return NSMakeSize((CGFloat)vf->GetSrcWidth(), (CGFloat)vf->GetSrcHeight());
}

- (NSSize) destSize
{
	return NSMakeSize((CGFloat)vf->GetDstWidth(), (CGFloat)vf->GetDstHeight());
}

- (VideoFilterParamType) filterParameterType:(VideoFilterParamID)paramID
{
	return vf->GetFilterParameterType(paramID);
}

- (int) filterParameteri:(VideoFilterParamID)paramID
{
	return vf->GetFilterParameteri(paramID);
}

- (unsigned int) filterParameterui:(VideoFilterParamID)paramID
{
	return vf->GetFilterParameterui(paramID);
}

- (float) filterParameterf:(VideoFilterParamID)paramID
{
	return vf->GetFilterParameterf(paramID);
}

- (void) setFilterParameter:(VideoFilterParamID)paramID intValue:(int)value
{
	vf->SetFilterParameteri(paramID, value);
}

- (void) setFilterParameter:(VideoFilterParamID)paramID uintValue:(unsigned int)value
{
	vf->SetFilterParameterui(paramID, value);
}

- (void) setFilterParameter:(VideoFilterParamID)paramID floatValue:(float)value
{
	vf->SetFilterParameterf(paramID, value);
}

+ (NSString *) typeStringByID:(VideoFilterTypeID)typeID
{
	const char *vfTypeCString = VideoFilter::GetTypeStringByID(typeID);
	NSString *vfTypeString = [NSString stringWithCString:vfTypeCString encoding:NSUTF8StringEncoding];
	
	return vfTypeString;
}

@end
