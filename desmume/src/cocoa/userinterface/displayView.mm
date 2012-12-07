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

#import "displayView.h"
#import "emuWindowDelegate.h"

#import "cocoa_input.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#undef BOOL

CGLContextObj OSXOpenGLRendererContext = NULL;

@implementation DisplayViewDelegate

@synthesize view;
@synthesize sendPortDisplay;
@synthesize sendPortInput;
@synthesize cdsController;
@synthesize isHudEnabled;
@synthesize isHudEditingModeEnabled;
@dynamic normalSize;
@dynamic gpuStateFlags;
@dynamic scale;
@dynamic rotation;
@dynamic useBilinearOutput;
@dynamic useVerticalSync;
@dynamic displayType;
@dynamic displayOrientation;
@dynamic displayOrder;
@synthesize bindings;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	bindings = [[NSMutableDictionary alloc] init];
	if (bindings == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	view = nil;
	spinlockNormalSize = OS_SPINLOCK_INIT;
	spinlockGpuStateFlags = OS_SPINLOCK_INIT;
	spinlockScale = OS_SPINLOCK_INIT;
	spinlockRotation = OS_SPINLOCK_INIT;
	spinlockUseBilinearOutput = OS_SPINLOCK_INIT;
	spinlockUseVerticalSync = OS_SPINLOCK_INIT;
	spinlockDisplayType = OS_SPINLOCK_INIT;
	spinlockDisplayOrientation = OS_SPINLOCK_INIT;
	spinlockDisplayOrder = OS_SPINLOCK_INIT;
	
	normalSize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2.0);
	sendPortDisplay = nil;
	cdsController = nil;
	isHudEnabled = NO;
	isHudEditingModeEnabled = NO;
	
	UInt32 gpuStateFlags =	GPUSTATE_MAIN_GPU_MASK |
	GPUSTATE_MAIN_BG0_MASK |
	GPUSTATE_MAIN_BG1_MASK |
	GPUSTATE_MAIN_BG2_MASK |
	GPUSTATE_MAIN_BG3_MASK |
	GPUSTATE_MAIN_OBJ_MASK |
	GPUSTATE_SUB_GPU_MASK |
	GPUSTATE_SUB_BG0_MASK |
	GPUSTATE_SUB_BG1_MASK |
	GPUSTATE_SUB_BG2_MASK |
	GPUSTATE_SUB_BG3_MASK |
	GPUSTATE_SUB_OBJ_MASK;
	
	[bindings setValue:[NSNumber numberWithInt:gpuStateFlags] forKey:@"GpuStateFlags"];
	[bindings setValue:[NSNumber numberWithDouble:1.0] forKey:@"scale"];
	[bindings setValue:[NSNumber numberWithDouble:0.0] forKey:@"rotation"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"useBilinearOutput"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"useVerticalSync"];
	[bindings setValue:[NSNumber numberWithInteger:DS_DISPLAY_TYPE_COMBO] forKey:@"displayMode"];
	[bindings setValue:@"Combo" forKey:@"displayModeString"];
	[bindings setValue:[NSNumber numberWithInteger:VideoFilterTypeID_None] forKey:@"videoFilterType"];
	[bindings setValue:[CocoaVideoFilter typeStringByID:VideoFilterTypeID_None] forKey:@"videoFilterTypeString"];
	[bindings setValue:[NSNumber numberWithInteger:CORE3DLIST_NULL] forKey:@"render3DRenderingEngine"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"render3DHighPrecisionColorInterpolation"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"render3DEdgeMarking"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"render3DFog"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"render3DTextures"];
	[bindings setValue:[NSNumber numberWithInteger:0] forKey:@"render3DDepthComparisonThreshold"];
	[bindings setValue:[NSNumber numberWithInteger:0] forKey:@"render3DThreads"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"render3DLineHack"];
	
    return self;
}

- (void)dealloc
{
	self.view = nil;
	self.cdsController = nil;
	[bindings release];
	
	[super dealloc];
}

- (NSSize) normalSize
{
	OSSpinLockLock(&spinlockNormalSize);
	NSSize theSize = normalSize;
	OSSpinLockUnlock(&spinlockNormalSize);
	
	return theSize;
}

- (void) setGpuStateFlags:(UInt32)flags
{
	OSSpinLockLock(&spinlockGpuStateFlags);
	[bindings setValue:[NSNumber numberWithInt:flags] forKey:@"GpuStateFlags"];
	OSSpinLockUnlock(&spinlockGpuStateFlags);
	
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_SET_GPU_STATE_FLAGS integerValue:flags];
}

- (UInt32) gpuStateFlags
{
	OSSpinLockLock(&spinlockGpuStateFlags);
	UInt32 flags = [[bindings valueForKey:@"GpuStateFlags"] intValue];
	OSSpinLockUnlock(&spinlockGpuStateFlags);
	
	return flags;
}

- (void) setScale:(double)s
{
	OSSpinLockLock(&spinlockScale);
	[bindings setValue:[NSNumber numberWithDouble:s] forKey:@"scale"];
	OSSpinLockUnlock(&spinlockScale);
}

- (double) scale
{
	OSSpinLockLock(&spinlockScale);
	double s = [[bindings valueForKey:@"scale"] doubleValue];
	OSSpinLockUnlock(&spinlockScale);
	
	return s;
}

- (void) setRotation:(double)angleDegrees
{
	OSSpinLockLock(&spinlockRotation);
	[bindings setValue:[NSNumber numberWithDouble:angleDegrees] forKey:@"rotation"];
	OSSpinLockUnlock(&spinlockRotation);
}

- (double) rotation
{
	OSSpinLockLock(&spinlockRotation);
	double angleDegrees = [[bindings valueForKey:@"rotation"] doubleValue];
	OSSpinLockUnlock(&spinlockRotation);
	
	return angleDegrees;
}

- (void) setUseBilinearOutput:(BOOL)theState
{
	OSSpinLockLock(&spinlockUseBilinearOutput);
	[bindings setValue:[NSNumber numberWithBool:theState] forKey:@"useBilinearOutput"];
	OSSpinLockUnlock(&spinlockUseBilinearOutput);
	
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_CHANGE_BILINEAR_OUTPUT boolValue:theState];
}

- (BOOL) useBilinearOutput
{
	OSSpinLockLock(&spinlockUseBilinearOutput);
	BOOL theState = [[bindings valueForKey:@"useBilinearOutput"] boolValue];
	OSSpinLockUnlock(&spinlockUseBilinearOutput);
	
	return theState;
}

- (void) setUseVerticalSync:(BOOL)theState
{
	OSSpinLockLock(&spinlockUseVerticalSync);
	[bindings setValue:[NSNumber numberWithBool:theState] forKey:@"useVerticalSync"];
	OSSpinLockUnlock(&spinlockUseVerticalSync);
	
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_CHANGE_VERTICAL_SYNC boolValue:theState];
}

- (BOOL) useVerticalSync
{
	OSSpinLockLock(&spinlockUseVerticalSync);
	BOOL theState = [[bindings valueForKey:@"useVerticalSync"] boolValue];
	OSSpinLockUnlock(&spinlockUseVerticalSync);
	
	return theState;
}

- (void) setDisplayType:(NSInteger)theType
{
	NSSize newDisplaySize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT);
	NSString *modeString = @"Unknown";
	
	switch (theType)
	{
		case DS_DISPLAY_TYPE_MAIN:
			modeString = NSSTRING_DISPLAYMODE_MAIN;
			break;
			
		case DS_DISPLAY_TYPE_TOUCH:
			modeString = NSSTRING_DISPLAYMODE_TOUCH;
			break;
			
		case DS_DISPLAY_TYPE_COMBO:
			modeString = NSSTRING_DISPLAYMODE_COMBO;
			
			if ([self displayOrientation] == DS_DISPLAY_ORIENTATION_VERTICAL)
			{
				newDisplaySize.height *= 2;
			}
			else
			{
				newDisplaySize.width *= 2;
			}
			
			break;
			
		default:
			break;
	}
	
	OSSpinLockLock(&spinlockDisplayType);
	[bindings setValue:[NSNumber numberWithInteger:theType] forKey:@"displayMode"];
	[bindings setValue:modeString forKey:@"displayModeString"];
	OSSpinLockUnlock(&spinlockDisplayType);
	
	OSSpinLockLock(&spinlockNormalSize);
	normalSize = newDisplaySize;
	OSSpinLockUnlock(&spinlockNormalSize);
	
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_CHANGE_DISPLAY_TYPE integerValue:theType];
}

- (NSInteger) displayType
{
	OSSpinLockLock(&spinlockDisplayType);
	NSInteger theType = [[bindings valueForKey:@"displayMode"] integerValue];
	OSSpinLockUnlock(&spinlockDisplayType);
	
	return theType;
}

- (void) setDisplayOrientation:(NSInteger)theOrientation
{
	OSSpinLockLock(&spinlockDisplayOrientation);
	[bindings setValue:[NSNumber numberWithInteger:theOrientation] forKey:@"displayOrientation"];
	OSSpinLockUnlock(&spinlockDisplayOrientation);
	
	if ([self displayType] == DS_DISPLAY_TYPE_COMBO)
	{
		NSSize newDisplaySize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT);
		
		if (theOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			newDisplaySize.height *= 2;
		}
		else
		{
			newDisplaySize.width *= 2;
		}
		
		OSSpinLockLock(&spinlockNormalSize);
		normalSize = newDisplaySize;
		OSSpinLockUnlock(&spinlockNormalSize);
	}
	
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_CHANGE_DISPLAY_ORIENTATION integerValue:theOrientation];
}

- (NSInteger) displayOrientation
{
	OSSpinLockLock(&spinlockDisplayOrientation);
	NSInteger theOrientation = [[bindings valueForKey:@"displayOrientation"] integerValue];
	OSSpinLockUnlock(&spinlockDisplayOrientation);
	
	return theOrientation;
}

- (void) setDisplayOrder:(NSInteger)theOrder
{
	OSSpinLockLock(&spinlockDisplayOrder);
	[bindings setValue:[NSNumber numberWithInteger:theOrder] forKey:@"displayOrder"];
	OSSpinLockUnlock(&spinlockDisplayOrder);
	
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_CHANGE_DISPLAY_ORDER integerValue:theOrder];
}

- (NSInteger) displayOrder
{
	OSSpinLockLock(&spinlockDisplayOrder);
	NSInteger theOrder = [[bindings valueForKey:@"displayOrder"] integerValue];
	OSSpinLockUnlock(&spinlockDisplayOrder);
	
	return theOrder;
}

- (void) setVideoFilterType:(NSInteger)theType
{
	[bindings setValue:[NSNumber numberWithInteger:theType] forKey:@"videoFilterType"];
	[bindings setValue:[CocoaVideoFilter typeStringByID:(VideoFilterTypeID)theType] forKey:@"videoFilterTypeString"];
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_CHANGE_VIDEO_FILTER integerValue:theType];
}

- (void) setRender3DRenderingEngine:(NSInteger)methodID
{
	[bindings setValue:[NSNumber numberWithInteger:methodID] forKey:@"render3DRenderingEngine"];
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_METHOD integerValue:methodID];
}

- (void) setRender3DHighPrecisionColorInterpolation:(BOOL)state
{
	[bindings setValue:[NSNumber numberWithBool:state] forKey:@"render3DHighPrecisionColorInterpolation"];
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_HIGH_PRECISION_COLOR_INTERPOLATION boolValue:state];
}

- (void) setRender3DEdgeMarking:(BOOL)state
{
	[bindings setValue:[NSNumber numberWithBool:state] forKey:@"render3DEdgeMarking"];
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_EDGE_MARKING boolValue:state];
}

- (void) setRender3DFog:(BOOL)state
{
	[bindings setValue:[NSNumber numberWithBool:state] forKey:@"render3DFog"];
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_FOG boolValue:state];
}

- (void) setRender3DTextures:(BOOL)state
{
	[bindings setValue:[NSNumber numberWithBool:state] forKey:@"render3DTextures"];
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_TEXTURES boolValue:state];
}

- (void) setRender3DDepthComparisonThreshold:(NSUInteger)threshold
{
	[bindings setValue:[NSNumber numberWithInteger:threshold] forKey:@"render3DDepthComparisonThreshold"];
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_DEPTH_COMPARISON_THRESHOLD integerValue:threshold];
}

- (void) setRender3DThreads:(NSUInteger)numberThreads
{
	[bindings setValue:[NSNumber numberWithInteger:numberThreads] forKey:@"render3DThreads"];
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_THREADS integerValue:numberThreads];
}

- (void) setRender3DLineHack:(BOOL)state
{
	[bindings setValue:[NSNumber numberWithBool:state] forKey:@"render3DLineHack"];
	[CocoaDSUtil messageSendOneWayWithBool:self.sendPortDisplay msgID:MESSAGE_SET_RENDER3D_LINE_HACK boolValue:state];
}

- (void) setViewToBlack
{
	[CocoaDSUtil messageSendOneWay:self.sendPortDisplay msgID:MESSAGE_SET_VIEW_TO_BLACK];
}

- (void) setViewToWhite
{
	[CocoaDSUtil messageSendOneWay:self.sendPortDisplay msgID:MESSAGE_SET_VIEW_TO_WHITE];
}

- (NSPoint) dsPointFromEvent:(NSEvent *)theEvent
{
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	NSPoint touchLoc = [theEvent locationInWindow];
	touchLoc = [view convertPoint:touchLoc fromView:nil];
	touchLoc = [self convertPointToDS:touchLoc];
	
	return touchLoc;
}

- (NSPoint) convertPointToDS:(NSPoint)clickLoc
{
	double viewAngle = [self rotation];
	if (viewAngle != 0.0)
	{
		viewAngle = CLOCKWISE_DEGREES(viewAngle);
	}
	
	NSPoint touchLoc = GetNormalPointFromTransformedPoint(clickLoc, self.normalSize, [[self view] bounds].size, [self scale], viewAngle);
	
	// Normalize the y-coordinate to the DS.
	if ([self displayType] == DS_DISPLAY_TYPE_COMBO)
	{
		NSInteger theOrientation = [self displayOrientation];
		NSInteger theOrder = [self displayOrder];
		
		if (theOrientation == DS_DISPLAY_ORIENTATION_VERTICAL && theOrder == DS_DISPLAY_ORDER_TOUCH_FIRST)
		{
			touchLoc.y -= GPU_DISPLAY_HEIGHT;
		}
		else if (theOrientation == DS_DISPLAY_ORIENTATION_HORIZONTAL && theOrder == DS_DISPLAY_ORDER_MAIN_FIRST)
		{
			touchLoc.x -= GPU_DISPLAY_WIDTH;
		}
	}
	
	touchLoc.y = GPU_DISPLAY_HEIGHT - touchLoc.y;
	
	// Constrain the touch point to the DS dimensions.
	if (touchLoc.x < 0)
	{
		touchLoc.x = 0;
	}
	else if (touchLoc.x > (GPU_DISPLAY_WIDTH - 1))
	{
		touchLoc.x = (GPU_DISPLAY_WIDTH - 1);
	}
	
	if (touchLoc.y < 0)
	{
		touchLoc.y = 0;
	}
	else if (touchLoc.y > (GPU_DISPLAY_HEIGHT - 1))
	{
		touchLoc.y = (GPU_DISPLAY_HEIGHT - 1);
	}
	
	return touchLoc;
}

- (void) requestScreenshot:(NSURL *)fileURL fileType:(NSBitmapImageFileType)fileType
{
	NSString *fileURLString = [fileURL absoluteString];
	NSData *fileURLStringData = [fileURLString dataUsingEncoding:NSUTF8StringEncoding];
	NSData *bitmapImageFileTypeData = [[NSData alloc] initWithBytes:&fileType length:sizeof(NSBitmapImageFileType)];
	NSArray *messageComponents = [[NSArray alloc] initWithObjects:fileURLStringData, bitmapImageFileTypeData, nil];
	
	[CocoaDSUtil messageSendOneWayWithMessageComponents:self.sendPortDisplay msgID:MESSAGE_REQUEST_SCREENSHOT array:messageComponents];
	
	[bitmapImageFileTypeData release];
	[messageComponents release];
}

- (void) copyToPasteboard
{
	[CocoaDSUtil messageSendOneWay:self.sendPortDisplay msgID:MESSAGE_COPY_TO_PASTEBOARD];
}

- (BOOL) gpuStateByBit:(UInt32)stateBit
{
	BOOL result = NO;
	UInt32 flags = [self gpuStateFlags];
	
	if (flags & (1 << stateBit))
	{
		result = YES;
	}
	
	return result;
}

- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed
{
	BOOL isHandled = NO;
	
	if (self.cdsController == nil)
	{
		return isHandled;
	}
	
	NSString *elementName = [NSString stringWithFormat:@"%d", [theEvent keyCode]];
	NSString *elementCode = elementName;
	NSDictionary *inputAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
									 @"NSEventKeyboard", @"deviceCode",
									 @"Keyboard", @"deviceName",
									 elementCode, @"elementCode",
									 elementName, @"elementName",
									 [NSNumber numberWithBool:keyPressed], @"on",
									 nil];
	
	if (keyPressed && [theEvent window] != nil && [[[theEvent window] delegate] respondsToSelector:@selector(setStatus:)])
	{
		[[[theEvent window] delegate] setStatus:[NSString stringWithFormat:@"Keyboard:%i", [theEvent keyCode]]];
	}
	
	isHandled = [self.cdsController setStateWithInput:inputAttributes];
	
	return isHandled;
}

- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed
{
	BOOL isHandled = NO;
	NSInteger dispType = [self displayType];
	
	if (self.cdsController == nil || (dispType != DS_DISPLAY_TYPE_TOUCH && dispType != DS_DISPLAY_TYPE_COMBO))
	{
		return isHandled;
	}
	
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	NSPoint touchLoc = [self dsPointFromEvent:theEvent];
	
	NSString *elementCode = [NSString stringWithFormat:@"%i", [theEvent buttonNumber]];
	NSString *elementName = [NSString stringWithFormat:@"Button %i", [theEvent buttonNumber]];
	
	switch ([theEvent buttonNumber])
	{
		case kCGMouseButtonLeft:
			elementName = @"Primary Button";
			break;
			
		case kCGMouseButtonRight:
			elementName = @"Secondary Button";
			break;
			
		case kCGMouseButtonCenter:
			elementName = @"Center Button";
			break;
			
		default:
			break;
	}
	
	NSDictionary *inputAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
									 @"NSEventMouse", @"deviceCode",
									 @"Mouse", @"deviceName",
									 elementCode, @"elementCode",
									 elementName, @"elementName",
									 [NSNumber numberWithBool:buttonPressed], @"on",
									 [NSNumber numberWithFloat:touchLoc.x], @"pointX",
									 [NSNumber numberWithFloat:touchLoc.y], @"pointY",
									 nil];
	
	if (buttonPressed && [theEvent window] != nil && [[[theEvent window] delegate] respondsToSelector:@selector(setStatus:)])
	{
		[[[theEvent window] delegate] setStatus:[NSString stringWithFormat:@"Mouse:%i X:%i Y:%i", [theEvent buttonNumber], (NSInteger)(touchLoc.x), (NSInteger)(touchLoc.y)]];
	}
	
	isHandled = [self.cdsController setStateWithInput:inputAttributes];
	
	return isHandled;
}

- (void) doInitVideoOutput:(NSDictionary *)properties
{
	[view doInitVideoOutput:properties];
}

- (void) doProcessVideoFrame:(const void *)videoFrameData frameSize:(NSSize)frameSize
{
	[view doProcessVideoFrame:videoFrameData frameSize:frameSize];
}

- (void) doResizeView:(NSRect)rect
{
	if (view == nil || ![view respondsToSelector:@selector(doResizeView:)])
	{
		return;
	}
	
	[view doResizeView:rect];
}

- (void) doRedraw
{
	if (view == nil || ![view respondsToSelector:@selector(doRedraw)])
	{
		return;
	}
	
	[view doRedraw];
}

- (void) doDisplayTypeChanged:(NSInteger)displayTypeID
{
	if (view == nil || ![view respondsToSelector:@selector(doDisplayTypeChanged:)])
	{
		return;
	}
	
	[view doDisplayTypeChanged:displayTypeID];
}

- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID
{
	if (view == nil || ![view respondsToSelector:@selector(doDisplayOrientationChanged:)])
	{
		return;
	}
	
	[view doDisplayOrientationChanged:displayOrientationID];
}

- (void) doDisplayOrderChanged:(NSInteger)displayOrderID
{
	if (view == nil || ![view respondsToSelector:@selector(doDisplayOrderChanged:)])
	{
		return;
	}
	
	[view doDisplayOrderChanged:displayOrderID];
}

- (void) doBilinearOutputChanged:(BOOL)useBilinear
{
	if (view == nil || ![view respondsToSelector:@selector(doBilinearOutputChanged:)])
	{
		return;
	}
	
	[view doBilinearOutputChanged:useBilinear];
}

- (void) doVerticalSyncChanged:(BOOL)useVerticalSync
{
	if (view == nil || ![view respondsToSelector:@selector(doVerticalSyncChanged:)])
	{
		return;
	}
	
	[view doVerticalSyncChanged:useVerticalSync];
}

- (void) doVideoFilterChanged:(NSInteger)videoFilterTypeID frameSize:(NSSize)videoFilterDestSize
{
	if (view == nil || ![view respondsToSelector:@selector(doVideoFilterChanged:frameSize:)])
	{
		return;
	}
	
	[view doVideoFilterChanged:videoFilterTypeID frameSize:videoFilterDestSize];
}

@end


@implementation ImageDisplayView

@synthesize dispViewDelegate;

- (id) init
{
	return [self initWithFrame:NSMakeRect(0.0f, 0.0f, GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2.0f)];
}

- (id) initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self == nil)
	{
		return self;
	}
	
	imageData = nil;
	currentImageRep = nil;
	
    return self;
}

- (void) dealloc
{
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
	[CocoaDSUtil messageSendOneWay:dispViewDelegate.sendPortDisplay msgID:MESSAGE_REDRAW_VIEW];
}

- (void) drawVideoFrame
{
	[self setImage:imageData];
	[self setNeedsDisplay];
}

- (void)setFrame:(NSRect)rect
{
	[super setFrame:rect];
	[CocoaDSUtil messageSendOneWayWithRect:dispViewDelegate.sendPortDisplay msgID:MESSAGE_RESIZE_VIEW rect:rect];
}

- (NSBitmapImageRep *) bitmapImageRep:(const void *)videoFrameData imageSize:(NSSize)imageSize
{
	if (videoFrameData == nil)
	{
		return nil;
	}
	
	NSUInteger w = (NSUInteger)imageSize.width;
	NSUInteger h = (NSUInteger)imageSize.height;
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
	
	if(!imageRep)
	{
		return nil;
	}
	
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	RGBA5551ToRGBA8888Buffer((const uint16_t *)videoFrameData, bitmapData, (w * h));
	
#ifdef __BIG_ENDIAN__
	uint32_t *bitmapDataEnd = bitmapData + (w * h);
	while (bitmapData < bitmapDataEnd)
	{
		*bitmapData++ = CFSwapInt32LittleToHost(*bitmapData);
	}
#endif
	
	return [imageRep autorelease];
}

- (void)keyDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleKeyPress:theEvent keyPressed:YES];
	if (!isHandled)
	{
		[super keyDown:theEvent];
	}
}

- (void)keyUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleKeyPress:theEvent keyPressed:NO];
	if (!isHandled)
	{
		[super keyUp:theEvent];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super mouseDown:theEvent];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	[self mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super mouseUp:theEvent];
	}
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super rightMouseDown:theEvent];
	}
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
	[self rightMouseDown:theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super rightMouseUp:theEvent];
	}
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super otherMouseDown:theEvent];
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	[self otherMouseDown:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super otherMouseUp:theEvent];
	}
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)becomeFirstResponder
{
	return YES;
}

- (BOOL)resignFirstResponder
{
	return YES;
}

- (void)doInitVideoOutput:(NSDictionary *)properties
{
	imageData = [[NSImage alloc] initWithSize:[dispViewDelegate normalSize]];
	currentImageRep = nil;
}

- (void)doProcessVideoFrame:(const void *)videoFrameData frameSize:(NSSize)frameSize
{
	// Render the frame in an NSBitmapImageRep
	NSBitmapImageRep *newImageRep = [self bitmapImageRep:videoFrameData imageSize:frameSize];
	if (newImageRep == nil)
	{
		return;
	}
	
	// Attach the rendered frame to the NSImageRep
	[imageData removeRepresentation:currentImageRep];
	[imageData addRepresentation:newImageRep];
	currentImageRep = newImageRep;
	
	// Draw the video frame.
	[self drawVideoFrame];
}

- (void)doRedraw
{
	[self drawVideoFrame];
}

@end

@implementation OpenGLDisplayView

@synthesize dispViewDelegate;

- (id)initWithCoder:(NSCoder *)aDecoder
{
	self = [super initWithCoder:aDecoder];
	if (self == nil)
	{
		return self;
	}
	
	dispViewDelegate = nil;
	glTexPixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	glTexRenderStyle = GL_LINEAR;
	
	UInt32 w = GetNearestPositivePOT((UInt32)GPU_DISPLAY_WIDTH);
	UInt32 h = GetNearestPositivePOT((UInt32)(GPU_DISPLAY_HEIGHT * 2.0));
	glTexBack = (GLvoid *)calloc(w * h, sizeof(UInt16));
	glTexBackSize = NSMakeSize(w, h);
	cglDisplayContext = (CGLContextObj)[[self openGLContext] CGLContextObj];
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	
	renderDisplayListIndex = glGenLists(3);
	glGenTextures(1, &mainDisplayTexIndex);
	glGenTextures(1, &touchDisplayTexIndex);
	glGenBuffers(1, &vboTexCoordID);
	glGenBuffers(1, &vboVertexID);
	glGenBuffers(1, &vboElementID);
	
	SetOpenGLRendererFunctions(&OSXOpenGLRendererInit,
							   &OSXOpenGLRendererBegin,
							   &OSXOpenGLRendererEnd);
	
	CGLSetCurrentContext(prevContext);
	
	NSOpenGLPixelFormatAttribute attrs[] =
	{
		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAStencilSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFAOffScreen,
		(NSOpenGLPixelFormatAttribute)0
	};
	
	NSOpenGLPixelFormat *tempPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	NSOpenGLPixelBuffer *tempPixelBuffer = [[NSOpenGLPixelBuffer alloc]
											initWithTextureTarget:GL_TEXTURE_2D
											textureInternalFormat:GL_RGBA
											textureMaxMipMapLevel:0
											pixelsWide:GPU_DISPLAY_WIDTH
											pixelsHigh:GPU_DISPLAY_HEIGHT*2];
	
	NSOpenGLContext *newOGLRendererContext = [[NSOpenGLContext alloc] initWithFormat:tempPixelFormat shareContext:nil];
	[newOGLRendererContext setPixelBuffer:tempPixelBuffer cubeMapFace:0 mipMapLevel:0 currentVirtualScreen:0];
	
	[tempPixelFormat release];
	[tempPixelBuffer release];
	
	oglRendererContext = newOGLRendererContext;
	OSXOpenGLRendererContext = (CGLContextObj)[oglRendererContext CGLContextObj];
	
    return self;
}

- (void)dealloc
{
	[oglRendererContext release];
	OSXOpenGLRendererContext = NULL;
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	
	glDeleteTextures(1, &mainDisplayTexIndex);
	glDeleteTextures(1, &touchDisplayTexIndex);
	glDeleteLists(renderDisplayListIndex, 3);
	glDeleteBuffers(1, &vboTexCoordID);
	glDeleteBuffers(1, &vboVertexID);
	glDeleteBuffers(1, &vboElementID);
	
	CGLSetCurrentContext(prevContext);
	
	free(glTexBack);
	glTexBack = NULL;
	
	self.dispViewDelegate = nil;
	
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
	[CocoaDSUtil messageSendOneWay:dispViewDelegate.sendPortDisplay msgID:MESSAGE_REDRAW_VIEW];
}

- (void)setFrame:(NSRect)rect
{
	NSRect oldFrame = [self frame];
	[super setFrame:rect];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glViewport(0, 0, rect.size.width, rect.size.height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-rect.size.width/2, -rect.size.width/2 + rect.size.width, -rect.size.height/2, -rect.size.height/2 + rect.size.height, -1.0, 1.0);
	
	CGLUnlockContext(cglDisplayContext);
	
	if (rect.size.width == oldFrame.size.width && rect.size.height == oldFrame.size.height)
	{
		[CocoaDSUtil messageSendOneWay:dispViewDelegate.sendPortDisplay msgID:MESSAGE_REDRAW_VIEW];
	}
	else
	{
		[CocoaDSUtil messageSendOneWayWithRect:dispViewDelegate.sendPortDisplay msgID:MESSAGE_RESIZE_VIEW rect:rect];
	}
}

- (void) prepareOpenGL
{
	glDisable(GL_DITHER);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_DEPTH_TEST);
	
	glEnable(GL_TEXTURE_2D);
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// Set up initial display vertices and store in VBO
	static const GLint vertices[8*2] =	{-GPU_DISPLAY_WIDTH/2, GPU_DISPLAY_HEIGHT,		// Top display, top left
										 GPU_DISPLAY_WIDTH/2, GPU_DISPLAY_HEIGHT,		// Top display, top right
										 GPU_DISPLAY_WIDTH/2, 0,						// Top display, bottom right
										 -GPU_DISPLAY_WIDTH/2, 0,						// Top display, bottom left
										 
										 -GPU_DISPLAY_WIDTH/2, 0,						// Bottom display, top left
										 GPU_DISPLAY_WIDTH/2, 0,						// Bottom display, top right
										 GPU_DISPLAY_WIDTH/2, -GPU_DISPLAY_HEIGHT,		// Bottom display, bottom right
										 -GPU_DISPLAY_WIDTH/2, -GPU_DISPLAY_HEIGHT};	// Bottom display, bottom left
	
	glBindBuffer(GL_ARRAY_BUFFER, vboVertexID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// Set up initial texture coordinates and store in VBO
	static const GLfloat texCoords[8*2] =	{0.0f, 0.0f,
											 1.0f, 0.0f,
											 1.0f, 1.0f,
											 0.0f, 1.0f,
											 
											 0.0f, 0.0f,
											 1.0f, 0.0f,
											 1.0f, 1.0f,
											 0.0f, 1.0f};
	
	glBindBuffer(GL_ARRAY_BUFFER, vboTexCoordID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// Set up initial vertex elements and store in VBO
	static const GLubyte elements[12] =	{0, 1, 2,
										 2, 3, 0,
										 
										 4, 5, 6,
										 6, 7, 4};
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboElementID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

- (void) drawVideoFrame
{
	CGLFlushDrawable(cglDisplayContext);
}

- (void) uploadDisplayTextures:(const GLvoid *)textureData textureSize:(NSSize)textureSize
{
	NSInteger displayType = [dispViewDelegate displayType];
	
	if (textureData == NULL)
	{
		return;
	}
	
	switch (displayType)
	{
		case DS_DISPLAY_TYPE_MAIN:
			glBindTexture(GL_TEXTURE_2D, mainDisplayTexIndex);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)textureSize.width, (GLsizei)textureSize.height, GL_RGBA, glTexPixelFormat, textureData);
			break;
			
		case DS_DISPLAY_TYPE_TOUCH:
			glBindTexture(GL_TEXTURE_2D, touchDisplayTexIndex);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)textureSize.width, (GLsizei)textureSize.height, GL_RGBA, glTexPixelFormat, textureData);
			break;
			
		case DS_DISPLAY_TYPE_COMBO:
		{
			textureSize.height /= 2;
			size_t offset = (size_t)textureSize.width * (size_t)textureSize.height;
			
			glBindTexture(GL_TEXTURE_2D, mainDisplayTexIndex);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)textureSize.width, (GLsizei)textureSize.height, GL_RGBA, glTexPixelFormat, textureData);
			
			glBindTexture(GL_TEXTURE_2D, touchDisplayTexIndex);
			if (glTexPixelFormat == GL_UNSIGNED_SHORT_1_5_5_5_REV)
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)textureSize.width, (GLsizei)textureSize.height, GL_RGBA, glTexPixelFormat, (uint16_t *)textureData + offset);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)textureSize.width, (GLsizei)textureSize.height, GL_RGBA, glTexPixelFormat, (uint32_t *)textureData + offset);
			}
			
			break;
		}
			
		default:
			break;
	}
	
	glBindTexture(GL_TEXTURE_2D, 0);
}

- (void) renderDisplay
{
	NSInteger displayType = [dispViewDelegate displayType];
	GLfloat angleDegrees = (GLfloat)CLOCKWISE_DEGREES([dispViewDelegate rotation]);
	GLfloat s = (GLfloat)[dispViewDelegate scale];
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glBindBuffer(GL_ARRAY_BUFFER, vboTexCoordID);
	glTexCoordPointer(2, GL_FLOAT, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vboVertexID);
	glVertexPointer(2, GL_INT, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboElementID);
	
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	
	glRotatef(angleDegrees, 0.0f, 0.0f, 1.0f);
	glScalef(s, s, 1.0f);
	
	switch (displayType)
	{
		case DS_DISPLAY_TYPE_MAIN:
			glBindTexture(GL_TEXTURE_2D, mainDisplayTexIndex);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
			break;
			
		case DS_DISPLAY_TYPE_TOUCH:
			glBindTexture(GL_TEXTURE_2D, touchDisplayTexIndex);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
			break;
			
		case DS_DISPLAY_TYPE_COMBO:
			glBindTexture(GL_TEXTURE_2D, mainDisplayTexIndex);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
			glBindTexture(GL_TEXTURE_2D, touchDisplayTexIndex);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (GLubyte *)6);
			break;
			
		default:
			break;
	}
	
	glPopMatrix();
	
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_TEXTURE_COORD_ARRAY, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

- (void) updateDisplayVertices
{
	NSInteger displayType = [dispViewDelegate displayType];
	GLint w = (GLint)GPU_DISPLAY_WIDTH;
	GLint h = (GLint)GPU_DISPLAY_HEIGHT;
	
	GLint *vertices = new GLint[8*2];
	
	if (displayType == DS_DISPLAY_TYPE_COMBO)
	{
		NSInteger displayOrientation = [dispViewDelegate displayOrientation];
		
		if (displayOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			vertices[0]		= -w/2;		vertices[1]		= h;		// Top display, top left
			vertices[2]		= w/2;		vertices[3]		= h;		// Top display, top right
			vertices[4]		= w/2;		vertices[5]		= 0;		// Top display, bottom right
			vertices[6]		= -w/2;		vertices[7]		= 0;		// Top display, bottom left
			
			vertices[8]		= -w/2;		vertices[9]		= 0;		// Bottom display, top left
			vertices[10]	= w/2;		vertices[11]	= 0;		// Bottom display, top right
			vertices[12]	= w/2;		vertices[13]	= -h;		// Bottom display, bottom right
			vertices[14]	= -w/2;		vertices[15]	= -h;		// Bottom display, bottom left
		}
		else // displayOrientation == DS_DISPLAY_ORIENTATION_HORIZONTAL
		{
			vertices[0]		= -w;		vertices[1]		= h/2;		// Left display, top left
			vertices[2]		= 0;		vertices[3]		= h/2;		// Left display, top right
			vertices[4]		= 0;		vertices[5]		= -h/2;		// Left display, bottom right
			vertices[6]		= -w;		vertices[7]		= -h/2;		// Left display, bottom left
			
			vertices[8]		= 0;		vertices[9]		= h/2;		// Right display, top left
			vertices[10]	= w;		vertices[11]	= h/2;		// Right display, top right
			vertices[12]	= w;		vertices[13]	= -h/2;		// Right display, bottom right
			vertices[14]	= 0;		vertices[15]	= -h/2;		// Right display, bottom left
		}
	}
	else // displayType == DS_DISPLAY_TYPE_MAIN || displayType == DS_DISPLAY_TYPE_TOUCH
	{
		vertices[0]		= -w/2;		vertices[1]		= h/2;		// Left display, top left
		vertices[2]		= w/2;		vertices[3]		= h/2;		// Left display, top right
		vertices[4]		= w/2;		vertices[5]		= -h/2;		// Left display, bottom right
		vertices[6]		= -w/2;		vertices[7]		= -h/2;		// Left display, bottom left
		
		vertices[8]		= -w/2;		vertices[9]		= h/2;		// Right display, top left
		vertices[10]	= w/2;		vertices[11]	= h/2;		// Right display, top right
		vertices[12]	= w/2;		vertices[13]	= -h/2;		// Right display, bottom right
		vertices[14]	= -w/2;		vertices[15]	= -h/2;		// Right display, bottom left
	}
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glBindBuffer(GL_ARRAY_BUFFER, vboVertexID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * 8 * 2, vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	CGLUnlockContext(cglDisplayContext);
	
	delete [] vertices;
}

- (void)keyDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleKeyPress:theEvent keyPressed:YES];
	if (!isHandled)
	{
		[super keyDown:theEvent];
	}
}

- (void)keyUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleKeyPress:theEvent keyPressed:NO];
	if (!isHandled)
	{
		[super keyUp:theEvent];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super mouseDown:theEvent];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	[self mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super mouseUp:theEvent];
	}
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super rightMouseDown:theEvent];
	}
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
	[self rightMouseDown:theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super rightMouseUp:theEvent];
	}
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super otherMouseDown:theEvent];
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	[self otherMouseDown:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
	BOOL isHandled = [dispViewDelegate handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super otherMouseUp:theEvent];
	}
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)becomeFirstResponder
{
	return YES;
}

- (BOOL)resignFirstResponder
{
	return YES;
}

- (void)doInitVideoOutput:(NSDictionary *)properties
{
	// No init needed, so do nothing.
}

- (void)doProcessVideoFrame:(const void *)videoFrameData frameSize:(NSSize)frameSize
{
	CGLLockContext(cglDisplayContext);
	
	CGLSetCurrentContext(cglDisplayContext);
	[self uploadDisplayTextures:videoFrameData textureSize:frameSize];
	[self renderDisplay];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doResizeView:(NSRect)rect
{
	[self updateDisplayVertices];
}

- (void)doRedraw
{
	CGLLockContext(cglDisplayContext);
	
	CGLSetCurrentContext(cglDisplayContext);
	[self renderDisplay];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayTypeChanged:(NSInteger)displayTypeID
{
	[self updateDisplayVertices];
}

- (void)doBilinearOutputChanged:(BOOL)useBilinear
{
	glTexRenderStyle = GL_NEAREST;
	if (useBilinear)
	{
		glTexRenderStyle = GL_LINEAR;
	}
	
	CGLLockContext(cglDisplayContext);
	
	glBindTexture(GL_TEXTURE_2D, mainDisplayTexIndex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glTexRenderStyle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glTexRenderStyle);
	
	glBindTexture(GL_TEXTURE_2D, touchDisplayTexIndex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glTexRenderStyle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glTexRenderStyle);
	
	glBindTexture(GL_TEXTURE_2D, NULL);
	
	CGLUnlockContext(cglDisplayContext);
}

- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID
{
	[self updateDisplayVertices];
}

- (void) doDisplayOrderChanged:(NSInteger)displayOrderID
{
	// Set up vertex elements and store in VBO
	static const GLubyte mainFirstElements[12] =	{0, 1, 2,
													 2, 3, 0,
													 
													 4, 5, 6,
													 6, 7, 4};
	
	static const GLubyte touchFirstElements[12] =	{4, 5, 6,
													 6, 7, 4,
													 
													 0, 1, 2,
													 2, 3, 0};
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboElementID);
	
	if (displayOrderID == DS_DISPLAY_ORDER_MAIN_FIRST)
	{
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mainFirstElements), mainFirstElements, GL_STATIC_DRAW);
	}
	else // displayOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
	{
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(touchFirstElements), touchFirstElements, GL_STATIC_DRAW);
	}
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	if ([dispViewDelegate displayType] == DS_DISPLAY_TYPE_COMBO)
	{
		[self doRedraw];
	}
}

- (void)doVerticalSyncChanged:(BOOL)useVerticalSync
{
	GLint swapInt = 0;
	
	if (useVerticalSync)
	{
		swapInt = 1;
	}
	
	CGLSetParameter(cglDisplayContext, kCGLCPSwapInterval, &swapInt);
}

- (void)doVideoFilterChanged:(NSInteger)videoFilterTypeID frameSize:(NSSize)videoFilterDestSize
{
	size_t colorDepth = sizeof(uint32_t);
	glTexPixelFormat = GL_UNSIGNED_INT_8_8_8_8_REV;
	
	if (videoFilterTypeID == VideoFilterTypeID_None)
	{
		colorDepth = sizeof(uint16_t);
		glTexPixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	}
	
	if ([dispViewDelegate displayType] == DS_DISPLAY_TYPE_COMBO)
	{
		videoFilterDestSize.height /= 2.0;
	}
	
	// Convert textures to Power-of-Two to support older GPUs
	// Example: Radeon X1600M on the 2006 MacBook Pro
	uint32_t potW = GetNearestPositivePOT((uint32_t)videoFilterDestSize.width);
	uint32_t potH = GetNearestPositivePOT((uint32_t)videoFilterDestSize.height);
	
	if (glTexBackSize.width != potW || glTexBackSize.height != potH)
	{
		glTexBackSize.width = potW;
		glTexBackSize.height = potH;
		
		free(glTexBack);
		glTexBack = (GLvoid *)calloc((size_t)potW * (size_t)potH, colorDepth);
		if (glTexBack == NULL)
		{
			return;
		}
	}
	
	GLfloat w = (GLfloat)(videoFilterDestSize.width / potW);
	GLfloat h = (GLfloat)(videoFilterDestSize.height / potH);
	
	CGLLockContext(cglDisplayContext);
	
	glBindTexture(GL_TEXTURE_2D, mainDisplayTexIndex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)potW, (GLsizei)potH, 0, GL_RGBA, glTexPixelFormat, glTexBack);
	glBindTexture(GL_TEXTURE_2D, touchDisplayTexIndex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)potW, (GLsizei)potH, 0, GL_RGBA, glTexPixelFormat, glTexBack);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up texture coordinates and store in VBO
	GLfloat texCoords[8*2] =	{0.0f, 0.0f,
								 w, 0.0f,
								 w, h,
								 0.0f, h,
								 
								 0.0f, 0.0f,
								 w, 0.0f,
								 w, h,
								 0.0f, h};
	
	glBindBuffer(GL_ARRAY_BUFFER, vboTexCoordID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	CGLUnlockContext(cglDisplayContext);
}

@end

bool OSXOpenGLRendererInit()
{
	return true;
}

bool OSXOpenGLRendererBegin()
{
	CGLSetCurrentContext(OSXOpenGLRendererContext);
	
	return true;
}

void OSXOpenGLRendererEnd()
{
	
}
