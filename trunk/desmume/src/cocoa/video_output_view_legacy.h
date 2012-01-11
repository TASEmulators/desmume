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

//This class uses OpenGL for drawing for speed
//if opengl is not available it uses NSImage

@class ScreenState;
@class CocoaDSController;

@interface VideoOutputView :
#ifdef HAVE_OPENGL
NSView
#else
NSImageView
#endif
{
#ifdef HAVE_OPENGL
	NSOpenGLContext* context;
#endif
	ScreenState *screen_buffer;
	double viewScale;
	double viewRotation;
	NSInteger displayMode;
	UInt32 gpuStateFlags;
	
	CocoaDSController *cdsController;
}
//init
- (id)initWithFrame:(NSRect)frame;

//image to display
- (void)setScreenState:(ScreenState*)screen;
- (const ScreenState*)screenState;

//size in pixels of screen display (disreguarding rotation of the view)
- (float)screenHeight;
- (float)screenWidth;

- (NSSize) normalSize;
- (void) setScale:(double)scalar;
- (double) scale;
- (void) setRotation:(double)angleDegrees;
- (double) rotation;
- (void) setDisplayMode:(NSInteger)theMode;
- (NSInteger) displayMode;
- (void) setGpuStateFlags:(UInt32)flags;
- (UInt32) gpuStateFlags;
- (void) setCdsController:(CocoaDSController *)theController;
- (CocoaDSController*) cdsController;

- (void) setViewToBlack;
- (void) setViewToWhite;
- (BOOL) gpuStateByBit:(UInt32)stateBit;
- (NSPoint) convertPointToDS:(NSPoint)touchLoc;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void SetGPULayerState(int displayType, unsigned int i, bool state);
bool GetGPULayerState(int displayType, unsigned int i);
void SetGPUDisplayState(int displayType, bool state);
bool GetGPUDisplayState(int displayType);

#ifdef __cplusplus
}
#endif
