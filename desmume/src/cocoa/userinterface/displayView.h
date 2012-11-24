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
- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID;
- (void) doDisplayOrderChanged:(NSInteger)displayOrderID;
- (void) doBilinearOutputChanged:(BOOL)useBilinear;
- (void) doVerticalSyncChanged:(BOOL)useVerticalSync;
- (void) doVideoFilterChanged:(NSInteger)videoFilterTypeID frameSize:(NSSize)videoFilterDestSize;

@end

@interface DisplayViewDelegate : NSObject <CocoaDSDisplayVideoDelegate>
{
	NSView <DisplayViewDelegate> *view;
	NSPort *sendPortDisplay;
	NSPort *sendPortInput;
	
	CocoaDSController *cdsController;
	
	BOOL isHudEnabled;
	BOOL isHudEditingModeEnabled;
	
	NSSize normalSize;
	NSMutableDictionary *bindings;
	
	OSSpinLock spinlockNormalSize;
	OSSpinLock spinlockGpuStateFlags;
	OSSpinLock spinlockScale;
	OSSpinLock spinlockRotation;
	OSSpinLock spinlockUseBilinearOutput;
	OSSpinLock spinlockUseVerticalSync;
	OSSpinLock spinlockDisplayType;
	OSSpinLock spinlockDisplayOrientation;
	OSSpinLock spinlockDisplayOrder;
}

@property (retain) NSView <DisplayViewDelegate> *view;
@property (retain) NSPort *sendPortInput;
@property (retain) CocoaDSController *cdsController;
@property (readonly) NSSize normalSize;
@property (assign) UInt32 gpuStateFlags;
@property (assign) double scale;
@property (assign) double rotation;
@property (assign) BOOL useBilinearOutput;
@property (assign) BOOL useVerticalSync;
@property (assign) NSInteger displayType;
@property (assign) NSInteger displayOrientation;
@property (assign) NSInteger displayOrder;
@property (readonly) NSMutableDictionary *bindings;

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
	GLint glTexRenderStyle;
	GLenum glTexPixelFormat;
	GLvoid *glTexBack;
	NSSize glTexBackSize;
	
	GLuint swRasterizerDrawTexture[2];
	GLfloat swRasterizerMainTexCoord[4][2];
	GLfloat swRasterizerMainVertex[4][2];
	GLfloat swRasterizerTouchTexCoord[4][2];
	GLfloat swRasterizerTouchVertex[4][2];
	GLuint swRasterizerDisplayListIndex;
}

- (void) drawVideoFrame;
- (void) uploadSWRasterizerTextureData:(const GLvoid *)textureData textureSize:(NSSize)textureSize;
- (void) renderSWRasterizer;
- (void) setupSWRasterizerVertices;
- (void) updateDisplayLists;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void SetupOpenGLView(GLsizei width, GLsizei height, GLfloat scalar, GLfloat angleDegrees);
bool OSXOpenGLRendererInit();
bool OSXOpenGLRendererBegin();
void OSXOpenGLRendererEnd();

#ifdef __cplusplus
}
#endif
