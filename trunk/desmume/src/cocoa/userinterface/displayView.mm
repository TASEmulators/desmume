/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2013 DeSmuME team

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
#import "EmuControllerDelegate.h"

#import "cocoa_input.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#undef BOOL

// VERTEX SHADER FOR DISPLAY OUTPUT
const char *vertexProgram_100 = {"\
	attribute vec2 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	varying vec2 vtxTexCoord; \n\
	\n\
	void main() \n\
	{ \n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2(cos(angleRadians), -sin(angleRadians)), \n\
								vec2(sin(angleRadians),  cos(angleRadians))); \n\
		\n\
		mat2 scale		= mat2(	vec2(scalar,    0.0), \n\
								vec2(   0.0, scalar)); \n\
		\n\
		vtxTexCoord = inTexCoord0; \n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 1.0, 1.0); \n\
	} \n\
"};

// FRAGMENT SHADER FOR DISPLAY OUTPUT
const char *fragmentProgram_100 = {"\
	varying vec2 vtxTexCoord; \n\
	uniform sampler2D tex; \n\
	\n\
	void main() \n\
	{ \n\
		gl_FragColor = texture2D(tex, vtxTexCoord); \n\
	} \n\
"};


@implementation DisplayViewDelegate

@synthesize view;
@synthesize sendPortDisplay;
@synthesize sendPortInput;
@synthesize cdsController;
@synthesize isHudEnabled;
@synthesize isHudEditingModeEnabled;
@dynamic normalSize;
@dynamic scale;
@dynamic rotation;
@dynamic useBilinearOutput;
@dynamic useVerticalSync;
@dynamic displayMode;
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
	
	[bindings setValue:[NSNumber numberWithDouble:1.0] forKey:@"scale"];
	[bindings setValue:[NSNumber numberWithDouble:0.0] forKey:@"rotation"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"useBilinearOutput"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"useVerticalSync"];
	[bindings setValue:[NSNumber numberWithInteger:DS_DISPLAY_TYPE_COMBO] forKey:@"displayMode"];
	[bindings setValue:@"Combo" forKey:@"displayModeString"];
	[bindings setValue:[NSNumber numberWithInteger:VideoFilterTypeID_None] forKey:@"videoFilterType"];
	[bindings setValue:[CocoaVideoFilter typeStringByID:VideoFilterTypeID_None] forKey:@"videoFilterTypeString"];
	
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

- (void) setScale:(double)s
{
	OSSpinLockLock(&spinlockScale);
	[bindings setValue:[NSNumber numberWithDouble:s] forKey:@"scale"];
	OSSpinLockUnlock(&spinlockScale);
	
	DisplayOutputTransformData transformData	= { s,
												    [self rotation],
												    0.0,
												    0.0,
												    0.0 };
	
	[CocoaDSUtil messageSendOneWayWithData:[self sendPortDisplay]
									 msgID:MESSAGE_TRANSFORM_VIEW
									  data:[NSData dataWithBytes:&transformData length:sizeof(DisplayOutputTransformData)]];
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
	
	DisplayOutputTransformData transformData	= { [self scale],
												    angleDegrees,
												    0.0,
												    0.0,
												    0.0 };
	
	[CocoaDSUtil messageSendOneWayWithData:[self sendPortDisplay]
									 msgID:MESSAGE_TRANSFORM_VIEW
									  data:[NSData dataWithBytes:&transformData length:sizeof(DisplayOutputTransformData)]];
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

- (void) setDisplayMode:(NSInteger)displayModeID
{
	NSSize newDisplaySize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT);
	NSString *modeString = @"Unknown";
	
	switch (displayModeID)
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
	NSInteger oldMode = [[bindings valueForKey:@"displayMode"] integerValue];
	[bindings setValue:[NSNumber numberWithInteger:displayModeID] forKey:@"displayMode"];
	[bindings setValue:modeString forKey:@"displayModeString"];
	OSSpinLockUnlock(&spinlockDisplayType);
	
	OSSpinLockLock(&spinlockNormalSize);
	normalSize = newDisplaySize;
	OSSpinLockUnlock(&spinlockNormalSize);
	
	[CocoaDSUtil messageSendOneWayWithInteger:self.sendPortDisplay msgID:MESSAGE_CHANGE_DISPLAY_TYPE integerValue:displayModeID];
	
	// If the display mode swaps between Main Only and Touch Only, the view will not resize to implicitly
	// redraw the view. So when swapping between these two display modes, explicitly tell the view to redraw.
	if ( (oldMode == DS_DISPLAY_TYPE_MAIN && displayModeID == DS_DISPLAY_TYPE_TOUCH) ||
		 (oldMode == DS_DISPLAY_TYPE_TOUCH && displayModeID == DS_DISPLAY_TYPE_MAIN) )
	{
		[CocoaDSUtil messageSendOneWay:self.sendPortDisplay msgID:MESSAGE_REDRAW_VIEW];
	}
}

- (NSInteger) displayMode
{
	OSSpinLockLock(&spinlockDisplayType);
	NSInteger displayModeID = [[bindings valueForKey:@"displayMode"] integerValue];
	OSSpinLockUnlock(&spinlockDisplayType);
	
	return displayModeID;
}

- (void) setDisplayOrientation:(NSInteger)theOrientation
{
	OSSpinLockLock(&spinlockDisplayOrientation);
	[bindings setValue:[NSNumber numberWithInteger:theOrientation] forKey:@"displayOrientation"];
	OSSpinLockUnlock(&spinlockDisplayOrientation);
	
	if ([self displayMode] == DS_DISPLAY_TYPE_COMBO)
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
	
	NSPoint touchLoc = GetNormalPointFromTransformedPoint(clickLoc, [self normalSize], [[self view] bounds].size, [self scale], viewAngle);
	
	// Normalize the touch location to the DS.
	if ([self displayMode] == DS_DISPLAY_TYPE_COMBO)
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
	
	if (keyPressed && [theEvent window] != nil && [[[theEvent window] delegate] respondsToSelector:@selector(setStatusText:)])
	{
		[(EmuControllerDelegate *)[[theEvent window] delegate] setStatusText:[NSString stringWithFormat:@"Keyboard:%i", [theEvent keyCode]]];
	}
	
	isHandled = [self.cdsController setStateWithInput:inputAttributes];
	
	return isHandled;
}

- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed
{
	BOOL isHandled = NO;
	NSInteger displayModeID = [self displayMode];
	
	if (self.cdsController == nil || (displayModeID != DS_DISPLAY_TYPE_TOUCH && displayModeID != DS_DISPLAY_TYPE_COMBO))
	{
		return isHandled;
	}
	
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	NSPoint touchLoc = [self dsPointFromEvent:theEvent];
	
	NSString *elementCode = [NSString stringWithFormat:@"%li", (long)[theEvent buttonNumber]];
	NSString *elementName = [NSString stringWithFormat:@"Button %li", (long)[theEvent buttonNumber]];
	
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
	
	if (buttonPressed && [theEvent window] != nil && [[[theEvent window] delegate] respondsToSelector:@selector(setStatusText:)])
	{
		[(EmuControllerDelegate *)[[theEvent window] delegate] setStatusText:[NSString stringWithFormat:@"Mouse:%li X:%li Y:%li", (long)[theEvent buttonNumber], (long)(touchLoc.x), (long)(touchLoc.y)]];
	}
	
	isHandled = [self.cdsController setStateWithInput:inputAttributes];
	
	return isHandled;
}

- (void) doInitVideoOutput:(NSDictionary *)properties
{
	[view doInitVideoOutput:properties];
}

- (void) doProcessVideoFrame:(const void *)videoFrameData displayMode:(const NSInteger)displayModeID width:(const NSInteger)frameWidth height:(const NSInteger)frameHeight
{
	[view doProcessVideoFrame:videoFrameData displayMode:displayModeID width:frameWidth height:frameHeight];
}

- (void) doResizeView:(NSRect)rect
{
	if (view == nil || ![view respondsToSelector:@selector(doResizeView:)])
	{
		return;
	}
	
	[view doResizeView:rect];
}

- (void) doTransformView:(const DisplayOutputTransformData *)transformData
{
	if (view == nil || ![view respondsToSelector:@selector(doTransformView:)])
	{
		return;
	}
	
	[view doTransformView:transformData];
}

- (void) doRedraw
{
	if (view == nil || ![view respondsToSelector:@selector(doRedraw)])
	{
		return;
	}
	
	[view doRedraw];
}

- (void) doDisplayModeChanged:(NSInteger)displayModeID
{
	if (view == nil || ![view respondsToSelector:@selector(doDisplayModeChanged:)])
	{
		return;
	}
	
	[view doDisplayModeChanged:displayModeID];
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

- (NSBitmapImageRep *) bitmapImageRep:(const void *)videoFrameData displayMode:(const NSInteger)displayModeID width:(const NSInteger)imageWidth height:(const NSInteger)imageHeight
{
	if (videoFrameData == nil)
	{
		return nil;
	}
	
	NSBitmapImageRep *imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																	  pixelsWide:imageWidth
																	  pixelsHigh:imageHeight
																   bitsPerSample:8
																 samplesPerPixel:4
																		hasAlpha:YES
																		isPlanar:NO
																  colorSpaceName:NSCalibratedRGBColorSpace
																	 bytesPerRow:imageWidth * 4
																	bitsPerPixel:32];
	
	if(!imageRep)
	{
		return nil;
	}
	
	uint32_t *bitmapData = (uint32_t *)[imageRep bitmapData];
	RGB555ToRGBA8888Buffer((const uint16_t *)videoFrameData, bitmapData, (imageWidth * imageHeight));
	
#ifdef __BIG_ENDIAN__
	uint32_t *bitmapDataEnd = bitmapData + (imageWidth * imageHeight);
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

- (void)doProcessVideoFrame:(const void *)videoFrameData displayMode:(const NSInteger)displayModeID width:(const NSInteger)frameWidth height:(const NSInteger)frameHeight
{
	// Render the frame in an NSBitmapImageRep
	NSBitmapImageRep *newImageRep = [self bitmapImageRep:videoFrameData displayMode:displayModeID width:frameWidth height:frameHeight];
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
	lastDisplayMode = DS_DISPLAY_TYPE_COMBO;
	currentDisplayOrientation = DS_DISPLAY_ORIENTATION_VERTICAL;
	glTexPixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	
	UInt32 w = GetNearestPositivePOT((UInt32)GPU_DISPLAY_WIDTH);
	UInt32 h = GetNearestPositivePOT((UInt32)(GPU_DISPLAY_HEIGHT * 2.0));
	glTexBack = (GLvoid *)calloc(w * h, sizeof(UInt16));
	glTexBackSize = NSMakeSize(w, h);
	cglDisplayContext = (CGLContextObj)[[self openGLContext] CGLContextObj];
	
	vtxBufferOffset = 0;
	
    return self;
}

- (void)dealloc
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	
	[self shutdownOpenGL];
	
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
	
	if (rect.size.width != oldFrame.size.width || rect.size.height != oldFrame.size.height)
	{
		[CocoaDSUtil messageSendOneWayWithRect:dispViewDelegate.sendPortDisplay msgID:MESSAGE_RESIZE_VIEW rect:rect];
		[self setNeedsDisplay:YES];
	}
}

- (void)prepareOpenGL
{
	[self updateDisplayVerticesUsingDisplayMode:lastDisplayMode orientation:currentDisplayOrientation];
	[self updateTexCoordS:1.0f T:2.0f];
	
	// Set up initial vertex elements
	vtxIndexBuffer[0]	= 0;	vtxIndexBuffer[1]	= 1;	vtxIndexBuffer[2]	= 2;
	vtxIndexBuffer[3]	= 2;	vtxIndexBuffer[4]	= 3;	vtxIndexBuffer[5]	= 0;
	
	vtxIndexBuffer[6]	= 4;	vtxIndexBuffer[7]	= 5;	vtxIndexBuffer[8]	= 6;
	vtxIndexBuffer[9]	= 6;	vtxIndexBuffer[10]	= 7;	vtxIndexBuffer[11]	= 4;
	
	[self setupOpenGL];
}

- (void) setupOpenGL
{
	// Check the OpenGL capabilities for this renderer
	const GLubyte *glExtString = glGetString(GL_EXTENSIONS);
	
	// Set up textures
	glGenTextures(1, &displayTexID);
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up shaders (but disable on PowerPC, since it doesn't seem to work there)
#if defined(__i386__) || defined(__x86_64__)
	isShaderSupported	= (gluCheckExtension((const GLubyte *)"GL_ARB_shader_objects", glExtString) &&
						   gluCheckExtension((const GLubyte *)"GL_ARB_vertex_shader", glExtString) &&
						   gluCheckExtension((const GLubyte *)"GL_ARB_fragment_shader", glExtString) &&
						   gluCheckExtension((const GLubyte *)"GL_ARB_vertex_program", glExtString) );
#else
	isShaderSupported	= false;
#endif
	if (isShaderSupported)
	{
		BOOL isShaderSetUp = [self setupShadersWithVertexProgram:vertexProgram_100 fragmentProgram:fragmentProgram_100];
		if (isShaderSetUp)
		{
			glUseProgram(shaderProgram);
			
			uniformAngleDegrees = glGetUniformLocation(shaderProgram, "angleDegrees");
			uniformScalar = glGetUniformLocation(shaderProgram, "scalar");
			uniformViewSize = glGetUniformLocation(shaderProgram, "viewSize");
			
			glUniform1f(uniformAngleDegrees, 0.0f);
			glUniform1f(uniformScalar, 1.0f);
			glUniform2f(uniformViewSize, GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2);
		}
		else
		{
			isShaderSupported = false;
		}
	}
	
	// Set up VBOs
	glGenBuffersARB(1, &vboVertexID);
	glGenBuffersARB(1, &vboTexCoordID);
	glGenBuffersARB(1, &vboElementID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * (2 * 8), vtxBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (2 * 8), texCoordBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, vboElementID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLubyte) * 12, vtxIndexBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysAPPLE(1, &vaoMainStatesID);
	glBindVertexArrayAPPLE(vaoMainStatesID);
	
	if (isShaderSupported)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_INT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, vboElementID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
		glVertexPointer(2, GL_INT, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, vboElementID);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	glBindVertexArrayAPPLE(0);
	
	// Render State Setup (common to both shaders and fixed-function pipeline)
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_STENCIL_TEST);
	
	// Set up fixed-function pipeline render states.
	if (!isShaderSupported)
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glEnable(GL_TEXTURE_2D);
	}
	
	// Set up clear attributes
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

- (void) shutdownOpenGL
{
	glDeleteTextures(1, &displayTexID);
	
	glDeleteVertexArraysAPPLE(1, &vaoMainStatesID);
	glDeleteBuffersARB(1, &vboVertexID);
	glDeleteBuffersARB(1, &vboTexCoordID);
	glDeleteBuffersARB(1, &vboElementID);
	
	if (isShaderSupported)
	{
		glUseProgram(0);
		
		glDetachShader(shaderProgram, vertexShaderID);
		glDetachShader(shaderProgram, fragmentShaderID);
		
		glDeleteProgram(shaderProgram);
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
	}
}

- (void) setupShaderIO
{
	glBindAttribLocation(shaderProgram, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(shaderProgram, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
}

- (BOOL) setupShadersWithVertexProgram:(const char *)vertShaderProgram fragmentProgram:(const char *)fragShaderProgram
{
	BOOL result = NO;
	GLint shaderStatus = GL_TRUE;
	
	vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if (vertexShaderID == 0)
	{
		NSLog(@"OpenGL Error - Failed to create vertex shader.");
		return result;
	}
	
	glShaderSource(vertexShaderID, 1, (const GLchar **)&vertShaderProgram, NULL);
	glCompileShader(vertexShaderID);
	glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		glDeleteShader(vertexShaderID);
		NSLog(@"OpenGL Error - Failed to compile vertex shader.");
		return result;
	}
	
	fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if (fragmentShaderID == 0)
	{
		glDeleteShader(vertexShaderID);
		NSLog(@"OpenGL Error - Failed to create fragment shader.");
		return result;
	}
	
	glShaderSource(fragmentShaderID, 1, (const GLchar **)&fragShaderProgram, NULL);
	glCompileShader(fragmentShaderID);
	glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		NSLog(@"OpenGL Error - Failed to compile fragment shader.");
		return result;
	}
	
	shaderProgram = glCreateProgram();
	if (shaderProgram == 0)
	{
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		NSLog(@"OpenGL Error - Failed to create shader program.");
		return result;
	}
	
	glAttachShader(shaderProgram, vertexShaderID);
	glAttachShader(shaderProgram, fragmentShaderID);
	
	[self setupShaderIO];
	
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		glDeleteProgram(shaderProgram);
		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);
		NSLog(@"OpenGL Error - Failed to link shader program.");
		return result;
	}
	
	glValidateProgram(shaderProgram);
	
	result = YES;
	return result;
}

- (void) drawVideoFrame
{
	CGLFlushDrawable(cglDisplayContext);
}

- (void) uploadVertices
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboVertexID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(GLint) * (2 * 8), vtxBuffer + vtxBufferOffset);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

- (void) uploadTexCoords
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vboTexCoordID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(GLfloat) * (2 * 8), texCoordBuffer);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

- (void) uploadDisplayTextures:(const GLvoid *)textureData displayMode:(const NSInteger)displayModeID width:(const GLsizei)texWidth height:(const GLsizei)texHeight
{
	if (textureData == NULL)
	{
		return;
	}
	
	const GLint lineOffset = (displayModeID == DS_DISPLAY_TYPE_TOUCH) ? texHeight : 0;
	
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lineOffset, texWidth, texHeight, GL_RGBA, glTexPixelFormat, textureData);
	glBindTexture(GL_TEXTURE_2D, 0);
}

- (void) renderDisplayUsingDisplayMode:(const NSInteger)displayModeID
{
	// Enable vertex attributes
	glBindVertexArrayAPPLE(vaoMainStatesID);
	
	// Perform the render
	if (lastDisplayMode != displayModeID)
	{
		lastDisplayMode = displayModeID;
		[self updateDisplayVerticesUsingDisplayMode:displayModeID orientation:currentDisplayOrientation];
		[self uploadVertices];
	}
	
	const GLsizei vtxElementCount = (displayModeID == DS_DISPLAY_TYPE_COMBO) ? 12 : 6;
	const GLubyte *elementPointer = !(displayModeID == DS_DISPLAY_TYPE_TOUCH) ? 0 : (GLubyte *)(vtxElementCount * sizeof(GLubyte));
	
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glDrawElements(GL_TRIANGLES, vtxElementCount, GL_UNSIGNED_BYTE, elementPointer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Disable vertex attributes
	glBindVertexArrayAPPLE(0);
}

- (void) updateDisplayVerticesUsingDisplayMode:(const NSInteger)displayModeID orientation:(const NSInteger)displayOrientationID
{
	const GLint w = (GLint)GPU_DISPLAY_WIDTH;
	const GLint h = (GLint)GPU_DISPLAY_HEIGHT;
	
	if (displayModeID == DS_DISPLAY_TYPE_COMBO)
	{
		// displayOrder == DS_DISPLAY_ORDER_MAIN_FIRST
		if (displayOrientationID == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			vtxBuffer[0]	= -w/2;		vtxBuffer[1]		= h;		// Top display, top left
			vtxBuffer[2]	= w/2;		vtxBuffer[3]		= h;		// Top display, top right
			vtxBuffer[4]	= w/2;		vtxBuffer[5]		= 0;		// Top display, bottom right
			vtxBuffer[6]	= -w/2;		vtxBuffer[7]		= 0;		// Top display, bottom left
			
			vtxBuffer[8]	= -w/2;		vtxBuffer[9]		= 0;		// Bottom display, top left
			vtxBuffer[10]	= w/2;		vtxBuffer[11]		= 0;		// Bottom display, top right
			vtxBuffer[12]	= w/2;		vtxBuffer[13]		= -h;		// Bottom display, bottom right
			vtxBuffer[14]	= -w/2;		vtxBuffer[15]		= -h;		// Bottom display, bottom left
		}
		else // displayOrientationID == DS_DISPLAY_ORIENTATION_HORIZONTAL
		{
			vtxBuffer[0]	= -w;		vtxBuffer[1]		= h/2;		// Left display, top left
			vtxBuffer[2]	= 0;		vtxBuffer[3]		= h/2;		// Left display, top right
			vtxBuffer[4]	= 0;		vtxBuffer[5]		= -h/2;		// Left display, bottom right
			vtxBuffer[6]	= -w;		vtxBuffer[7]		= -h/2;		// Left display, bottom left
			
			vtxBuffer[8]	= 0;		vtxBuffer[9]		= h/2;		// Right display, top left
			vtxBuffer[10]	= w;		vtxBuffer[11]		= h/2;		// Right display, top right
			vtxBuffer[12]	= w;		vtxBuffer[13]		= -h/2;		// Right display, bottom right
			vtxBuffer[14]	= 0;		vtxBuffer[15]		= -h/2;		// Right display, bottom left
		}
		
		// displayOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
		memcpy(vtxBuffer + (2 * 8), vtxBuffer + (1 * 8), sizeof(GLint) * (1 * 8));
		memcpy(vtxBuffer + (3 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (1 * 8));
	}
	else // displayModeID == DS_DISPLAY_TYPE_MAIN || displayModeID == DS_DISPLAY_TYPE_TOUCH
	{
		vtxBuffer[0]	= -w/2;		vtxBuffer[1]		= h/2;		// First display, top left
		vtxBuffer[2]	= w/2;		vtxBuffer[3]		= h/2;		// First display, top right
		vtxBuffer[4]	= w/2;		vtxBuffer[5]		= -h/2;		// First display, bottom right
		vtxBuffer[6]	= -w/2;		vtxBuffer[7]		= -h/2;		// First display, bottom left
		
		memcpy(vtxBuffer + (1 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (1 * 8));	// Second display
		memcpy(vtxBuffer + (2 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (2 * 8));	// Second display
	}
}

- (void) updateTexCoordS:(GLfloat)s T:(GLfloat)t
{
	texCoordBuffer[0]	= 0.0f;		texCoordBuffer[1]	=   0.0f;
	texCoordBuffer[2]	=    s;		texCoordBuffer[3]	=   0.0f;
	texCoordBuffer[4]	=    s;		texCoordBuffer[5]	= t/2.0f;
	texCoordBuffer[6]	= 0.0f;		texCoordBuffer[7]	= t/2.0f;
	
	texCoordBuffer[8]	= 0.0f;		texCoordBuffer[9]	= t/2.0f;
	texCoordBuffer[10]	=    s;		texCoordBuffer[11]	= t/2.0f;
	texCoordBuffer[12]	=    s;		texCoordBuffer[13]	=      t;
	texCoordBuffer[14]	= 0.0f;		texCoordBuffer[15]	=      t;
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

- (void)doProcessVideoFrame:(const void *)videoFrameData displayMode:(const NSInteger)displayModeID width:(const NSInteger)frameWidth height:(const NSInteger)frameHeight
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self uploadDisplayTextures:videoFrameData displayMode:displayModeID width:frameWidth height:frameHeight];
	[self renderDisplayUsingDisplayMode:displayModeID];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doResizeView:(NSRect)rect
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glViewport(0, 0, rect.size.width, rect.size.height);
	
	if (isShaderSupported)
	{
		glUniform2f(uniformViewSize, rect.size.width, rect.size.height);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-rect.size.width/2, -rect.size.width/2 + rect.size.width, -rect.size.height/2, -rect.size.height/2 + rect.size.height, -1.0, 1.0);
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doTransformView:(const DisplayOutputTransformData *)transformData
{
	const GLfloat angleDegrees = (GLfloat)transformData->rotation;
	const GLfloat s = (GLfloat)transformData->scale;
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	if (isShaderSupported)
	{
		glUniform1f(uniformAngleDegrees, angleDegrees);
		glUniform1f(uniformScalar, s);
	}
	else
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(CLOCKWISE_DEGREES(angleDegrees), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doRedraw
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self renderDisplayUsingDisplayMode:lastDisplayMode];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayModeChanged:(NSInteger)displayModeID
{
	lastDisplayMode = displayModeID;
	[self updateDisplayVerticesUsingDisplayMode:displayModeID orientation:currentDisplayOrientation];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self uploadVertices];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doBilinearOutputChanged:(BOOL)useBilinear
{
	const GLint textureFilter = useBilinear ? GL_LINEAR : GL_NEAREST;
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureFilter);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	CGLUnlockContext(cglDisplayContext);
}

- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID
{
	currentDisplayOrientation = displayOrientationID;
	[self updateDisplayVerticesUsingDisplayMode:lastDisplayMode orientation:displayOrientationID];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self uploadVertices];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void) doDisplayOrderChanged:(NSInteger)displayOrderID
{
	if (displayOrderID == DS_DISPLAY_ORDER_MAIN_FIRST)
	{
		vtxBufferOffset = 0;
	}
	else // displayOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
	{
		vtxBufferOffset = (2 * 8);
	}
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self uploadVertices];
	
	if (lastDisplayMode == DS_DISPLAY_TYPE_COMBO)
	{
		[self renderDisplayUsingDisplayMode:lastDisplayMode];
		[self drawVideoFrame];
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doVerticalSyncChanged:(BOOL)useVerticalSync
{
	const GLint swapInt = useVerticalSync ? 1 : 0;
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
	
	if ([dispViewDelegate displayMode] != DS_DISPLAY_TYPE_COMBO)
	{
		videoFilterDestSize.height = (uint32_t)videoFilterDestSize.height * 2;
	}
	
	// Convert textures to Power-of-Two to support older GPUs
	// Example: Radeon X1600M on the 2006 MacBook Pro
	const uint32_t potW = GetNearestPositivePOT((uint32_t)videoFilterDestSize.width);
	const uint32_t potH = GetNearestPositivePOT((uint32_t)videoFilterDestSize.height);
	
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
	
	const GLfloat s = (GLfloat)videoFilterDestSize.width / (GLfloat)potW;
	const GLfloat t = (GLfloat)videoFilterDestSize.height / (GLfloat)potH;
	[self updateTexCoordS:s T:t];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)potW, (GLsizei)potH, 0, GL_BGRA, glTexPixelFormat, glTexBack);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	[self uploadTexCoords];
	
	CGLUnlockContext(cglDisplayContext);
}

@end
