/*
	Copyright (C) 2025 DeSmuME team

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

#import "CocoaDisplayView.h"

#include "MacOGLDisplayView.h"

#ifdef ENABLE_APPLE_METAL
#include "MacMetalDisplayView.h"
#endif

#import "DisplayWindowController.h"
#import "EmuControllerDelegate.h"
#import "InputManager.h"

#import "cocoa_core.h"
#import "cocoa_GPU.h"
#import "cocoa_util.h"
#import "cocoa_globals.h"

@implementation CocoaDisplayView

@synthesize inputManager;
@dynamic displayViewOutput;
@dynamic canUseShaderBasedFilters;
@dynamic allowViewUpdates;
@dynamic isHUDVisible;
@dynamic isHUDExecutionSpeedVisible;
@dynamic isHUDVideoFPSVisible;
@dynamic isHUDRender3DFPSVisible;
@dynamic isHUDFrameIndexVisible;
@dynamic isHUDLagFrameCountVisible;
@dynamic isHUDCPULoadAverageVisible;
@dynamic isHUDRealTimeClockVisible;
@dynamic isHUDInputVisible;
@dynamic hudColorExecutionSpeed;
@dynamic hudColorVideoFPS;
@dynamic hudColorRender3DFPS;
@dynamic hudColorFrameIndex;
@dynamic hudColorLagFrameCount;
@dynamic hudColorCPULoadAverage;
@dynamic hudColorRTC;
@dynamic hudColorInputPendingAndApplied;
@dynamic hudColorInputAppliedOnly;
@dynamic hudColorInputPendingOnly;
@dynamic displayMainVideoSource;
@dynamic displayTouchVideoSource;
@dynamic displayViewID;
@dynamic useVerticalSync;
@dynamic videoFiltersPreferGPU;
@dynamic sourceDeposterize;
@dynamic outputFilter;
@dynamic pixelScaler;
@dynamic scaleFactor;

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self == nil)
	{
		return self;
	}
	
	_dvo = new MacDisplayViewOutput;
	_dvo->SetClientObject(self);
	
	inputManager = nil;
	localLayer = nil;
	localOGLContext = nil;
	
	return self;
}

- (void)dealloc
{
	delete _dvo;
	_dvo = NULL;
	
	[self setInputManager:nil];
	[self setLayer:nil];
	
	if (localOGLContext != nil)
	{
		[localOGLContext clearDrawable];
		[localOGLContext release];
	}
	
	[localLayer release];
	
	[super dealloc];
}

#pragma mark Class Methods

- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	
	MacInputDevicePropertiesEncoder *inputEncoder = [inputManager inputEncoder];
	const ClientInputDeviceProperties inputProperty = inputEncoder->EncodeKeyboardInput((int32_t)[theEvent keyCode], (keyPressed) ? true : false);
	
	if (keyPressed && [theEvent window] != nil)
	{
		NSString *newStatusText = [NSString stringWithFormat:@"%s:%s", inputProperty.deviceName, inputProperty.elementName];
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	isHandled = [inputManager dispatchCommandUsingInputProperties:&inputProperty];
	return isHandled;
}

- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	MacDisplayLayeredView *cdv = [localLayer clientDisplayView];
	const ClientDisplayMode displayMode = cdv->Get3DPresenter()->GetMode();
	
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	const int32_t buttonNumber = (int32_t)[theEvent buttonNumber];
	uint8_t x = 0;
	uint8_t y = 0;
	
	if (displayMode != ClientDisplayMode_Main)
	{
		const ClientDisplayPresenterProperties &props = cdv->Get3DPresenter()->GetPresenterProperties();
		const double scaleFactor = cdv->Get3DPresenter()->GetScaleFactor();
		const NSEventType eventType = [theEvent type];
		const bool isInitialMouseDown = (eventType == EVENT_MOUSEDOWN_LEFT) || (eventType == EVENT_MOUSEDOWN_RIGHT) || (eventType == EVENT_MOUSEDOWN_OTHER);
		
		// Convert the clicked location from window coordinates, to view coordinates, and finally to NDS touchscreen coordinates.
		const NSPoint clientLoc = [self convertPoint:[theEvent locationInWindow] fromView:nil];
		
		cdv->GetNDSPoint(props,
						 props.clientWidth / scaleFactor, props.clientHeight / scaleFactor,
						 clientLoc.x, clientLoc.y, (int)buttonNumber, isInitialMouseDown, x, y);
	}
	
	MacInputDevicePropertiesEncoder *inputEncoder = [inputManager inputEncoder];
	const ClientInputDeviceProperties inputProperty = inputEncoder->EncodeMouseInput(buttonNumber, (float)x, (float)y, (buttonPressed) ? true : false);
	
	if (buttonPressed && [theEvent window] != nil)
	{
		NSString *newStatusText = (displayMode == ClientDisplayMode_Main) ? [NSString stringWithFormat:@"%s:%s", inputProperty.deviceName, inputProperty.elementName] : [NSString stringWithFormat:@"%s:%s X:%i Y:%i", inputProperty.deviceName, inputProperty.elementName, (int)inputProperty.intCoordX, (int)inputProperty.intCoordY];
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	isHandled = [inputManager dispatchCommandUsingInputProperties:&inputProperty];
	return isHandled;
}

#pragma mark CocoaDisplayView Protocol

- (MacDisplayViewOutput *) displayViewOutput
{
	return _dvo;
}

- (BOOL) canUseShaderBasedFilters
{
	return _dvo->CanProcessVideoOnGPU() ? YES : NO;
}

- (BOOL) allowViewUpdates
{
	return _dvo->AllowViewUpdates() ? YES : NO;
}

- (void) setAllowViewUpdates:(BOOL)allowUpdates
{
	_dvo->SetAllowViewUpdates((allowUpdates) ? true : false);
}

- (void) setIsHUDVisible:(BOOL)theState
{
	_dvo->SetHUDVisible((theState) ? true : false);
}

- (BOOL) isHUDVisible
{
	return _dvo->IsHUDVisible() ? true : false;
}

- (void) setIsHUDExecutionSpeedVisible:(BOOL)theState
{
	_dvo->SetHUDExecutionSpeedVisible((theState) ? true : false);
}

- (BOOL) isHUDExecutionSpeedVisible
{
	return _dvo->IsHUDExecutionSpeedVisible() ? YES : NO;
}

- (void) setIsHUDVideoFPSVisible:(BOOL)theState
{
	_dvo->SetHUDVideoFPSVisible((theState) ? true : false);
}

- (BOOL) isHUDVideoFPSVisible
{
	return _dvo->IsHUDVideoFPSVisible() ? YES : NO;
}

- (void) setIsHUDRender3DFPSVisible:(BOOL)theState
{
	_dvo->SetHUDRender3DFPSVisible((theState) ? true : false);
}

- (BOOL) isHUDRender3DFPSVisible
{
	return _dvo->IsHUDRender3DFPSVisible() ? YES : NO;
}

- (void) setIsHUDFrameIndexVisible:(BOOL)theState
{
	_dvo->SetHUDFrameIndexVisible((theState) ? true : false);
}

- (BOOL) isHUDFrameIndexVisible
{
	return _dvo->IsHUDFrameIndexVisible() ? YES : NO;
}

- (void) setIsHUDLagFrameCountVisible:(BOOL)theState
{
	_dvo->SetHUDLagFrameCounterVisible((theState) ? true : false);
}

- (BOOL) isHUDLagFrameCountVisible
{
	return _dvo->IsHUDLagFrameCounterVisible() ? YES : NO;
}

- (void) setIsHUDCPULoadAverageVisible:(BOOL)theState
{
	_dvo->SetHUDCPULoadAverageVisible((theState) ? true : false);
}

- (BOOL) isHUDCPULoadAverageVisible
{
	return _dvo->IsHUDCPULoadAverageVisible() ? YES : NO;
}

- (void) setIsHUDRealTimeClockVisible:(BOOL)theState
{
	_dvo->SetHUDRealTimeClockVisible((theState) ? true : false);
}

- (BOOL) isHUDRealTimeClockVisible
{
	return _dvo->IsHUDRealTimeClockVisible() ? YES : NO;
}

- (void) setIsHUDInputVisible:(BOOL)theState
{
	_dvo->SetHUDInputVisible((theState) ? true : false);
}

- (BOOL) isHUDInputVisible
{
	return _dvo->IsHUDInputVisible() ? YES : NO;
}

- (void) setHudColorExecutionSpeed:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorExecutionSpeed(color32);
}

- (NSColor *) hudColorExecutionSpeed
{
	const Color4u8 color32 = _dvo->GetHUDColorExecutionSpeed();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorVideoFPS:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorVideoFPS(color32);
}

- (NSColor *) hudColorVideoFPS
{
	const Color4u8 color32 = _dvo->GetHUDColorVideoFPS();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorRender3DFPS:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorRender3DFPS(color32);
}

- (NSColor *) hudColorRender3DFPS
{
	const Color4u8 color32 = _dvo->GetHUDColorRender3DFPS();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorFrameIndex:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorFrameIndex(color32);
}

- (NSColor *) hudColorFrameIndex
{
	const Color4u8 color32 = _dvo->GetHUDColorFrameIndex();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorLagFrameCount:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorLagFrameCount(color32);
}

- (NSColor *) hudColorLagFrameCount
{
	const Color4u8 color32 = _dvo->GetHUDColorLagFrameCount();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorCPULoadAverage:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorCPULoadAverage(color32);
}

- (NSColor *) hudColorCPULoadAverage
{
	const Color4u8 color32 = _dvo->GetHUDColorCPULoadAverage();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorRTC:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorRealTimeClock(color32);
}

- (NSColor *) hudColorRTC
{
	const Color4u8 color32 = _dvo->GetHUDColorRealTimeClock();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorInputPendingAndApplied:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorInputPendingAndApplied(color32);
}

- (NSColor *) hudColorInputPendingAndApplied
{
	const Color4u8 color32 = _dvo->GetHUDColorInputPendingAndApplied();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorInputAppliedOnly:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorInputAppliedOnly(color32);
}

- (NSColor *) hudColorInputAppliedOnly
{
	const Color4u8 color32 = _dvo->GetHUDColorInputAppliedOnly();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setHudColorInputPendingOnly:(NSColor *)theColor
{
	Color4u8 color32;
	color32.value = [CocoaDSUtil RGBA8888FromNSColor:theColor];
	_dvo->SetHUDColorInputPendingOnly(color32);
}

- (NSColor *) hudColorInputPendingOnly
{
	const Color4u8 color32 = _dvo->GetHUDColorInputPendingOnly();
	return [CocoaDSUtil NSColorFromRGBA8888:color32.value];
}

- (void) setDisplayMainVideoSource:(NSInteger)displaySourceID
{
	_dvo->SetDisplayMainVideoSource((ClientDisplaySource)displaySourceID);
}

- (NSInteger) displayMainVideoSource
{
	return (NSInteger)_dvo->GetDisplayMainVideoSource();
}

- (void) setDisplayTouchVideoSource:(NSInteger)displaySourceID
{
	_dvo->SetDisplayTouchVideoSource((ClientDisplaySource)displaySourceID);
}

- (NSInteger) displayTouchVideoSource
{
	return (NSInteger)_dvo->GetDisplayTouchVideoSource();
}

- (void) setDisplayViewID:(NSInteger)displayID
{
	_dvo->SetDisplayViewID((int32_t)displayID);
}

- (NSInteger) displayViewID
{
	return (NSInteger)_dvo->GetDisplayViewID();
}

- (void) setUseVerticalSync:(BOOL)theState
{
	_dvo->SetVerticalSync((theState) ? true : false);
}

- (BOOL) useVerticalSync
{
	return _dvo->UseVerticalSync() ? YES : NO;
}

- (void) setVideoFiltersPreferGPU:(BOOL)theState
{
	_dvo->SetVideoProcessingPrefersGPU((theState) ? true : false);
}

- (BOOL) videoFiltersPreferGPU
{
	return _dvo->VideoProcessingPrefersGPU() ? YES : NO;
}

- (void) setSourceDeposterize:(BOOL)theState
{
	_dvo->SetSourceDeposterize((theState) ? true : false);
}

- (BOOL) sourceDeposterize
{
	return _dvo->UseSourceDeposterize() ? YES : NO;
}

- (void) setOutputFilter:(NSInteger)filterID
{
	return _dvo->SetOutputFilterByID((OutputFilterTypeID)filterID);
}

- (NSInteger) outputFilter
{
	return (OutputFilterTypeID)_dvo->GetOutputFilterID();
}

- (void) setPixelScaler:(NSInteger)filterID
{
	_dvo->SetPixelScalerByID((VideoFilterTypeID)filterID);
}

- (NSInteger) pixelScaler
{
	return (VideoFilterTypeID)_dvo->GetPixelScalerID();
}

- (void) setScaleFactor:(double)theScale
{
	_dvo->SetScaleFactor(theScale);
}

- (double) scaleFactor
{
	return _dvo->GetScaleFactor();
}

- (void) setupLayer
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[[[windowController emuControl] cdsCoreController] content];
	MacGPUFetchObjectDisplayLink *dlFetchObj = (MacGPUFetchObjectDisplayLink *)[[cdsCore cdsGPU] fetchObject];
	_dvo->SetMacFetchObject(dlFetchObj);
	
#ifdef ENABLE_APPLE_METAL
	BOOL isMetalLayer = NO;
	
	if ( (dlFetchObj->GetClientData() != nil) && (dlFetchObj->GetID() == GPUClientFetchObjectID_MacMetal) )
	{
		MetalDisplayViewSharedData *metalSharedData = (MetalDisplayViewSharedData *)dlFetchObj->GetClientData();
		
		if ([metalSharedData device] != nil)
		{
			MacMetalDisplayView *macMTLCDV = new MacMetalDisplayView(metalSharedData);
			macMTLCDV->Init();
			
			localLayer = macMTLCDV->GetCALayer();
			isMetalLayer = YES;
		}
	}
	else
#endif
	{
		MacOGLDisplayView *macOGLCDV = new MacOGLDisplayView((MacOGLClientFetchObject *)dlFetchObj);
		macOGLCDV->Init();
		
		localLayer = macOGLCDV->GetCALayer();
		
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
		if ([[self window] respondsToSelector:@selector(backingScaleFactor)] && [localLayer respondsToSelector:@selector(setContentsScale:)])
		{
			[localLayer setContentsScale:[[self window] backingScaleFactor]];
		}
#endif
		
		// For macOS 10.8 Mountain Lion and later, we can use the CAOpenGLLayer directly. But for
		// earlier versions of macOS, using the CALayer directly will cause too many strange issues,
		// so we'll just keep using the old-school NSOpenGLContext for these older macOS versions.
		if (IsOSXVersionSupported(10, 8, 0))
		{
			macOGLCDV->SetRenderToCALayer(true);
		}
		else
		{
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
			if ([self respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)])
			{
				SILENCE_DEPRECATION_MACOS_10_14([self setWantsBestResolutionOpenGLSurface:YES])
			}
#endif
			localOGLContext = ((MacOGLDisplayPresenter *)macOGLCDV->Get3DPresenter())->GetNSContext();
			[localOGLContext retain];
		}
	}
	
	MacDisplayLayeredView *cdv = [localLayer clientDisplayView];
	_dvo->SetClientDisplayView(cdv);
	
	_dvo->SetIdle(false);
	_dvo->CreateThread();
	
	NSString *fontPath = [[NSBundle mainBundle] pathForResource:@"SourceSansPro-Bold" ofType:@"otf"];
	cdv->Get3DPresenter()->SetHUDFontPath([CocoaDSUtil cPathFromFilePath:fontPath]);
	cdv->Get3DPresenter()->SetHUDRenderMipmapped(true);
	
	// Set up the scaling factor if this is a Retina window
	float scaleFactor = 1.0f;
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
	if ([[windowController masterWindow] respondsToSelector:@selector(backingScaleFactor)])
	{
		scaleFactor = [[windowController masterWindow] backingScaleFactor];
	}
#endif
	
	if (scaleFactor != 1.0f)
	{
		_dvo->SetScaleFactor(scaleFactor);
	}
	else
	{
		cdv->Get3DPresenter()->LoadHUDFont();
	}
	
	cdv->Get3DPresenter()->UpdateLayout();
	
	if (localOGLContext != nil)
	{
		// If localOGLContext isn't nil, then we will not assign the local layer
		// directly to the view, since the OpenGL context will already be what
		// is assigned.
		cdv->FlushAndFinalizeImmediate();
		return;
	}
	
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if ([self respondsToSelector:@selector(setLayerContentsRedrawPolicy:)])
	{
		[self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
	}
#endif
	
	[self setLayer:localLayer];
	[self setWantsLayer:YES];
	
	if (cdv->GetRenderToCALayer())
	{
		[localLayer setNeedsDisplay];
	}
	else
	{
		cdv->FlushAndFinalizeImmediate();
	}
}

- (void) updateLayerPresenterProperties:(ClientDisplayPresenterProperties &)props scaleFactor:(const double)scaleFactor needFlush:(BOOL)needFlush
{
	double checkWidth = props.normalWidth;
	double checkHeight = props.normalHeight;
	ClientDisplayPresenter::ConvertNormalToTransformedBounds(1.0, props.rotation, checkWidth, checkHeight);
	props.viewScale = ClientDisplayPresenter::GetMaxScalarWithinBounds(checkWidth, checkHeight, props.clientWidth, props.clientHeight);
	
	if (localOGLContext != nil)
	{
		[localOGLContext update];
	}
	else if ([localLayer isKindOfClass:[CAOpenGLLayer class]])
	{
		[localLayer setBounds:CGRectMake(0.0f, 0.0f, props.clientWidth / scaleFactor, props.clientHeight / scaleFactor)];
	}
#ifdef ENABLE_APPLE_METAL
	else if ([localLayer isKindOfClass:[CAMetalLayer class]])
	{
		[(CAMetalLayer *)localLayer setDrawableSize:CGSizeMake(props.clientWidth, props.clientHeight)];
	}
#endif
	
	_dvo->CommitPresenterProperties(props, (needFlush) ? true : false);
}

- (void) isPixelScalerSupportedByID:(NSInteger)filterID cpu:(BOOL &)outSupportedOnCPU shader:(BOOL &)outSupportedOnShader
{
	bool isSupportedOnCPU = false;
	bool isSupportedOnShader = false;
	OGLFilter::GetSupport((int)filterID, &isSupportedOnCPU, &isSupportedOnShader);
	
	outSupportedOnCPU = (isSupportedOnCPU) ? YES : NO;
	outSupportedOnShader = ([self canUseShaderBasedFilters] && isSupportedOnShader) ? YES : NO;
}

#pragma mark ClientDisplayViewOutput
- (NSImage *) image
{
	MacDisplayLayeredView *cdv = (MacDisplayLayeredView *)_dvo->GetClientDisplayView();
	
	NSSize viewSize = NSMakeSize(cdv->Get3DPresenter()->GetPresenterProperties().clientWidth, cdv->Get3DPresenter()->GetPresenterProperties().clientHeight);
	NSUInteger w = viewSize.width;
	NSUInteger h = viewSize.height;
	
	NSImage *newImage = [[NSImage alloc] initWithSize:viewSize];
	if (newImage == nil)
	{
		return newImage;
	}
	
	// Render the frame in an NSBitmapImageRep
	NSBitmapImageRep *newImageRep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																			 pixelsWide:w
																			 pixelsHigh:h
																		  bitsPerSample:8
																		samplesPerPixel:4
																			   hasAlpha:YES
																			   isPlanar:NO
																		 colorSpaceName:NSCalibratedRGBColorSpace
																			bytesPerRow:w * 4
																		   bitsPerPixel:32] autorelease];
	if (newImageRep == nil)
	{
		[newImage release];
		newImage = nil;
		return newImage;
	}
	
	cdv->Get3DPresenter()->CopyFrameToBuffer((Color4u8 *)[newImageRep bitmapData]);
	
	// Attach the rendered frame to the NSImageRep
	[newImage addRepresentation:newImageRep];
	
	return [newImage autorelease];
}

- (void) handleCopyToPasteboard
{
	NSImage *screenshot = [self image];
	if (screenshot == nil)
	{
		return;
	}
	
	NSPasteboard *pboard = [NSPasteboard generalPasteboard];
	[pboard declareTypes:[NSArray arrayWithObjects:PASTEBOARDTYPE_TIFF, nil] owner:self];
	[pboard setData:[screenshot TIFFRepresentationUsingCompression:NSTIFFCompressionLZW factor:1.0f] forType:PASTEBOARDTYPE_TIFF];
}

#pragma mark InputHIDManagerTarget Protocol
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue hidManager:(InputHIDManager *)hidManager
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	
	MacInputDevicePropertiesEncoder *inputEncoder = [[hidManager inputManager] inputEncoder];
	ClientInputDevicePropertiesList inputPropertyList = inputEncoder->EncodeHIDQueue(hidQueue, [hidManager inputManager], false);
	NSString *newStatusText = nil;
	
	const size_t inputCount = inputPropertyList.size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const ClientInputDeviceProperties &inputProperty = inputPropertyList[i];
		
		if (inputProperty.isAnalog)
		{
			newStatusText = [NSString stringWithFormat:@"%s:%s (%1.2f)", inputProperty.deviceName, inputProperty.elementName, inputProperty.scalar];
			break;
		}
		else if (inputProperty.state == ClientInputDeviceState_On)
		{
			newStatusText = [NSString stringWithFormat:@"%s:%s", inputProperty.deviceName, inputProperty.elementName];
			break;
		}
	}
	
	if (newStatusText != nil)
	{
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	CommandAttributesList cmdList = [inputManager generateCommandListUsingInputList:&inputPropertyList];
	if (cmdList.empty())
	{
		return isHandled;
	}
	
	[inputManager dispatchCommandList:&cmdList];
	
	isHandled = YES;
	return isHandled;
}

#pragma mark NSView Methods

- (void)lockFocus
{
	[super lockFocus];
	
	if ( (localOGLContext != nil) && ([localOGLContext view] != self) )
	{
		[localOGLContext setView:self];
	}
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)wantsDefaultClipping
{
	return NO;
}

- (BOOL)wantsUpdateLayer
{
	return YES;
}

- (void)updateLayer
{
	_dvo->FlushAndFinalizeImmediate();
}

- (void)drawRect:(NSRect)dirtyRect
{
	_dvo->FlushAndFinalizeImmediate();
}

- (void)setFrame:(NSRect)rect
{
	NSRect oldFrame = [self frame];
	[super setFrame:rect];
	
	if (rect.size.width != oldFrame.size.width || rect.size.height != oldFrame.size.height)
	{
		DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
		NSRect newViewportRect = rect;
		
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
		if ([self respondsToSelector:@selector(convertRectToBacking:)])
		{
			newViewportRect = [self convertRectToBacking:rect];
		}
#endif
		
		// Calculate the view scale for the given client size.
		ClientDisplayPresenterProperties &props = [windowController localViewProperties];
		props.clientWidth = newViewportRect.size.width;
		props.clientHeight = newViewportRect.size.height;
		
		const double scaleFactor = _dvo->GetScaleFactor();
		[self updateLayerPresenterProperties:props scaleFactor:scaleFactor needFlush:YES];
	}
}

#pragma mark NSResponder Methods

- (void)keyDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleKeyPress:theEvent keyPressed:YES];
	if (!isHandled)
	{
		[super keyDown:theEvent];
	}
}

- (void)keyUp:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleKeyPress:theEvent keyPressed:NO];
	if (!isHandled)
	{
		[super keyUp:theEvent];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButton:theEvent buttonPressed:YES];
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
	BOOL isHandled = [self handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super mouseUp:theEvent];
	}
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButton:theEvent buttonPressed:YES];
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
	BOOL isHandled = [self handleMouseButton:theEvent buttonPressed:NO];
	if (!isHandled)
	{
		[super rightMouseUp:theEvent];
	}
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButton:theEvent buttonPressed:YES];
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
	BOOL isHandled = [self handleMouseButton:theEvent buttonPressed:NO];
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

@end

#pragma mark -

MacDisplayViewOutput::MacDisplayViewOutput()
{
	__InstanceInit(NULL);
}

MacDisplayViewOutput::MacDisplayViewOutput(MacGPUFetchObjectDisplayLink *fetchObj)
{
	__InstanceInit(fetchObj);
}

void MacDisplayViewOutput::__InstanceInit(MacGPUFetchObjectDisplayLink *fetchObj)
{
	_threadName = "macOS Display View Output";
	_platformId = PlatformTypeID_Mac;
	_typeId = OutputTypeID_Video | OutputTypeID_HostDisplay;
	
	_macFetchObj = fetchObj;
	
	if (fetchObj != NULL)
	{
		fetchObj->RegisterOutputDisplay(this);
	}
}

MacDisplayViewOutput::~MacDisplayViewOutput()
{
	if (this->_macFetchObj != NULL)
	{
		this->_macFetchObj->UnregisterOutputDisplay(this);
	}
}

MacGPUFetchObjectDisplayLink* MacDisplayViewOutput::GetMacFetchObject()
{
	return this->_macFetchObj;
}

void MacDisplayViewOutput::SetMacFetchObject(MacGPUFetchObjectDisplayLink *fetchObj)
{
	this->_macFetchObj = fetchObj;
	
	if (fetchObj != NULL)
	{
		fetchObj->RegisterOutputDisplay(this);
	}
}

void MacDisplayViewOutput::SetVideoProcessingPrefersGPU(bool theState)
{
	pthread_mutex_lock(&this->_mutexVideoFiltersPreferGPU);
	pthread_mutex_lock(&this->_mutexSourceDeposterize);
	pthread_mutex_lock(&this->_mutexPixelScaler);
	
	const bool oldStateWillFilterDirectToCPU = ( !this->_cdv->Get3DPresenter()->WillFilterOnGPU() && !this->_cdv->Get3DPresenter()->GetSourceDeposterize() && (this->_cdv->Get3DPresenter()->GetPixelScaler() != VideoFilterTypeID_None) );
	const bool oldStateRequestFilterOnCPU = !this->_cdv->Get3DPresenter()->GetFiltersPreferGPU();
	this->_cdv->Get3DPresenter()->SetFiltersPreferGPU(theState);
	const bool newStateRequestFilterOnCPU = !this->_cdv->Get3DPresenter()->GetFiltersPreferGPU();
	const bool newStateWillFilterDirectToCPU = ( !this->_cdv->Get3DPresenter()->WillFilterOnGPU() && !this->_cdv->Get3DPresenter()->GetSourceDeposterize() && (this->_cdv->Get3DPresenter()->GetPixelScaler() != VideoFilterTypeID_None) );
	
	pthread_mutex_unlock(&this->_mutexPixelScaler);
	pthread_mutex_unlock(&this->_mutexSourceDeposterize);
	pthread_mutex_unlock(&this->_mutexVideoFiltersPreferGPU);
	
	if ( (oldStateRequestFilterOnCPU != newStateRequestFilterOnCPU) || (oldStateWillFilterDirectToCPU != newStateWillFilterDirectToCPU) )
	{
		if (oldStateRequestFilterOnCPU != newStateRequestFilterOnCPU)
		{
			if (newStateRequestFilterOnCPU)
			{
				this->_macFetchObj->IncrementViewsPreferringCPUVideoProcessing();
			}
			else
			{
				this->_macFetchObj->DecrementViewsPreferringCPUVideoProcessing();
			}
		}
		
		if (oldStateWillFilterDirectToCPU != newStateWillFilterDirectToCPU)
		{
			if (newStateWillFilterDirectToCPU)
			{
				this->_macFetchObj->IncrementViewsUsingDirectToCPUFiltering();
			}
			else
			{
				this->_macFetchObj->DecrementViewsUsingDirectToCPUFiltering();
			}
		}
		
		this->Dispatch(MESSAGE_RELOAD_REPROCESS_REDRAW);
	}
}

void MacDisplayViewOutput::SetSourceDeposterize(bool theState)
{
	pthread_mutex_lock(&this->_mutexSourceDeposterize);
	pthread_mutex_lock(&this->_mutexPixelScaler);
	
	const bool oldStateWillFilterDirectToCPU = ( !this->_cdv->Get3DPresenter()->WillFilterOnGPU() && !this->_cdv->Get3DPresenter()->GetSourceDeposterize() && (this->_cdv->Get3DPresenter()->GetPixelScaler() != VideoFilterTypeID_None) );
	this->_cdv->Get3DPresenter()->SetSourceDeposterize(theState);
	const bool newStateWillFilterDirectToCPU = ( !this->_cdv->Get3DPresenter()->WillFilterOnGPU() && !this->_cdv->Get3DPresenter()->GetSourceDeposterize() && (this->_cdv->Get3DPresenter()->GetPixelScaler() != VideoFilterTypeID_None) );
	
	pthread_mutex_unlock(&this->_mutexPixelScaler);
	pthread_mutex_unlock(&this->_mutexSourceDeposterize);
	
	if (oldStateWillFilterDirectToCPU != newStateWillFilterDirectToCPU)
	{
		if (newStateWillFilterDirectToCPU)
		{
			this->_macFetchObj->IncrementViewsUsingDirectToCPUFiltering();
		}
		else
		{
			this->_macFetchObj->DecrementViewsUsingDirectToCPUFiltering();
		}
	}
	
	this->Dispatch(MESSAGE_REPROCESS_AND_REDRAW);
}

void MacDisplayViewOutput::SetPixelScalerByID(VideoFilterTypeID filterID)
{
	pthread_mutex_lock(&this->_mutexSourceDeposterize);
	pthread_mutex_lock(&this->_mutexPixelScaler);
	
	const bool oldStateWillFilterDirectToCPU = ( !this->_cdv->Get3DPresenter()->WillFilterOnGPU() && !this->_cdv->Get3DPresenter()->GetSourceDeposterize() && (this->_cdv->Get3DPresenter()->GetPixelScaler() != VideoFilterTypeID_None) );
	this->_cdv->Get3DPresenter()->SetPixelScaler((VideoFilterTypeID)filterID);
	const bool newStateWillFilterDirectToCPU = ( !this->_cdv->Get3DPresenter()->WillFilterOnGPU() && !this->_cdv->Get3DPresenter()->GetSourceDeposterize() && (this->_cdv->Get3DPresenter()->GetPixelScaler() != VideoFilterTypeID_None) );
	
	pthread_mutex_unlock(&this->_mutexPixelScaler);
	pthread_mutex_unlock(&this->_mutexSourceDeposterize);
	
	if (oldStateWillFilterDirectToCPU != newStateWillFilterDirectToCPU)
	{
		if (newStateWillFilterDirectToCPU)
		{
			this->_macFetchObj->IncrementViewsUsingDirectToCPUFiltering();
		}
		else
		{
			this->_macFetchObj->DecrementViewsUsingDirectToCPUFiltering();
		}
	}
	
	this->Dispatch(MESSAGE_REPROCESS_AND_REDRAW);
}

void MacDisplayViewOutput::HandleCopyToPasteboard()
{
	CocoaDisplayView *cocoaDV = (CocoaDisplayView *)this->_clientObject;
	[cocoaDV handleCopyToPasteboard];
}
