/*
	Copyright (C) 2013-2017 DeSmuME team

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

#import "DisplayWindowController.h"
#import "EmuControllerDelegate.h"
#import "InputManager.h"

#import "cocoa_core.h"
#import "cocoa_GPU.h"
#import "cocoa_file.h"
#import "cocoa_input.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#include "MacOGLDisplayView.h"

#ifdef ENABLE_APPLE_METAL
#include "MacMetalDisplayView.h"
#endif

#include <Carbon/Carbon.h>

#if defined(__ppc__) || defined(__ppc64__)
#include <map>
#elif !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_6)
#include <tr1/unordered_map>
#else
#include <unordered_map>
#endif


@implementation DisplayWindowController

@synthesize dummyObject;
@synthesize emuControl;
@synthesize cdsVideoOutput;
@synthesize assignedScreen;
@synthesize masterWindow;
@synthesize view;
@synthesize saveScreenshotPanelAccessoryView;
@synthesize outputVolumeControlView;
@synthesize microphoneGainControlView;
@synthesize outputVolumeMenuItem;
@synthesize microphoneGainMenuItem;
@synthesize microphoneGainSlider;
@synthesize microphoneMuteButton;

@dynamic isFullScreen;
@dynamic displayScale;
@dynamic displayRotation;
@dynamic displayMainVideoSource;
@dynamic displayTouchVideoSource;
@dynamic videoFiltersPreferGPU;
@dynamic videoSourceDeposterize;
@dynamic videoOutputFilter;
@dynamic videoPixelScaler;
@dynamic displayMode;
@dynamic displayOrientation;
@dynamic displayOrder;
@dynamic displayGap;
@synthesize screenshotFileFormat;
@dynamic isMinSizeNormal;
@dynamic isShowingStatusBar;

#if defined(__ppc__) || defined(__ppc64__)
static std::map<NSScreen *, DisplayWindowController *> _screenMap; // Key = NSScreen object pointer, Value = DisplayWindowController object pointer
#elif !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_6)
static std::tr1::unordered_map<NSScreen *, DisplayWindowController *> _screenMap; // Key = NSScreen object pointer, Value = DisplayWindowController object pointer
#else
static std::unordered_map<NSScreen *, DisplayWindowController *> _screenMap; // Key = NSScreen object pointer, Value = DisplayWindowController object pointer

#endif

- (id)initWithWindowNibName:(NSString *)windowNibName emuControlDelegate:(EmuControllerDelegate *)theEmuController
{
	self = [super initWithWindowNibName:windowNibName];
	if (self == nil)
	{
		return self;
	}
	
	emuControl = [theEmuController retain];
	cdsVideoOutput = nil;
	assignedScreen = nil;
	masterWindow = nil;
	screenshotFileFormat = NSTIFFFileType;
	
	// These need to be initialized first since there are dependencies on these.
	_localViewProps.normalWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_localViewProps.normalHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2.0;
	_localViewProps.clientWidth = _localViewProps.normalWidth;
	_localViewProps.clientHeight = _localViewProps.normalHeight;
	_localViewProps.rotation = 0.0;
	_localViewProps.viewScale = 1.0;
	_localViewProps.gapScale = 0.0;
	_localViewProps.gapDistance = DS_DISPLAY_UNSCALED_GAP * _localViewProps.gapScale;
	_localViewProps.mode = ClientDisplayMode_Dual;
	_localViewProps.layout = ClientDisplayLayout_Vertical;
	_localViewProps.order = ClientDisplayOrder_MainFirst;
	
	_localViewScale = _localViewProps.viewScale;
	_localRotation = 0.0;
	_minDisplayViewSize = NSMakeSize(_localViewProps.clientWidth, _localViewProps.clientHeight + _localViewProps.gapDistance);
	_isMinSizeNormal = YES;
	_statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
	_isUpdatingDisplayScaleValueOnly = NO;
	_canUseMavericksFullScreen = IsOSXVersionSupported(10, 9, 0);
	_masterWindowScale = 1.0;
	_masterWindowFrame = NSMakeRect(0.0, 0.0, _localViewProps.clientWidth, _localViewProps.clientHeight + _localViewProps.gapDistance);
	_masterStatusBarState = NO;
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(saveScreenshotAsFinish:)
												 name:@"org.desmume.DeSmuME.requestScreenshotDidFinish"
											   object:nil];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(respondToScreenChange:)
												 name:@"NSApplicationDidChangeScreenParametersNotification"
											   object:NSApp];
	
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[self setEmuControl:nil];
	[self setAssignedScreen:nil];
	[self setMasterWindow:nil];
	[self setCdsVideoOutput:nil];
	
	[super dealloc];
}

#pragma mark Dynamic Property Methods

- (BOOL) isFullScreen
{
	return ([self assignedScreen] != nil);
}

- (void) setDisplayScale:(double)s
{
	// There are two ways that this property is used:
	// 1. Update the displayScale value only (usually as the result of a window resize)
	// 2. Resize the window as a result of setting displayScale
	//
	// Use the _isUpdatingDisplayScaleValueOnly flag to control this property's behavior.
	_localViewScale = s;
	
	if (!_isUpdatingDisplayScaleValueOnly)
	{
		// When the window resizes, this property's value will be implicitly updated using _isUpdatingDisplayScaleValueOnly.
		[self resizeWithTransform];
	}
}

- (double) displayScale
{
	return _localViewScale;
}

- (void) setDisplayRotation:(double)angleDegrees
{
	double newAngleDegrees = fmod(angleDegrees, 360.0);
	if (newAngleDegrees < 0.0)
	{
		newAngleDegrees = 360.0 + newAngleDegrees;
	}
	
	if (newAngleDegrees == 360.0)
	{
		newAngleDegrees = 0.0;
	}
	
	// Convert angle to clockwise-direction degrees (left-handed Cartesian coordinate system).
	_localRotation = newAngleDegrees;
	_localViewProps.rotation = 360.0 - _localRotation;
	
	// Set the minimum content size for the window, since this will change based on rotation.
	[self setIsMinSizeNormal:[self isMinSizeNormal]];
	
	if ([self isFullScreen])
	{
		[[self cdsVideoOutput] commitViewProperties:_localViewProps];
	}
	else
	{
		// Resize the window.
		NSWindow *theWindow = [self window];
		const NSSize oldBounds = [theWindow frame].size;
		[self resizeWithTransform];
		const NSSize newBounds = [theWindow frame].size;
		
		// If the window size didn't change, it is possible that the old and new rotation angles
		// are 180 degrees offset from each other. In this case, we'll need to force the
		// display view to update itself.
		if (oldBounds.width == newBounds.width && oldBounds.height == newBounds.height)
		{
			[[self cdsVideoOutput] commitViewProperties:_localViewProps];
		}
	}
}

- (double) displayRotation
{
	return _localRotation;
}

- (void) setDisplayMainVideoSource:(NSInteger)displaySourceID
{
	[[self view] setDisplayMainVideoSource:displaySourceID];
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_RELOAD_REPROCESS_REDRAW];
}

- (NSInteger) displayMainVideoSource
{
	return [[self view] displayMainVideoSource];
}

- (void) setDisplayTouchVideoSource:(NSInteger)displaySourceID
{
	[[self view] setDisplayTouchVideoSource:displaySourceID];
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_RELOAD_REPROCESS_REDRAW];
}

- (NSInteger) displayTouchVideoSource
{
	return [[self view] displayTouchVideoSource];
}

- (void) setDisplayMode:(NSInteger)displayModeID
{
	const BOOL willModeChangeSize = ( ((displayModeID == ClientDisplayMode_Main) || (displayModeID == ClientDisplayMode_Touch)) && (_localViewProps.mode == ClientDisplayMode_Dual) ) ||
	                                ( ((_localViewProps.mode == ClientDisplayMode_Main) || (_localViewProps.mode == ClientDisplayMode_Touch)) && (displayModeID == ClientDisplayMode_Dual) );
	_localViewProps.mode = (ClientDisplayMode)displayModeID;
	
	ClientDisplayView::CalculateNormalSize((ClientDisplayMode)displayModeID, (ClientDisplayLayout)[self displayOrientation], [self displayGap], _localViewProps.normalWidth, _localViewProps.normalHeight);
	[self setIsMinSizeNormal:[self isMinSizeNormal]];
	
	if (![self isFullScreen] && willModeChangeSize)
	{
		[self resizeWithTransform];
	}
	else
	{
		[[self cdsVideoOutput] commitViewProperties:_localViewProps];
	}
}

- (NSInteger) displayMode
{
	return _localViewProps.mode;
}

- (void) setDisplayOrientation:(NSInteger)theOrientation
{
	_localViewProps.layout = (ClientDisplayLayout)theOrientation;
	
	ClientDisplayView::CalculateNormalSize((ClientDisplayMode)[self displayMode], (ClientDisplayLayout)theOrientation, [self displayGap], _localViewProps.normalWidth, _localViewProps.normalHeight);
	[self setIsMinSizeNormal:[self isMinSizeNormal]];
	
	if ([self displayMode] == ClientDisplayMode_Dual)
	{
		if ([self isFullScreen])
		{
			[[self cdsVideoOutput] commitViewProperties:_localViewProps];
		}
		else
		{
			[self resizeWithTransform];
		}
	}
}

- (NSInteger) displayOrientation
{
	return _localViewProps.layout;
}

- (void) setDisplayOrder:(NSInteger)theOrder
{
	_localViewProps.order = (ClientDisplayOrder)theOrder;
	[[self cdsVideoOutput] commitViewProperties:_localViewProps];
}

- (NSInteger) displayOrder
{
	return _localViewProps.order;
}

- (void) setDisplayGap:(double)gapScalar
{
	_localViewProps.gapScale = gapScalar;
	ClientDisplayView::CalculateNormalSize((ClientDisplayMode)[self displayMode], (ClientDisplayLayout)[self displayOrientation], gapScalar, _localViewProps.normalWidth, _localViewProps.normalHeight);
	[self setIsMinSizeNormal:[self isMinSizeNormal]];
	
	if ([self displayMode] == ClientDisplayMode_Dual)
	{
		if ([self isFullScreen])
		{
			switch ([self displayOrientation])
			{
				case ClientDisplayLayout_Horizontal:
				case ClientDisplayLayout_Hybrid_2_1:
					break;
					
				case ClientDisplayLayout_Hybrid_16_9:
				case ClientDisplayLayout_Hybrid_16_10:
				default:
					[[self cdsVideoOutput] commitViewProperties:_localViewProps];
					break;
			}
		}
		else
		{
			switch ([self displayOrientation])
			{
				case ClientDisplayLayout_Hybrid_16_9:
				case ClientDisplayLayout_Hybrid_16_10:
					[[self cdsVideoOutput] commitViewProperties:_localViewProps];
					break;
					
				case ClientDisplayLayout_Horizontal:
				case ClientDisplayLayout_Hybrid_2_1:
					break;
					
				default:
					[self resizeWithTransform];
					break;
			}
		}
	}
}

- (double) displayGap
{
	return _localViewProps.gapScale;
}

- (void) setVideoFiltersPreferGPU:(BOOL)theState
{
	[[self view] setVideoFiltersPreferGPU:theState];
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_RELOAD_REPROCESS_REDRAW];
}

- (BOOL) videoFiltersPreferGPU
{
	return [[self view] videoFiltersPreferGPU];
}

- (void) setVideoSourceDeposterize:(BOOL)theState
{
	[[self view] setSourceDeposterize:theState];
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_RELOAD_REPROCESS_REDRAW];
}

- (BOOL) videoSourceDeposterize
{
	return [[self view] sourceDeposterize];
}

- (void) setVideoOutputFilter:(NSInteger)filterID
{
	[[self view] setOutputFilter:filterID];
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_REDRAW_VIEW];
}

- (NSInteger) videoOutputFilter
{
	return [[self view] outputFilter];
}

- (void) setVideoPixelScaler:(NSInteger)filterID
{
	[[self view] setPixelScaler:filterID];
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_RELOAD_REPROCESS_REDRAW];
}

- (NSInteger) videoPixelScaler
{
	return [[self view] pixelScaler];
}

- (void) setIsMinSizeNormal:(BOOL)theState
{
	_isMinSizeNormal = theState;
	_minDisplayViewSize.width = _localViewProps.normalWidth;
	_minDisplayViewSize.height = _localViewProps.normalHeight;
	
	if (!_isMinSizeNormal)
	{
		_minDisplayViewSize.width /= 4.0;
		_minDisplayViewSize.height /= 4.0;
	}
	
	// Set the minimum content size, keeping the display rotation in mind.
	double transformedMinWidth = _minDisplayViewSize.width;
	double transformedMinHeight = _minDisplayViewSize.height;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, _localViewProps.rotation, transformedMinWidth, transformedMinHeight);
	[[self window] setContentMinSize:NSMakeSize(transformedMinWidth, transformedMinHeight + _statusBarHeight)];
}

- (BOOL) isMinSizeNormal
{
	return _isMinSizeNormal;
}

- (void) setIsShowingStatusBar:(BOOL)showStatusBar
{
	NSRect frameRect = [masterWindow frame];
	
	if (showStatusBar)
	{
		_statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
		frameRect.size.height += WINDOW_STATUS_BAR_HEIGHT;
		
		NSRect screenFrame = [[masterWindow screen] visibleFrame];
		if (frameRect.size.height > screenFrame.size.height)
		{
			NSRect windowContentRect = [[masterWindow contentView] bounds];
			double widthToHeightRatio = windowContentRect.size.width / windowContentRect.size.height;
			windowContentRect.size.height -= frameRect.size.height - screenFrame.size.height;
			windowContentRect.size.width = windowContentRect.size.height * widthToHeightRatio;
			
			frameRect.size.height = screenFrame.size.height;
			frameRect.size.width = windowContentRect.size.width;			
		}
		else
		{
			frameRect.origin.y -= WINDOW_STATUS_BAR_HEIGHT;
		}
	}
	else
	{
		_statusBarHeight = 0;
		frameRect.origin.y += WINDOW_STATUS_BAR_HEIGHT;
		frameRect.size.height -= WINDOW_STATUS_BAR_HEIGHT;
	}
	
	[[NSUserDefaults standardUserDefaults] setBool:showStatusBar forKey:@"DisplayView_ShowStatusBar"];
	[masterWindow setFrame:frameRect display:YES animate:NO];
}

- (BOOL) isShowingStatusBar
{
	return !(_statusBarHeight == 0);
}

#pragma mark Class Methods

- (ClientDisplayViewProperties &) localViewProperties
{
	return _localViewProps;
}

- (void) setDisplayMode:(ClientDisplayMode)mode
			  viewScale:(double)viewScale
			   rotation:(double)rotation
				 layout:(ClientDisplayLayout)layout
				  order:(ClientDisplayOrder)order
			   gapScale:(double)gapScale
		isMinSizeNormal:(BOOL)isMinSizeNormal
	 isShowingStatusBar:(BOOL)isShowingStatusBar
{
	_statusBarHeight = (isShowingStatusBar) ? WINDOW_STATUS_BAR_HEIGHT : 0;
	
	_localViewScale = viewScale;
	_localRotation = fmod(rotation, 360.0);
	if (_localRotation < 0.0)
	{
		_localRotation = 360.0 + _localRotation;
	}
	
	if (_localRotation == 360.0)
	{
		_localRotation = 0.0;
	}
	
	_localViewProps.mode		= mode;
	_localViewProps.layout		= layout;
	_localViewProps.order		= order;
	_localViewProps.viewScale	= _localViewScale;
	_localViewProps.gapScale	= gapScale;
	_localViewProps.gapDistance	= DS_DISPLAY_UNSCALED_GAP * gapScale;
	_localViewProps.rotation	= 360.0 - _localRotation;
	
	ClientDisplayView::CalculateNormalSize(mode, layout, gapScale, _localViewProps.normalWidth, _localViewProps.normalHeight);
	
	// Set the minimum content size.
	_isMinSizeNormal = isMinSizeNormal;
	_minDisplayViewSize.width = _localViewProps.normalWidth;
	_minDisplayViewSize.height = _localViewProps.normalHeight;
	
	if (!_isMinSizeNormal)
	{
		_minDisplayViewSize.width /= 4.0;
		_minDisplayViewSize.height /= 4.0;
	}
	
	double transformedMinWidth = _minDisplayViewSize.width;
	double transformedMinHeight = _minDisplayViewSize.height;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, _localViewProps.rotation, transformedMinWidth, transformedMinHeight);
	[[self window] setContentMinSize:NSMakeSize(transformedMinWidth, transformedMinHeight + _statusBarHeight)];
	
	// Set the client size and resize the window.
	const NSRect newWindowFrame = [self updateViewProperties];
	const NSSize oldBounds = [masterWindow frame].size;
	[masterWindow setFrame:newWindowFrame display:YES animate:NO];
	const NSSize newBounds = [masterWindow frame].size;
	
	// Just because the window size didn't change doesn't mean that other view properties haven't
	// changed. Assume that view properties were changed and force them to update, whether the
	// window size changed or not.
	if (oldBounds.width == newBounds.width && oldBounds.height == newBounds.height)
	{
		[[self cdsVideoOutput] commitViewProperties:_localViewProps];
	}
}

- (void) setVideoPropertiesWithoutUpdateUsingPreferGPU:(BOOL)preferGPU
									 sourceDeposterize:(BOOL)useDeposterize
										  outputFilter:(NSInteger)outputFilterID
										   pixelScaler:(NSInteger)pixelScalerID
{
	[[self view] setVideoFiltersPreferGPU:preferGPU];
	[[self view] setSourceDeposterize:useDeposterize];
	[[self view] setOutputFilter:outputFilterID];
	[[self view] setPixelScaler:pixelScalerID];
}

- (void) setupUserDefaults
{
	[self setDisplayMode:(ClientDisplayMode)[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_Mode"]
			   viewScale:([[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayView_Size"] / 100.0)
				rotation:[[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayView_Rotation"]
				  layout:(ClientDisplayLayout)[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Orientation"]
				   order:(ClientDisplayOrder)[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Order"]
				gapScale:([[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayViewCombo_Gap"] / 100.0)
		 isMinSizeNormal:[self isMinSizeNormal]
	  isShowingStatusBar:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_ShowStatusBar"]];
	
	[self setDisplayMainVideoSource:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_DisplayMainVideoSource"]];
	[self setDisplayTouchVideoSource:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_DisplayTouchVideoSource"]];
	
	[self setVideoPropertiesWithoutUpdateUsingPreferGPU:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_FiltersPreferGPU"]
									  sourceDeposterize:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_Deposterize"]
										   outputFilter:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_OutputFilter"]
											pixelScaler:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"]];
	
	[[self view] setIsHUDVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_EnableHUD"]];
	[[self view] setIsHUDVideoFPSVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowVideoFPS"]];
	[[self view] setIsHUDRender3DFPSVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowRender3DFPS"]];
	[[self view] setIsHUDFrameIndexVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowFrameIndex"]];
	[[self view] setIsHUDLagFrameCountVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowLagFrameCount"]];
	[[self view] setIsHUDCPULoadAverageVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowCPULoadAverage"]];
	[[self view] setIsHUDRealTimeClockVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowRTC"]];
	// TODO: Show HUD Input.
	//[[self view] setIsHUDInputVisible:[[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowInput"]];
	
	[[self view] setHudColorVideoFPS:[CocoaDSUtil NSColorFromRGBA8888:[[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_VideoFPS"]]];
	[[self view] setHudColorRender3DFPS:[CocoaDSUtil NSColorFromRGBA8888:[[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Render3DFPS"]]];
	[[self view] setHudColorFrameIndex:[CocoaDSUtil NSColorFromRGBA8888:[[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_FrameIndex"]]];
	[[self view] setHudColorLagFrameCount:[CocoaDSUtil NSColorFromRGBA8888:[[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_LagFrameCount"]]];
	[[self view] setHudColorCPULoadAverage:[CocoaDSUtil NSColorFromRGBA8888:[[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_CPULoadAverage"]]];
	[[self view] setHudColorRTC:[CocoaDSUtil NSColorFromRGBA8888:[[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_RTC"]]];
}

- (BOOL) masterStatusBarState
{
	return (![self isFullScreen] || !_canUseMavericksFullScreen) ? [self isShowingStatusBar] : _masterStatusBarState;
}

- (NSRect) masterWindowFrame
{
	return (![self isFullScreen] || !_canUseMavericksFullScreen) ? [masterWindow frame] : _masterWindowFrame;
}

- (double) masterWindowScale
{
	return (![self isFullScreen] || !_canUseMavericksFullScreen) ? [self displayScale] : _masterWindowScale;
}

- (NSRect) updateViewProperties
{
	// Get the maximum scalar size within drawBounds.
	double checkWidth = _localViewProps.normalWidth;
	double checkHeight = _localViewProps.normalHeight;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, _localViewProps.rotation, checkWidth, checkHeight);
	
	const double maxViewScaleInHostScreen = [self maxViewScaleInHostScreen:checkWidth height:checkHeight];
	if (_localViewScale > maxViewScaleInHostScreen)
	{
		_localViewScale = maxViewScaleInHostScreen;
	}
	
	// Get the new bounds for the window's content view based on the transformed draw bounds.
	double transformedWidth = _localViewProps.normalWidth;
	double transformedHeight = _localViewProps.normalHeight;
	ClientDisplayView::ConvertNormalToTransformedBounds(_localViewScale, _localViewProps.rotation, transformedWidth, transformedHeight);
	
	// Get the center of the content view in screen coordinates.
	const NSRect windowContentRect = [[masterWindow contentView] bounds];
	const double translationX = (windowContentRect.size.width - transformedWidth) / 2.0;
	const double translationY = ((windowContentRect.size.height - _statusBarHeight) - transformedHeight) / 2.0;
	
	// Resize the window.
	const NSRect windowFrame = [masterWindow frame];
	const NSRect newFrame = [masterWindow frameRectForContentRect:NSMakeRect(windowFrame.origin.x + translationX, windowFrame.origin.y + translationY, transformedWidth, transformedHeight + _statusBarHeight)];
	
	return newFrame;
}

- (void) resizeWithTransform
{
	if ([self isFullScreen])
	{
		return;
	}
	
	NSRect newWindowFrame = [self updateViewProperties];
	[masterWindow setFrame:newWindowFrame display:YES animate:NO];
}

- (double) maxViewScaleInHostScreen:(double)contentBoundsWidth height:(double)contentBoundsHeight
{
	// Determine the maximum scale based on the visible screen size (which
	// doesn't include the menu bar or dock).
	const NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
	const NSRect windowFrame = [[self window] frameRectForContentRect:NSMakeRect(0.0, 0.0, contentBoundsWidth, contentBoundsHeight + _statusBarHeight)];
	const NSSize visibleScreenBounds = { (screenFrame.size.width - (windowFrame.size.width - contentBoundsWidth)), (screenFrame.size.height - (windowFrame.size.height - contentBoundsHeight)) };
	
	return ClientDisplayView::GetMaxScalarWithinBounds(contentBoundsWidth, contentBoundsHeight, visibleScreenBounds.width, visibleScreenBounds.height);
}

- (void) saveScreenshotAsFinish:(NSNotification *)aNotification
{
	NSURL *fileURL = (NSURL *)[[aNotification userInfo] valueForKey:@"fileURL"];
	NSBitmapImageFileType fileType = (NSBitmapImageFileType)[(NSNumber *)[[aNotification userInfo] valueForKey:@"fileType"] integerValue];
	NSImage *screenshotImage = (NSImage *)[[aNotification userInfo] valueForKey:@"screenshotImage"];
	
	const BOOL fileSaved = [CocoaDSFile saveScreenshot:fileURL bitmapData:(NSBitmapImageRep *)[[screenshotImage representations] objectAtIndex:0] fileType:fileType];
	if (!fileSaved)
	{
		NSAlert *theAlert = [[NSAlert alloc] init];
		[theAlert setMessageText:NSSTRING_ALERT_SCREENSHOT_FAILED_TITLE];
		[theAlert setInformativeText:NSSTRING_ALERT_SCREENSHOT_FAILED_MESSAGE];
		[theAlert setAlertStyle:NSCriticalAlertStyle];
		
		[theAlert runModal];
		
		[theAlert release];
	}
	
	[emuControl restoreCoreState];
}

- (void) enterFullScreen
{
	NSScreen *targetScreen = [masterWindow screen];
	
	// If there is a window that is already assigned to the target screen, then force the
	// current window to exit full screen first.
	if (_screenMap.find(targetScreen) != _screenMap.end())
	{
		DisplayWindowController *currentFullScreenWindow = _screenMap[targetScreen];
		[currentFullScreenWindow exitFullScreen];
	}
	
	[[self window] orderOut:nil];
	
	// Since we'll be using the screen rect to position the window, we need to set the origin
	// to (0,0) since creating the new full screen window requires the screen rect to be in
	// screen coordinates.
	NSRect screenRect = [targetScreen frame];
	screenRect.origin.x = 0.0;
	screenRect.origin.y = 0.0;
	
	DisplayFullScreenWindow *newFullScreenWindow = [[[DisplayFullScreenWindow alloc] initWithContentRect:screenRect
																							   styleMask:NSBorderlessWindowMask
																								 backing:NSBackingStoreBuffered
																								   defer:NO
																								  screen:targetScreen] autorelease];
	[newFullScreenWindow setHasShadow:NO];
	[view setFrame:screenRect];
	[[newFullScreenWindow contentView] addSubview:view];
	[newFullScreenWindow setInitialFirstResponder:view];
	[newFullScreenWindow setDelegate:self];
	
	// If the target screen is the main screen (index 0), then autohide the menu bar and dock.
	if (targetScreen == [[NSScreen screens] objectAtIndex:0])
	{
		SetSystemUIMode(kUIModeAllHidden, kUIOptionAutoShowMenuBar);
	}
	
	// Show the full screen window.
	[self setWindow:newFullScreenWindow];
	[newFullScreenWindow makeKeyAndOrderFront:self];
	[newFullScreenWindow makeMainWindow];
	[newFullScreenWindow display];
	
	[self setAssignedScreen:targetScreen];
	_screenMap[targetScreen] = self;
}

- (void) exitFullScreen
{
	_screenMap.erase([self assignedScreen]);
	[self setAssignedScreen:nil];
	[[self window] orderOut:nil];
	
	// If the window is using the main screen (index 0), then restore the menu bar and dock.
	if ([masterWindow screen] == [[NSScreen screens] objectAtIndex:0])
	{
		SetSystemUIMode(kUIModeNormal, 0);
	}
	
	[self setWindow:masterWindow];
	[self resizeWithTransform];
	
	NSRect viewFrame = [[masterWindow contentView] frame];
	viewFrame.size.height -= _statusBarHeight;
	viewFrame.origin.y = _statusBarHeight;
    
	[view setFrame:viewFrame];
	[[masterWindow contentView] addSubview:view];
    [masterWindow setInitialFirstResponder:view];
	[masterWindow makeKeyAndOrderFront:self];
	[masterWindow makeMainWindow];
	[masterWindow display];
}

- (void) respondToScreenChange:(NSNotification *)aNotification
{
	if (_canUseMavericksFullScreen)
	{
		return;
	}
	else
	{
		// This method only applies for displays in full screen mode. For displays in
		// windowed mode, we don't need to do anything.
		if (![self isFullScreen])
		{
			return;
		}
		
		NSArray *screenList = [NSScreen screens];
		
		// If the assigned screen was disconnected, exit full screen mode. Hopefully, the
		// window will automatically move onto an available screen.
		if (![screenList containsObject:[self assignedScreen]])
		{
			[self exitFullScreen];
		}
		else
		{
			// There are many other reasons that a screen change would occur, but the only
			// other one we care about is a resolution change. Let's just assume that a
			// resolution change occurred and resize the full screen window.
			NSRect screenRect = [assignedScreen frame];
			[[self window] setFrame:screenRect display:NO];
			
			screenRect.origin.x = 0.0;
			screenRect.origin.y = 0.0;
			[view setFrame:screenRect];
		}
	}
}

#pragma mark IBActions

- (IBAction) copy:(id)sender
{
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_COPY_TO_PASTEBOARD];
}

- (IBAction) changeHardwareMicGain:(id)sender
{
	[emuControl changeHardwareMicGain:sender];
}

- (IBAction) changeHardwareMicMute:(id)sender
{
	[emuControl changeHardwareMicMute:sender];
}

- (IBAction) changeVolume:(id)sender
{
	[emuControl changeVolume:sender];
}

- (IBAction) toggleHUDVisibility:(id)sender
{
	[[self view] setIsHUDVisible:![[self view] isHUDVisible]];
}

- (IBAction) toggleShowHUDVideoFPS:(id)sender
{
	[[self view] setIsHUDVideoFPSVisible:![[self view] isHUDVideoFPSVisible]];
}

- (IBAction) toggleShowHUDRender3DFPS:(id)sender
{
	[[self view] setIsHUDRender3DFPSVisible:![[self view] isHUDRender3DFPSVisible]];
}

- (IBAction) toggleShowHUDFrameIndex:(id)sender
{
	[[self view] setIsHUDFrameIndexVisible:![[self view] isHUDFrameIndexVisible]];
}

- (IBAction) toggleShowHUDLagFrameCount:(id)sender
{
	[[self view] setIsHUDLagFrameCountVisible:![[self view] isHUDLagFrameCountVisible]];
}

- (IBAction) toggleShowHUDInput:(id)sender
{
	//[[self view] setIsHUDInputVisible:![[self view] isHUDInputVisible]];
}

- (IBAction) toggleShowHUDCPULoadAverage:(id)sender
{
	[[self view] setIsHUDCPULoadAverageVisible:![[self view] isHUDCPULoadAverageVisible]];
}

- (IBAction) toggleShowHUDRealTimeClock:(id)sender
{
	[[self view] setIsHUDRealTimeClockVisible:![[self view] isHUDRealTimeClockVisible]];
}

- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender
{	
	if ([self isMinSizeNormal])
	{
		[self setIsMinSizeNormal:NO];
	}
	else
	{
		[self setIsMinSizeNormal:YES];
		
		// Set the minimum content size, keeping the display rotation in mind.
		double transformedMinWidth = _minDisplayViewSize.width;
		double transformedMinHeight = _minDisplayViewSize.height;
		ClientDisplayView::ConvertNormalToTransformedBounds(1.0, _localViewProps.rotation, transformedMinWidth, transformedMinHeight);
		transformedMinHeight += _statusBarHeight;
		
		// Resize the window if it's smaller than the minimum content size.
		NSRect windowContentRect = [masterWindow contentRectForFrameRect:[masterWindow frame]];
		if (windowContentRect.size.width < transformedMinWidth || windowContentRect.size.height < transformedMinHeight)
		{
			// Prepare to resize.
			NSRect oldFrameRect = [masterWindow frame];
			windowContentRect.size = NSMakeSize(transformedMinWidth, transformedMinHeight);
			NSRect newFrameRect = [masterWindow frameRectForContentRect:windowContentRect];
			
			// Keep the window centered when expanding the size.
			newFrameRect.origin.x = oldFrameRect.origin.x - ((newFrameRect.size.width - oldFrameRect.size.width) / 2);
			newFrameRect.origin.y = oldFrameRect.origin.y - ((newFrameRect.size.height - oldFrameRect.size.height) / 2);
			
			// Set the window size.
			[masterWindow setFrame:newFrameRect display:YES animate:NO];
		}
	}
}

- (IBAction) toggleStatusBar:(id)sender
{
	[self setIsShowingStatusBar:([self isShowingStatusBar]) ? NO : YES];
}

- (IBAction) toggleFullScreenDisplay:(id)sender
{
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
	if (_canUseMavericksFullScreen)
	{
		[masterWindow toggleFullScreen:nil];
	}
	else
#endif
	{
		if ([self isFullScreen])
		{
			[self exitFullScreen];
		}
		else
		{
			[self enterFullScreen];
		}
	}
}

- (IBAction) toggleExecutePause:(id)sender
{
	[emuControl toggleExecutePause:sender];
}

- (IBAction) frameAdvance:(id)sender
{
	[emuControl frameAdvance:sender];
}

- (IBAction) reset:(id)sender
{
	[emuControl reset:sender];
}

- (IBAction) changeCoreSpeed:(id)sender
{
	[emuControl changeCoreSpeed:sender];
}

- (IBAction) openRom:(id)sender
{
	[emuControl openRom:sender];
}

- (IBAction) saveScreenshotAs:(id)sender
{
	[emuControl pauseCore];
	
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setCanCreateDirectories:YES];
	[panel setTitle:NSSTRING_TITLE_SAVE_SCREENSHOT_PANEL];
	[panel setAccessoryView:saveScreenshotPanelAccessoryView];
	
	const NSInteger buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		[view requestScreenshot:[panel URL] fileType:(NSBitmapImageFileType)[self screenshotFileFormat]];
	}
	else
	{
		[emuControl restoreCoreState];
	}
}

- (IBAction) changeScale:(id)sender
{
	[self setDisplayScale:(double)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0];
}

- (IBAction) changeRotation:(id)sender
{
	// Get the rotation value from the sender.
	if ([sender isMemberOfClass:[NSSlider class]])
	{
		[self setDisplayRotation:[(NSSlider *)sender doubleValue]];
	}
	else
	{
		[self setDisplayRotation:(double)[CocoaDSUtil getIBActionSenderTag:sender]];
	}
}

- (IBAction) changeRotationRelative:(id)sender
{
	const double angleDegrees = [self displayRotation] + (double)[CocoaDSUtil getIBActionSenderTag:sender];
	[self setDisplayRotation:angleDegrees];
}

- (IBAction) changeDisplayMode:(id)sender
{
	const NSInteger newDisplayModeID = [CocoaDSUtil getIBActionSenderTag:sender];
	
	if (newDisplayModeID == [self displayMode])
	{
		return;
	}
	
	[self setDisplayMode:newDisplayModeID];
}

- (IBAction) changeDisplayOrientation:(id)sender
{
	const NSInteger newDisplayOrientation = [CocoaDSUtil getIBActionSenderTag:sender];
	
	if (newDisplayOrientation == [self displayOrientation])
	{
		return;
	}
	
	[self setDisplayOrientation:newDisplayOrientation];
}

- (IBAction) changeDisplayOrder:(id)sender
{
	[self setDisplayOrder:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeDisplayGap:(id)sender
{
	[self setDisplayGap:(double)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0];
}

- (IBAction) changeDisplayVideoSource:(id)sender
{
	NSInteger tag = [CocoaDSUtil getIBActionSenderTag:sender];
	
	if (tag >= DISPLAY_VIDEO_SOURCE_TOUCH_TAG_BASE)
	{
		if ((tag-DISPLAY_VIDEO_SOURCE_TOUCH_TAG_BASE) == [self displayTouchVideoSource])
		{
			return;
		}
		
		[self setDisplayTouchVideoSource:tag-DISPLAY_VIDEO_SOURCE_TOUCH_TAG_BASE];
	}
	else
	{
		if ((tag-DISPLAY_VIDEO_SOURCE_MAIN_TAG_BASE) == [self displayMainVideoSource])
		{
			return;
		}
		
		[self setDisplayMainVideoSource:tag-DISPLAY_VIDEO_SOURCE_MAIN_TAG_BASE];
	}
}

- (IBAction) toggleVideoFiltersPreferGPU:(id)sender
{
	[self setVideoFiltersPreferGPU:![self videoFiltersPreferGPU]];
}

- (IBAction) toggleVideoSourceDeposterize:(id)sender
{
	[self setVideoSourceDeposterize:![self videoSourceDeposterize]];
}

- (IBAction) changeVideoOutputFilter:(id)sender
{
	[self setVideoOutputFilter:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeVideoPixelScaler:(id)sender
{
	[self setVideoPixelScaler:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) toggleNDSDisplays:(id)sender
{
	[self setDisplayOrder:([self displayOrder] == ClientDisplayOrder_MainFirst) ? ClientDisplayOrder_TouchFirst : ClientDisplayOrder_MainFirst];
}

- (IBAction) writeDefaultsDisplayRotation:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setDouble:[self displayRotation] forKey:@"DisplayView_Rotation"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsDisplayGap:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setDouble:([self displayGap] * 100.0) forKey:@"DisplayViewCombo_Gap"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsHUDSettings:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDVisible] forKey:@"DisplayView_EnableHUD"];
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDVideoFPSVisible] forKey:@"HUD_ShowVideoFPS"];
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDRender3DFPSVisible] forKey:@"HUD_ShowRender3DFPS"];
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDFrameIndexVisible] forKey:@"HUD_ShowFrameIndex"];
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDLagFrameCountVisible] forKey:@"HUD_ShowLagFrameCount"];
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDCPULoadAverageVisible] forKey:@"HUD_ShowCPULoadAverage"];
	[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDRealTimeClockVisible] forKey:@"HUD_ShowRTC"];
	// TODO: Show HUD Input.
	//[[NSUserDefaults standardUserDefaults] setBool:[[self view] isHUDInputVisible] forKey:@"HUD_ShowInput"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[CocoaDSUtil RGBA8888FromNSColor:[[self view] hudColorVideoFPS]] forKey:@"HUD_Color_VideoFPS"];
	[[NSUserDefaults standardUserDefaults] setInteger:[CocoaDSUtil RGBA8888FromNSColor:[[self view] hudColorRender3DFPS]] forKey:@"HUD_Color_Render3DFPS"];
	[[NSUserDefaults standardUserDefaults] setInteger:[CocoaDSUtil RGBA8888FromNSColor:[[self view] hudColorFrameIndex]] forKey:@"HUD_Color_FrameIndex"];
	[[NSUserDefaults standardUserDefaults] setInteger:[CocoaDSUtil RGBA8888FromNSColor:[[self view] hudColorLagFrameCount]] forKey:@"HUD_Color_LagFrameCount"];
	[[NSUserDefaults standardUserDefaults] setInteger:[CocoaDSUtil RGBA8888FromNSColor:[[self view] hudColorCPULoadAverage]] forKey:@"HUD_Color_CPULoadAverage"];
	[[NSUserDefaults standardUserDefaults] setInteger:[CocoaDSUtil RGBA8888FromNSColor:[[self view] hudColorRTC]] forKey:@"HUD_Color_RTC"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsDisplayVideoSettings:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setBool:[self videoFiltersPreferGPU] forKey:@"DisplayView_FiltersPreferGPU"];
	[[NSUserDefaults standardUserDefaults] setBool:[self videoSourceDeposterize] forKey:@"DisplayView_Deposterize"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self videoOutputFilter] forKey:@"DisplayView_OutputFilter"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self videoPixelScaler] forKey:@"DisplayView_VideoFilter"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

#pragma mark NSUserInterfaceValidations Protocol

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)theItem
{
	BOOL enable = YES;
    const SEL theAction = [theItem action];
	
	if (theAction == @selector(changeScale:))
	{
		const NSInteger viewScale = (NSInteger)([self displayScale] * 100.0);
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:(viewScale == [theItem tag]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(changeRotation:))
	{
		const NSInteger viewRotation = (NSInteger)[self displayRotation];
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([theItem tag] == -1)
			{
				if (viewRotation == 0 ||
					viewRotation == 90 ||
					viewRotation == 180 ||
					viewRotation == 270)
				{
					[(NSMenuItem *)theItem setState:NSOffState];
				}
				else
				{
					[(NSMenuItem *)theItem setState:NSOnState];
				}
			}
			else
			{
				[(NSMenuItem *)theItem setState:(viewRotation == [theItem tag]) ? NSOnState : NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeDisplayMode:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self displayMode] == [theItem tag]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(changeDisplayOrientation:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self displayOrientation] == [theItem tag]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(changeDisplayOrder:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self displayOrder] == [theItem tag]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(changeDisplayGap:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			const NSInteger gapScalar = (NSInteger)([self displayGap] * 100.0);
			
			if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
			{
				if ([theItem tag] == -1)
				{
					if (gapScalar == 0 ||
						gapScalar == 50 ||
						gapScalar == 100 ||
						gapScalar == 150 ||
						gapScalar == 200)
					{
						[(NSMenuItem *)theItem setState:NSOffState];
					}
					else
					{
						[(NSMenuItem *)theItem setState:NSOnState];
					}
				}
				else
				{
					[(NSMenuItem *)theItem setState:(gapScalar == [theItem tag]) ? NSOnState : NSOffState];
				}
			}
		}
	}
	else if (theAction == @selector(changeDisplayVideoSource:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([theItem tag] >= DISPLAY_VIDEO_SOURCE_TOUCH_TAG_BASE)
			{
				[(NSMenuItem *)theItem setState:([self displayTouchVideoSource] == ([theItem tag]-DISPLAY_VIDEO_SOURCE_TOUCH_TAG_BASE)) ? NSOnState : NSOffState];
			}
			else
			{
				[(NSMenuItem *)theItem setState:([self displayMainVideoSource]  == ([theItem tag]-DISPLAY_VIDEO_SOURCE_MAIN_TAG_BASE)) ? NSOnState : NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeVideoOutputFilter:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self videoOutputFilter] == [theItem tag]) ? NSOnState : NSOffState];
			enable = ([theItem tag] == OutputFilterTypeID_NearestNeighbor || [theItem tag] == OutputFilterTypeID_Bilinear) || [[self view] canUseShaderBasedFilters];
		}
	}
	else if (theAction == @selector(toggleVideoSourceDeposterize:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self videoSourceDeposterize]) ? NSOnState : NSOffState];
		}
		
		enable = [[self view] canUseShaderBasedFilters];
	}
	else if (theAction == @selector(toggleVideoFiltersPreferGPU:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] videoFiltersPreferGPU]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(changeVideoPixelScaler:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self videoPixelScaler] == [theItem tag]) ? NSOnState : NSOffState];
			
			bool isSupportingCPU = false;
			bool isSupportingShader = false;
			OGLFilter::GetSupport([theItem tag], &isSupportingCPU, &isSupportingShader);
			
			enable = isSupportingCPU || (isSupportingShader && [[self view] canUseShaderBasedFilters]);
		}
	}
	else if (theAction == @selector(toggleHUDVisibility:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleShowHUDVideoFPS:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDVideoFPSVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleShowHUDRender3DFPS:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDRender3DFPSVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleShowHUDFrameIndex:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDFrameIndexVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleShowHUDLagFrameCount:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDLagFrameCountVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleShowHUDCPULoadAverage:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDCPULoadAverageVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleShowHUDRealTimeClock:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([[self view] isHUDRealTimeClockVisible]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleStatusBar:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setTitle:([self isShowingStatusBar]) ? NSSTRING_TITLE_HIDE_STATUS_BAR : NSSTRING_TITLE_SHOW_STATUS_BAR];
		}
		
		if ([self isFullScreen])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleFullScreenDisplay:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setTitle:([self isFullScreen]) ? NSSTRING_TITLE_EXIT_FULL_SCREEN : NSSTRING_TITLE_ENTER_FULL_SCREEN];
		}
	}
	else if (theAction == @selector(toggleKeepMinDisplaySizeAtNormal:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self isMinSizeNormal]) ? NSOnState : NSOffState];
		}
		
		if ([self isFullScreen])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleToolbarShown:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setTitle:([[[self window] toolbar] isVisible]) ? NSSTRING_TITLE_HIDE_TOOLBAR : NSSTRING_TITLE_SHOW_TOOLBAR];
		}
	}
	
	return enable;
}

#pragma mark NSWindowDelegate Protocol

- (void)windowDidLoad
{
	// Set up the master window that is associated with this window controller.
	[self setMasterWindow:[self window]];
	[masterWindow setTitle:(NSString *)[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"]];
	[masterWindow setInitialFirstResponder:view];
	[[emuControl windowList] addObject:self];
	[emuControl updateAllWindowTitles];
	
	[view setupLayer];
	[view setInputManager:[emuControl inputManager]];
	
	// Set up the scaling factor if this is a Retina window
	float scaleFactor = 1.0f;
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
	if ([masterWindow respondsToSelector:@selector(backingScaleFactor)])
	{
		scaleFactor = [masterWindow backingScaleFactor];
	}
    
	if (_canUseMavericksFullScreen)
	{
		[masterWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	}
#endif
	
	// Set up some custom UI elements.
	[microphoneGainMenuItem setView:microphoneGainControlView];
	[outputVolumeMenuItem setView:outputVolumeControlView];
	
	// Set up the video output thread.
	CocoaDSDisplayVideo *newDisplayOutput = [[CocoaDSDisplayVideo alloc] init];
	[newDisplayOutput setClientDisplayView:[view clientDisplay3DView]];
	
	ClientDisplayView *cdv = [newDisplayOutput clientDisplayView];
	NSString *fontPath = [[NSBundle mainBundle] pathForResource:@"SourceSansPro-Bold" ofType:@"otf"];
	cdv->SetHUDFontPath([fontPath cStringUsingEncoding:NSUTF8StringEncoding]);
	
	if (scaleFactor != 1.0f)
	{
		[newDisplayOutput setScaleFactor:scaleFactor];
	}
	else
	{
		cdv->LoadHUDFont();
	}
	
	[self setCdsVideoOutput:newDisplayOutput];
	[view setCdsVideoOutput:newDisplayOutput];
	
	// Add the video thread to the output list.
	[emuControl addOutputToCore:newDisplayOutput];
	
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:newDisplayOutput withObject:nil];
	while ([newDisplayOutput thread] == nil)
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
	
	// Setup default values per user preferences.
	[self setupUserDefaults];
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
	[emuControl setMainWindow:self];
	[emuControl updateDisplayPanelTitles];
	[view setNextResponder:[self window]];
	[[view inputManager] setHidInputTarget:view];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
	if ([self isFullScreen])
	{
		return frameSize;
	}
	
	// Get a content Rect so that we can make our comparison.
	// This will be based on the proposed frameSize.
	const NSRect frameRect = NSMakeRect(0.0f, 0.0f, frameSize.width, frameSize.height);
	const NSRect contentRect = [sender contentRectForFrameRect:frameRect];
	
	// Find the maximum scalar we can use for the display view, bounded by the content Rect.
	double checkWidth = _localViewProps.normalWidth;
	double checkHeight = _localViewProps.normalHeight;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, _localViewProps.rotation, checkWidth, checkHeight);
	const NSSize contentBounds = NSMakeSize(contentRect.size.width, contentRect.size.height - _statusBarHeight);
	_localViewScale = ClientDisplayView::GetMaxScalarWithinBounds(checkWidth, checkHeight, contentBounds.width, contentBounds.height);
	
	// Make a new content Rect with our max scalar, and convert it back to a frame Rect.
	const NSRect finalContentRect = NSMakeRect(0.0f, 0.0f, checkWidth * _localViewScale, (checkHeight * _localViewScale) + _statusBarHeight);
	const NSRect finalFrameRect = [sender frameRectForContentRect:finalContentRect];
	
	// Set the final size based on our new frame Rect.
	return finalFrameRect.size;
}

- (void)windowDidResize:(NSNotification *)notification
{
	if ([self isFullScreen])
	{
		return;
	}
	
	NSRect newContentFrame = [[[self window] contentView] bounds];
	newContentFrame.origin.y = _statusBarHeight;
	newContentFrame.size.height -= _statusBarHeight;
	[view setFrame:newContentFrame];
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window defaultFrame:(NSRect)newFrame
{
	if ([self isFullScreen])
	{
		return newFrame;
	}
	
	NSScreen *targetScreen = [window screen];
	
	if (newFrame.size.height > [targetScreen visibleFrame].size.height)
	{
		newFrame.size.height = [targetScreen visibleFrame].size.height;
	}
	
	if (newFrame.size.width > [targetScreen visibleFrame].size.width)
	{
		newFrame.size.width = [targetScreen visibleFrame].size.width;
	}
	
	NSRect currentFrame = [window frame];
	BOOL isZooming = (newFrame.size.width > currentFrame.size.width) || (newFrame.size.height > currentFrame.size.height);
	
	// Get a content Rect so that we can make our comparison.
	// This will be based on the proposed frameSize.
	const NSRect frameRect = newFrame;
	const NSRect contentRect = [window contentRectForFrameRect:frameRect];
	
	// Find the maximum scalar we can use for the display view, bounded by the content Rect.
	double checkWidth = _localViewProps.normalWidth;
	double checkHeight = _localViewProps.normalHeight;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, _localViewProps.rotation, checkWidth, checkHeight);
	const NSSize contentBounds = NSMakeSize(contentRect.size.width, contentRect.size.height - _statusBarHeight);
	const double maxS = ClientDisplayView::GetMaxScalarWithinBounds(checkWidth, checkHeight, contentBounds.width, contentBounds.height);
	
	// Make a new content Rect with our max scalar, and convert it back to a frame Rect.
	const NSRect finalContentRect = NSMakeRect(0.0f, 0.0f, checkWidth * maxS, (checkHeight * maxS) + _statusBarHeight);
	NSRect finalFrameRect = [window frameRectForContentRect:finalContentRect];
	
	if (isZooming)
	{
		finalFrameRect.origin.x = ((currentFrame.origin.x) + (currentFrame.size.width / 2.0f)) - (finalFrameRect.size.width / 2.0f);
	}
	
	// Set the final size based on our new frame Rect.
	return finalFrameRect;
}

- (BOOL)windowShouldClose:(id)sender
{
	BOOL shouldClose = YES;
	
	if ([[emuControl windowList] count] > 1) // If this isn't the last window, then just close it without doing anything else.
	{
		return shouldClose;
	}
	else if ([emuControl currentRom] != nil) // If a ROM is loaded, just close the ROM, but don't close the window.
	{
		[emuControl closeRom:nil];
		shouldClose = NO;
	}
	
	return shouldClose;
}

- (void)windowWillClose:(NSNotification *)notification
{
	[emuControl removeOutputFromCore:[self cdsVideoOutput]];
	[[self cdsVideoOutput] forceThreadExit];
	[[self cdsVideoOutput] release];
	[self setCdsVideoOutput:nil];
	
	[[emuControl windowList] removeObject:self];
	
	if ([[emuControl windowList] count] < 1)
	{
		if ([emuControl currentRom] != nil)
		{
			[emuControl closeRom:nil];
		}
		
		[NSApp terminate:[notification object]];
		return;
	}
	
	[emuControl updateAllWindowTitles];
	[emuControl updateDisplayPanelTitles];
}

#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize
{
	return [[window screen] frame].size;
}

- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
	NSApplicationPresentationOptions options = (NSApplicationPresentationHideDock |
												NSApplicationPresentationAutoHideMenuBar |
												NSApplicationPresentationFullScreen |
												NSApplicationPresentationAutoHideToolbar);
	
#if defined(MAC_OS_X_VERSION_10_11_2) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11)
	options |= NSApplicationPresentationDisableCursorLocationAssistance;
#endif
	
	return options;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
	_masterWindowScale = [self displayScale];
	_masterWindowFrame = [masterWindow frame];
	_masterStatusBarState = [self isShowingStatusBar];
	[self setIsShowingStatusBar:NO];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
	NSScreen *targetScreen = [masterWindow screen];
	[self setAssignedScreen:targetScreen];
	_screenMap[targetScreen] = self;
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
	_screenMap.erase([self assignedScreen]);
	[self setAssignedScreen:nil];
	[self setIsShowingStatusBar:_masterStatusBarState];
}

#endif

- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem
{
	BOOL enable = YES;
    const SEL theAction = [theItem action];
	
	if (theAction == @selector(toggleExecutePause:))
    {
		if (![emuControl masterExecuteFlag] ||
			[emuControl currentRom] == nil ||
			[emuControl isUserInterfaceBlockingExecution])
		{
			enable = NO;
		}
		
		if ([emuControl executionState] == ExecutionBehavior_Pause)
		{
			[theItem setLabel:NSSTRING_TITLE_EXECUTE_CONTROL];
			[theItem setImage:[emuControl iconExecute]];
		}
		else
		{
			[theItem setLabel:NSSTRING_TITLE_PAUSE_CONTROL];
			[theItem setImage:[emuControl iconPause]];
		}
    }
	else if (theAction == @selector(frameAdvance:))
    {
		if (![emuControl masterExecuteFlag] ||
			[emuControl currentRom] == nil ||
			[emuControl isUserInterfaceBlockingExecution] ||
			[emuControl executionState] != ExecutionBehavior_Pause)
		{
			enable = NO;
		}
    }
	else if (theAction == @selector(reset:))
	{
		if ([emuControl currentRom] == nil || [emuControl isUserInterfaceBlockingExecution])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(changeCoreSpeed:))
	{
		NSInteger speedScalar = (NSInteger)([emuControl speedScalar] * 100.0);
		
		if (speedScalar != (NSInteger)(SPEED_SCALAR_NORMAL * 100.0))
		{
			[theItem setLabel:NSSTRING_TITLE_SPEED_1X];
			[theItem setTag:100];
			[theItem setImage:[emuControl iconSpeedNormal]];
		}
		else
		{
			[theItem setLabel:NSSTRING_TITLE_SPEED_2X];
			[theItem setTag:200];
			[theItem setImage:[emuControl iconSpeedDouble]];
		}
	}
	else if (theAction == @selector(openRom:))
	{
		if ([emuControl isRomLoading] || [emuControl isShowingSaveStateDialog])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleHUDVisibility:))
	{
		if ([[self view] isHUDVisible])
		{
			[theItem setLabel:NSSTRING_TITLE_DISABLE_HUD];
		}
		else
		{
			[theItem setLabel:NSSTRING_TITLE_ENABLE_HUD];
		}
	}
	
	return enable;
}

@end

#pragma mark -
@implementation DisplayView

@synthesize inputManager;
@synthesize cdsVideoOutput;
@dynamic clientDisplay3DView;
@dynamic canUseShaderBasedFilters;
@dynamic allowViewUpdates;
@dynamic isHUDVisible;
@dynamic isHUDVideoFPSVisible;
@dynamic isHUDRender3DFPSVisible;
@dynamic isHUDFrameIndexVisible;
@dynamic isHUDLagFrameCountVisible;
@dynamic isHUDCPULoadAverageVisible;
@dynamic isHUDRealTimeClockVisible;
@dynamic hudColorVideoFPS;
@dynamic hudColorRender3DFPS;
@dynamic hudColorFrameIndex;
@dynamic hudColorLagFrameCount;
@dynamic hudColorCPULoadAverage;
@dynamic hudColorRTC;
@dynamic displayMainVideoSource;
@dynamic displayTouchVideoSource;
@dynamic useVerticalSync;
@dynamic videoFiltersPreferGPU;
@dynamic sourceDeposterize;
@dynamic outputFilter;
@dynamic pixelScaler;

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self == nil)
	{
		return self;
	}
	
	inputManager = nil;
	cdsVideoOutput = nil;
	localLayer = nil;
	localOGLContext = nil;
	
	return self;
}

- (void)dealloc
{
	[self setInputManager:nil];
	[self setCdsVideoOutput:nil];
	[self setLayer:nil];
	
	if (localOGLContext != nil)
	{
		[localOGLContext clearDrawable];
		[localOGLContext release];
	}
	
	[localLayer release];
	
	[super dealloc];
}

#pragma mark Dynamic Property Methods

- (ClientDisplay3DView *) clientDisplay3DView
{
	return [(id<DisplayViewCALayer>)localLayer clientDisplay3DView];
}

- (BOOL) canUseShaderBasedFilters
{
	return [[self cdsVideoOutput] canFilterOnGPU];
}

- (BOOL) allowViewUpdates
{
	return ([self clientDisplay3DView]->GetAllowViewUpdates()) ? YES : NO;
}

- (void) setAllowViewUpdates:(BOOL)allowUpdates
{
	[self clientDisplay3DView]->SetAllowViewUpdates((allowUpdates) ? true : false);
}

- (void) setIsHUDVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDVisible:theState];
}

- (BOOL) isHUDVisible
{
	return [[self cdsVideoOutput] isHUDVisible];
}

- (void) setIsHUDVideoFPSVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDVideoFPSVisible:theState];
}

- (BOOL) isHUDVideoFPSVisible
{
	return [[self cdsVideoOutput] isHUDVideoFPSVisible];
}

- (void) setIsHUDRender3DFPSVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDRender3DFPSVisible:theState];
}

- (BOOL) isHUDRender3DFPSVisible
{
	return [[self cdsVideoOutput] isHUDRender3DFPSVisible];
}

- (void) setIsHUDFrameIndexVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDFrameIndexVisible:theState];
}

- (BOOL) isHUDFrameIndexVisible
{
	return [[self cdsVideoOutput] isHUDFrameIndexVisible];
}

- (void) setIsHUDLagFrameCountVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDLagFrameCountVisible:theState];
}

- (BOOL) isHUDLagFrameCountVisible
{
	return [[self cdsVideoOutput] isHUDLagFrameCountVisible];
}

- (void) setIsHUDCPULoadAverageVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDCPULoadAverageVisible:theState];
}

- (BOOL) isHUDCPULoadAverageVisible
{
	return [[self cdsVideoOutput] isHUDCPULoadAverageVisible];
}

- (void) setIsHUDRealTimeClockVisible:(BOOL)theState
{
	[[self cdsVideoOutput] setIsHUDRealTimeClockVisible:theState];
}

- (BOOL) isHUDRealTimeClockVisible
{
	return [[self cdsVideoOutput] isHUDRealTimeClockVisible];
}

- (void) setHudColorVideoFPS:(NSColor *)theColor
{
	[[self cdsVideoOutput] setHudColorVideoFPS:[CocoaDSUtil RGBA8888FromNSColor:theColor]];
}

- (NSColor *) hudColorVideoFPS
{
	return [CocoaDSUtil NSColorFromRGBA8888:[[self cdsVideoOutput] hudColorVideoFPS]];
}

- (void) setHudColorRender3DFPS:(NSColor *)theColor
{
	[[self cdsVideoOutput] setHudColorRender3DFPS:[CocoaDSUtil RGBA8888FromNSColor:theColor]];
}

- (NSColor *) hudColorRender3DFPS
{
	return [CocoaDSUtil NSColorFromRGBA8888:[[self cdsVideoOutput] hudColorRender3DFPS]];
}

- (void) setHudColorFrameIndex:(NSColor *)theColor
{
	[[self cdsVideoOutput] setHudColorFrameIndex:[CocoaDSUtil RGBA8888FromNSColor:theColor]];
}

- (NSColor *) hudColorFrameIndex
{
	return [CocoaDSUtil NSColorFromRGBA8888:[[self cdsVideoOutput] hudColorFrameIndex]];
}

- (void) setHudColorLagFrameCount:(NSColor *)theColor
{
	[[self cdsVideoOutput] setHudColorLagFrameCount:[CocoaDSUtil RGBA8888FromNSColor:theColor]];
}

- (NSColor *) hudColorLagFrameCount
{
	return [CocoaDSUtil NSColorFromRGBA8888:[[self cdsVideoOutput] hudColorLagFrameCount]];
}

- (void) setHudColorCPULoadAverage:(NSColor *)theColor
{
	[[self cdsVideoOutput] setHudColorCPULoadAverage:[CocoaDSUtil RGBA8888FromNSColor:theColor]];
}

- (NSColor *) hudColorCPULoadAverage
{
	return [CocoaDSUtil NSColorFromRGBA8888:[[self cdsVideoOutput] hudColorCPULoadAverage]];
}

- (void) setHudColorRTC:(NSColor *)theColor
{
	[[self cdsVideoOutput] setHudColorRTC:[CocoaDSUtil RGBA8888FromNSColor:theColor]];
}

- (NSColor *) hudColorRTC
{
	return [CocoaDSUtil NSColorFromRGBA8888:[[self cdsVideoOutput] hudColorRTC]];
}

- (void) setDisplayMainVideoSource:(NSInteger)displaySourceID
{
	[[self cdsVideoOutput] setDisplayMainVideoSource:displaySourceID];
}

- (NSInteger) displayMainVideoSource
{
	return [[self cdsVideoOutput] displayMainVideoSource];
}

- (void) setDisplayTouchVideoSource:(NSInteger)displaySourceID
{
	[[self cdsVideoOutput] setDisplayTouchVideoSource:displaySourceID];
}

- (NSInteger) displayTouchVideoSource
{
	return [[self cdsVideoOutput] displayTouchVideoSource];
}

- (void) setUseVerticalSync:(BOOL)theState
{
	[[self cdsVideoOutput] setUseVerticalSync:theState];
}

- (BOOL) useVerticalSync
{
	return [[self cdsVideoOutput] useVerticalSync];
}

- (void) setVideoFiltersPreferGPU:(BOOL)theState
{
	const BOOL oldState = ( ![[self cdsVideoOutput] willFilterOnGPU] && ![[self cdsVideoOutput] sourceDeposterize] && ([[self cdsVideoOutput] pixelScaler] != VideoFilterTypeID_None) );
	[[self cdsVideoOutput] setVideoFiltersPreferGPU:theState];
	const BOOL newState = ( ![[self cdsVideoOutput] willFilterOnGPU] && ![[self cdsVideoOutput] sourceDeposterize] && ([[self cdsVideoOutput] pixelScaler] != VideoFilterTypeID_None) );
	
	if (oldState != newState)
	{
		DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
		CocoaDSCore *cdsCore = (CocoaDSCore *)[[[windowController emuControl] cdsCoreController] content];
		CocoaDSGPU *cdsGPU = [cdsCore cdsGPU];
		MacClientSharedObject *macSharedData = [cdsGPU sharedData];
		
		if (newState)
		{
			[macSharedData incrementViewsUsingDirectToCPUFiltering];
		}
		else
		{
			[macSharedData decrementViewsUsingDirectToCPUFiltering];
		}
	}
}

- (BOOL) videoFiltersPreferGPU
{
	return [[self cdsVideoOutput] videoFiltersPreferGPU];
}

- (void) setSourceDeposterize:(BOOL)theState
{
	const BOOL oldState = ( ![[self cdsVideoOutput] willFilterOnGPU] && ![[self cdsVideoOutput] sourceDeposterize] && ([[self cdsVideoOutput] pixelScaler] != VideoFilterTypeID_None) );
	[[self cdsVideoOutput] setSourceDeposterize:theState];
	const BOOL newState = ( ![[self cdsVideoOutput] willFilterOnGPU] && ![[self cdsVideoOutput] sourceDeposterize] && ([[self cdsVideoOutput] pixelScaler] != VideoFilterTypeID_None) );
	
	if (oldState != newState)
	{
		DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
		CocoaDSCore *cdsCore = (CocoaDSCore *)[[[windowController emuControl] cdsCoreController] content];
		CocoaDSGPU *cdsGPU = [cdsCore cdsGPU];
		MacClientSharedObject *macSharedData = [cdsGPU sharedData];
		
		if (newState)
		{
			[macSharedData incrementViewsUsingDirectToCPUFiltering];
		}
		else
		{
			[macSharedData decrementViewsUsingDirectToCPUFiltering];
		}
	}
}

- (BOOL) sourceDeposterize
{
	return [[self cdsVideoOutput] sourceDeposterize];
}

- (void) setOutputFilter:(NSInteger)filterID
{
	[[self cdsVideoOutput] setOutputFilter:filterID];
}

- (NSInteger) outputFilter
{
	return [[self cdsVideoOutput] outputFilter];
}

- (void) setPixelScaler:(NSInteger)filterID
{
	const BOOL oldState = ( ![[self cdsVideoOutput] willFilterOnGPU] && ![[self cdsVideoOutput] sourceDeposterize] && ([[self cdsVideoOutput] pixelScaler] != VideoFilterTypeID_None) );
	[[self cdsVideoOutput] setPixelScaler:filterID];
	const BOOL newState = ( ![[self cdsVideoOutput] willFilterOnGPU] && ![[self cdsVideoOutput] sourceDeposterize] && ([[self cdsVideoOutput] pixelScaler] != VideoFilterTypeID_None) );
	
	if (oldState != newState)
	{
		DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
		CocoaDSCore *cdsCore = (CocoaDSCore *)[[[windowController emuControl] cdsCoreController] content];
		CocoaDSGPU *cdsGPU = [cdsCore cdsGPU];
		MacClientSharedObject *macSharedData = [cdsGPU sharedData];
		
		if (newState)
		{
			[macSharedData incrementViewsUsingDirectToCPUFiltering];
		}
		else
		{
			[macSharedData decrementViewsUsingDirectToCPUFiltering];
		}
	}
}

- (NSInteger) pixelScaler
{
	return [[self cdsVideoOutput] pixelScaler];
}

#pragma mark Class Methods
- (void) setupLayer
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[[[windowController emuControl] cdsCoreController] content];
	CocoaDSGPU *cdsGPU = [cdsCore cdsGPU];
	
#ifdef ENABLE_APPLE_METAL
	MacClientSharedObject *macSharedData = [cdsGPU sharedData];
	if ((macSharedData != nil) && [macSharedData isKindOfClass:[MetalDisplayViewSharedData class]])
	{
		localLayer = [[DisplayViewMetalLayer alloc] init];
		[(DisplayViewMetalLayer *)localLayer setSharedData:(MetalDisplayViewSharedData *)macSharedData];
		
		MacMetalDisplayView *cdv = (MacMetalDisplayView *)[(id<DisplayViewCALayer>)localLayer clientDisplay3DView];
		cdv->SetFetchObject([cdsGPU fetchObject]);
		cdv->Init();
		
		if ([(DisplayViewMetalLayer *)localLayer device] == nil)
		{
			[localLayer release];
			localLayer = nil;
		}
	}
#endif
	
	if (localLayer == nil)
	{
		localLayer = [[DisplayViewOpenGLLayer alloc] init];
		MacOGLDisplayView *macOGLCDV = (MacOGLDisplayView *)[(id<DisplayViewCALayer>)localLayer clientDisplay3DView];
		macOGLCDV->SetFetchObject([cdsGPU fetchObject]);
		macOGLCDV->Init();
		
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
				[self setWantsBestResolutionOpenGLSurface:YES];
			}
#endif
			localOGLContext = macOGLCDV->GetNSContext();
			[localOGLContext retain];
			macOGLCDV->SetRenderToCALayer(false);
		}
	}
	
	if (localOGLContext != nil)
	{
		// If localOGLContext isn't nil, then we will not assign the local layer
		// directly to the view, since the OpenGL context will already be what
		// is assigned.
		return;
	}
	
	[localLayer setNeedsDisplay];
    [self setLayer:localLayer];
    [self setWantsLayer:YES];
    
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
    if ([self respondsToSelector:@selector(setLayerContentsRedrawPolicy:)])
    {
        [self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
    }
#endif
}

#pragma mark InputHIDManagerTarget Protocol
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue hidManager:(InputHIDManager *)hidManager
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	
	InputAttributesList inputList = InputManagerEncodeHIDQueue(hidQueue, [hidManager inputManager], false);
	NSString *newStatusText = nil;
	
	const size_t inputCount = inputList.size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = inputList[i];
		
		if (inputAttr.isAnalog)
		{
			newStatusText = [NSString stringWithFormat:@"%s:%s (%1.2f)", inputAttr.deviceName, inputAttr.elementName, inputAttr.scalar];
			break;
		}
		else if (inputAttr.state == INPUT_ATTRIBUTE_STATE_ON)
		{
			newStatusText = [NSString stringWithFormat:@"%s:%s", inputAttr.deviceName, inputAttr.elementName];
			break;
		}
	}
	
	if (newStatusText != nil)
	{
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	CommandAttributesList cmdList = [inputManager generateCommandListUsingInputList:&inputList];
	if (cmdList.empty())
	{
		return isHandled;
	}
	
	[inputManager dispatchCommandList:&cmdList];
	
	isHandled = YES;
	return isHandled;
}

- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	
	const InputAttributes inputAttr = InputManagerEncodeKeyboardInput([theEvent keyCode], keyPressed);
	
	if (keyPressed && [theEvent window] != nil)
	{
		NSString *newStatusText = [NSString stringWithFormat:@"%s:%s", inputAttr.deviceName, inputAttr.elementName];
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	isHandled = [inputManager dispatchCommandUsingInputAttributes:&inputAttr];
	return isHandled;
}

- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	ClientDisplayView *cdv = [(id<DisplayViewCALayer>)localLayer clientDisplay3DView];
	const ClientDisplayMode displayMode = cdv->GetMode();
	
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	const NSInteger buttonNumber = [theEvent buttonNumber];
	uint8_t x = 0;
	uint8_t y = 0;
	
	if (displayMode != ClientDisplayMode_Main)
	{
		const NSEventType eventType = [theEvent type];
		const bool isInitialMouseDown = (eventType == NSLeftMouseDown) || (eventType == NSRightMouseDown) || (eventType == NSOtherMouseDown);
		
		// Convert the clicked location from window coordinates, to view coordinates, and finally to NDS touchscreen coordinates.
		const NSPoint clientLoc = [self convertPoint:[theEvent locationInWindow] fromView:nil];
		cdv->GetNDSPoint((int)buttonNumber, isInitialMouseDown, clientLoc.x, clientLoc.y, x, y);
	}
	
	const InputAttributes inputAttr = InputManagerEncodeMouseButtonInput(buttonNumber, NSMakePoint(x, y), buttonPressed);
	
	if (buttonPressed && [theEvent window] != nil)
	{
		NSString *newStatusText = (displayMode == ClientDisplayMode_Main) ? [NSString stringWithFormat:@"%s:%s", inputAttr.deviceName, inputAttr.elementName] : [NSString stringWithFormat:@"%s:%s X:%i Y:%i", inputAttr.deviceName, inputAttr.elementName, (int)inputAttr.intCoordX, (int)inputAttr.intCoordY];
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	isHandled = [inputManager dispatchCommandUsingInputAttributes:&inputAttr];
	return isHandled;
}

- (void) requestScreenshot:(NSURL *)fileURL fileType:(NSBitmapImageFileType)fileType
{
	NSString *fileURLString = [fileURL absoluteString];
	NSData *fileURLStringData = [fileURLString dataUsingEncoding:NSUTF8StringEncoding];
	NSData *bitmapImageFileTypeData = [[NSData alloc] initWithBytes:&fileType length:sizeof(NSBitmapImageFileType)];
	NSArray *messageComponents = [[NSArray alloc] initWithObjects:fileURLStringData, bitmapImageFileTypeData, nil];
	
	[CocoaDSUtil messageSendOneWayWithMessageComponents:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_REQUEST_SCREENSHOT array:messageComponents];
	
	[bitmapImageFileTypeData release];
	[messageComponents release];
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
	[self clientDisplay3DView]->UpdateView();
}

- (void)drawRect:(NSRect)dirtyRect
{
	[self clientDisplay3DView]->UpdateView();
}

- (void)setFrame:(NSRect)rect
{
	NSRect oldFrame = [self frame];
	[super setFrame:rect];
	
	if (rect.size.width != oldFrame.size.width || rect.size.height != oldFrame.size.height)
	{
		DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
		ClientDisplayViewProperties &props = [windowController localViewProperties];
		NSRect newViewportRect = rect;
		
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
		if ([self respondsToSelector:@selector(convertRectToBacking:)])
		{
			newViewportRect = [self convertRectToBacking:rect];
		}
#endif
		
		// Calculate the view scale for the given client size.
		double checkWidth = props.normalWidth;
		double checkHeight = props.normalHeight;
		ClientDisplayView::ConvertNormalToTransformedBounds(1.0, props.rotation, checkWidth, checkHeight);
		
		props.clientWidth = newViewportRect.size.width;
		props.clientHeight = newViewportRect.size.height;
		props.viewScale = ClientDisplayView::GetMaxScalarWithinBounds(checkWidth, checkHeight, props.clientWidth, props.clientHeight);
		
		if (localOGLContext != nil)
		{
			[localOGLContext update];
		}
		else if ([localLayer isKindOfClass:[CAOpenGLLayer class]])
		{
			[localLayer setBounds:CGRectMake(0.0f, 0.0f, props.clientWidth, props.clientHeight)];
		}
#ifdef ENABLE_APPLE_METAL
		else if ([[self layer] isKindOfClass:[CAMetalLayer class]])
		{
			[(CAMetalLayer *)localLayer setDrawableSize:CGSizeMake(props.clientWidth, props.clientHeight)];
		}
#endif
		
		[[self cdsVideoOutput] commitViewProperties:props];
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
@implementation DisplayFullScreenWindow

#pragma mark NSWindow Methods
- (BOOL)canBecomeKeyWindow
{
	return YES;
}

- (BOOL)canBecomeMainWindow
{
	return YES;
}

@end
