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
#include <OpenGL/OpenGL.h>
#include <libkern/OSAtomic.h>

#import "cocoa_output.h"

@class DisplayViewDelegate;
@class CocoaDSController;


@protocol DisplayViewDelegate

@required
- (void) doInitVideoOutput:(NSDictionary *)properties;
- (void) doProcessVideoFrame:(const void *)videoFrameData frameSize:(NSSize)frameSize;

@property (retain) DisplayViewDelegate *dispViewDelegate;

@optional
- (void) doResizeView:(NSRect)rect;
- (void) doRedraw;
- (void) doDisplayTypeChanged:(NSInteger)displayTypeID;
- (void) doBilinearOutputChanged:(BOOL)useBilinear;
- (void) doVideoFilterChanged:(NSInteger)videoFilterTypeID;

@end

@interface DisplayViewDelegate : NSObject <CocoaDSDisplayDelegate>
{
	NSView <DisplayViewDelegate> *view;
	NSPort *sendPortDisplay;
	NSPort *sendPortInput;
	
	CocoaDSController *cdsController;
	
	BOOL isHudEnabled;
	BOOL isHudEditingModeEnabled;
	
	NSSize normalSize;
	NSMutableDictionary *bindings;
	
	OSSpinLock spinlockGpuStateFlags;
	OSSpinLock spinlockDisplayType;
	OSSpinLock spinlockNormalSize;
	OSSpinLock spinlockScale;
	OSSpinLock spinlockRotation;
	OSSpinLock spinlockUseBilinearOutput;
}

@property (retain) NSView <DisplayViewDelegate> *view;
@property (retain) NSPort *sendPortInput;
@property (retain) CocoaDSController *cdsController;
@property (readonly) NSSize normalSize;
@property (assign) double scale;
@property (assign) double rotation;
@property (assign) BOOL useBilinearOutput;
@property (assign) NSInteger displayType;
@property (readonly) NSMutableDictionary *bindings;

- (void) setGpuStateFlags:(UInt32)flags;
- (UInt32) gpuStateFlags;
- (void) setScale:(double)s;
- (double) scale;
- (void) setRotation:(double)angleDegrees;
- (double) rotation;
- (void) setUseBilinearOutput:(BOOL)theState;
- (BOOL) useBilinearOutput;
- (void) setDisplayType:(NSInteger)theType;
- (NSInteger) displayType;
- (void) setVideoFilterType:(NSInteger)theType;
- (void) setRender3DRenderingEngine:(NSInteger)methodID;
- (void) setRender3DHighPrecisionColorInterpolation:(BOOL)state;
- (void) setRender3DEdgeMarking:(BOOL)state;
- (void) setRender3DFog:(BOOL)state;
- (void) setRender3DTextures:(BOOL)state;
- (void) setRender3DDepthComparisonThreshold:(NSUInteger)threshold;
- (void) setRender3DThreads:(NSUInteger)numberThreads;
- (void) setRender3DLineHack:(BOOL)state;
- (void) setViewToBlack;
- (void) setViewToWhite;
- (void) requestScreenshot:(NSURL *)fileURL fileType:(NSBitmapImageFileType)fileType;
- (void) copyToPasteboard;
- (BOOL) gpuStateByBit:(UInt32)stateBit;
- (NSPoint) dsPointFromEvent:(NSEvent *)theEvent;
- (NSPoint) convertPointToDS:(NSPoint)clickLoc;
- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed;

@end

@interface ImageDisplayView : NSImageView <DisplayViewDelegate>
{
	DisplayViewDelegate *dispViewDelegate;
	NSImage *imageData;
	NSBitmapImageRep *currentImageRep;
}

- (NSBitmapImageRep *) bitmapImageRep:(const void *)videoFrameData imageSize:(NSSize)imageSize;

@end

@interface OpenGLDisplayView : NSOpenGLView <DisplayViewDelegate>
{
	DisplayViewDelegate *dispViewDelegate;
	NSSize lastFrameSize;
	GLint glTexRenderStyle;
	GLenum glTexPixelFormat;
	GLvoid *glTexBack;
	NSSize glTexBackSize;
	GLuint drawTexture[1];
}

- (void) drawVideoFrame;
- (void) uploadFrameTexture:(const GLvoid *)frameBytes textureSize:(NSSize)textureSize;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void SetupOpenGLView(GLsizei width, GLsizei height, GLfloat scalar, GLfloat angleDegrees);

#ifdef __cplusplus
}
#endif
