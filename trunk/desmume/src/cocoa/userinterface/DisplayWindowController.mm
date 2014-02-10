/*
	Copyright (C) 2013-2014 DeSmuME team

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

#import "cocoa_file.h"
#import "cocoa_input.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#include "OGLDisplayOutput.h"
#include <Carbon/Carbon.h>

#if defined(__ppc__) || defined(__ppc64__)
#include <map>
#else
#include <tr1/unordered_map>
#endif


@implementation DisplayWindowController

@synthesize dummyObject;
@synthesize emuControl;
@synthesize cdsVideoOutput;
@synthesize assignedScreen;
@synthesize masterWindow;
@synthesize view;
@synthesize saveScreenshotPanelAccessoryView;

@dynamic normalSize;
@dynamic displayScale;
@dynamic displayRotation;
@dynamic useBilinearOutput;
@dynamic useVerticalSync;
@dynamic displayMode;
@dynamic displayOrientation;
@dynamic displayOrder;
@dynamic displayGap;
@dynamic videoFilterType;
@synthesize screenshotFileFormat;
@dynamic isMinSizeNormal;
@dynamic isShowingStatusBar;

#if defined(__ppc__) || defined(__ppc64__)
static std::map<NSScreen *, DisplayWindowController *> _screenMap; // Key = NSScreen object pointer, Value = DisplayWindowController object pointer
#else
static std::tr1::unordered_map<NSScreen *, DisplayWindowController *> _screenMap; // Key = NSScreen object pointer, Value = DisplayWindowController object pointer
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
	
	spinlockNormalSize = OS_SPINLOCK_INIT;
	spinlockScale = OS_SPINLOCK_INIT;
	spinlockRotation = OS_SPINLOCK_INIT;
	spinlockUseBilinearOutput = OS_SPINLOCK_INIT;
	spinlockUseVerticalSync = OS_SPINLOCK_INIT;
	spinlockDisplayMode = OS_SPINLOCK_INIT;
	spinlockDisplayOrientation = OS_SPINLOCK_INIT;
	spinlockDisplayOrder = OS_SPINLOCK_INIT;
	spinlockDisplayGap = OS_SPINLOCK_INIT;
	spinlockVideoFilterType = OS_SPINLOCK_INIT;
	
	screenshotFileFormat = NSTIFFFileType;
	
	// These need to be initialized first since there are dependencies on these.
	_displayGap = 0.0;
	_displayMode = DS_DISPLAY_TYPE_DUAL;
	_displayOrientation = DS_DISPLAY_ORIENTATION_VERTICAL;
	
	_minDisplayViewSize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*_displayGap));
	_isMinSizeNormal = YES;
	_statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
	_isWindowResizing = NO;
	
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
	
	[super dealloc];
}

#pragma mark Dynamic Property Methods

- (NSSize) normalSize
{
	OSSpinLockLock(&spinlockNormalSize);
	const NSSize theSize = _normalSize;
	OSSpinLockUnlock(&spinlockNormalSize);
	
	return theSize;
}

- (void) setDisplayScale:(double)s
{
	if (_isWindowResizing)
	{
		// Resize the window when displayScale changes.
		// No need to set the view's scale here since window resizing will implicitly change it.
		OSSpinLockLock(&spinlockScale);
		_displayScale = s;
		OSSpinLockUnlock(&spinlockScale);
	}
	else
	{
		const double constrainedScale = [self resizeWithTransform:[self normalSize] scalar:s rotation:[self displayRotation]];
		OSSpinLockLock(&spinlockScale);
		_displayScale = constrainedScale;
		OSSpinLockUnlock(&spinlockScale);
	}
}

- (double) displayScale
{
	OSSpinLockLock(&spinlockScale);
	const double s = _displayScale;
	OSSpinLockUnlock(&spinlockScale);
	
	return s;
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
	
	OSSpinLockLock(&spinlockRotation);
	_displayRotation = newAngleDegrees;
	OSSpinLockUnlock(&spinlockRotation);
	
	NSWindow *theWindow = [self window];
	
	// Set the minimum content size for the window, since this will change based on rotation.
	CGSize minContentSize = GetTransformedBounds(_minDisplayViewSize.width, _minDisplayViewSize.height, 1.0, CLOCKWISE_DEGREES(newAngleDegrees));
	minContentSize.height += _statusBarHeight;
	[theWindow setContentMinSize:NSMakeSize(minContentSize.width, minContentSize.height)];
	
	// Resize the window.
	const NSSize oldBounds = [theWindow frame].size;
	const double constrainedScale = [self resizeWithTransform:[self normalSize] scalar:[self displayScale] rotation:newAngleDegrees];
	const NSSize newBounds = [theWindow frame].size;
	
	OSSpinLockLock(&spinlockScale);
	_displayScale = constrainedScale;
	OSSpinLockUnlock(&spinlockScale);
	
	// If the window size didn't change, it is possible that the old and new rotation angles
	// are 180 degrees offset from each other. In this case, we'll need to force the
	// display view to update itself.
	if (oldBounds.width == newBounds.width && oldBounds.height == newBounds.height)
	{
		[view setNeedsDisplay:YES];
	}
	
	DisplayOutputTransformData transformData	= { constrainedScale,
												    angleDegrees,
												    0.0,
												    0.0,
												    0.0 };
	
	[CocoaDSUtil messageSendOneWayWithData:[[self cdsVideoOutput] receivePort]
									 msgID:MESSAGE_TRANSFORM_VIEW
									  data:[NSData dataWithBytes:&transformData length:sizeof(DisplayOutputTransformData)]];
}

- (double) displayRotation
{
	OSSpinLockLock(&spinlockRotation);
	const double angleDegrees = _displayRotation;
	OSSpinLockUnlock(&spinlockRotation);
	
	return angleDegrees;
}

- (void) setUseBilinearOutput:(BOOL)theState
{
	OSSpinLockLock(&spinlockUseBilinearOutput);
	_useBilinearOutput = theState;
	OSSpinLockUnlock(&spinlockUseBilinearOutput);
	
	[[self view] doBilinearOutputChanged:theState];
}

- (BOOL) useBilinearOutput
{
	OSSpinLockLock(&spinlockUseBilinearOutput);
	const BOOL theState = _useBilinearOutput;
	OSSpinLockUnlock(&spinlockUseBilinearOutput);
	
	return theState;
}

- (void) setUseVerticalSync:(BOOL)theState
{
	OSSpinLockLock(&spinlockUseVerticalSync);
	_useVerticalSync = theState;
	OSSpinLockUnlock(&spinlockUseVerticalSync);
	
	[[self view] doVerticalSyncChanged:theState];
}

- (BOOL) useVerticalSync
{
	OSSpinLockLock(&spinlockUseVerticalSync);
	const BOOL theState = _useVerticalSync;
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
			
		case DS_DISPLAY_TYPE_DUAL:
		{
			const double gapScalar = [self displayGap];
			modeString = NSSTRING_DISPLAYMODE_DUAL;
			
			if ([self displayOrientation] == DS_DISPLAY_ORIENTATION_VERTICAL)
			{
				newDisplaySize.height = newDisplaySize.height * 2.0 + (DS_DISPLAY_GAP * gapScalar);
			}
			else
			{
				newDisplaySize.width = newDisplaySize.width * 2.0 + (DS_DISPLAY_GAP * gapScalar);
			}
			
			break;
		}
			
		default:
			break;
	}
	
	OSSpinLockLock(&spinlockDisplayMode);
	_displayMode = displayModeID;
	OSSpinLockUnlock(&spinlockDisplayMode);
	
	OSSpinLockLock(&spinlockNormalSize);
	_normalSize = newDisplaySize;
	OSSpinLockUnlock(&spinlockNormalSize);
	
	[self setIsMinSizeNormal:[self isMinSizeNormal]];
	[self resizeWithTransform:[self normalSize] scalar:[self displayScale] rotation:[self displayRotation]];
	
	[CocoaDSUtil messageSendOneWayWithInteger:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_DISPLAY_TYPE integerValue:displayModeID];
}

- (NSInteger) displayMode
{
	OSSpinLockLock(&spinlockDisplayMode);
	const NSInteger displayModeID = _displayMode;
	OSSpinLockUnlock(&spinlockDisplayMode);
	
	return displayModeID;
}

- (void) setDisplayOrientation:(NSInteger)theOrientation
{
	OSSpinLockLock(&spinlockDisplayOrientation);
	_displayOrientation = theOrientation;
	OSSpinLockUnlock(&spinlockDisplayOrientation);
	
	if ([self displayMode] == DS_DISPLAY_TYPE_DUAL)
	{
		const double gapScalar = [self displayGap];
		NSSize newDisplaySize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT);
		
		if (theOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			newDisplaySize.height = newDisplaySize.height * 2.0 + (DS_DISPLAY_GAP * gapScalar);
		}
		else
		{
			newDisplaySize.width = newDisplaySize.width * 2.0 + (DS_DISPLAY_GAP * gapScalar);
		}
		
		OSSpinLockLock(&spinlockNormalSize);
		_normalSize = newDisplaySize;
		OSSpinLockUnlock(&spinlockNormalSize);
		
		[self setIsMinSizeNormal:[self isMinSizeNormal]];
		[self resizeWithTransform:[self normalSize] scalar:[self displayScale] rotation:[self displayRotation]];
	}
	
	[CocoaDSUtil messageSendOneWayWithInteger:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_DISPLAY_ORIENTATION integerValue:theOrientation];
}

- (NSInteger) displayOrientation
{
	OSSpinLockLock(&spinlockDisplayOrientation);
	const NSInteger theOrientation = _displayOrientation;
	OSSpinLockUnlock(&spinlockDisplayOrientation);
	
	return theOrientation;
}

- (void) setDisplayOrder:(NSInteger)theOrder
{
	OSSpinLockLock(&spinlockDisplayOrder);
	_displayOrder = theOrder;
	OSSpinLockUnlock(&spinlockDisplayOrder);
	
	[CocoaDSUtil messageSendOneWayWithInteger:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_DISPLAY_ORDER integerValue:theOrder];
}

- (NSInteger) displayOrder
{
	OSSpinLockLock(&spinlockDisplayOrder);
	const NSInteger theOrder = _displayOrder;
	OSSpinLockUnlock(&spinlockDisplayOrder);
	
	return theOrder;
}

- (void) setDisplayGap:(double)gapScalar
{
	OSSpinLockLock(&spinlockDisplayGap);
	_displayGap = gapScalar;
	OSSpinLockUnlock(&spinlockDisplayGap);
	
	if ([self displayMode] == DS_DISPLAY_TYPE_DUAL)
	{
		NSSize newDisplaySize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT);
		
		if ([self displayOrientation] == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			newDisplaySize.height = newDisplaySize.height * 2.0 + (DS_DISPLAY_GAP * gapScalar);
		}
		else
		{
			newDisplaySize.width = newDisplaySize.width * 2.0 + (DS_DISPLAY_GAP * gapScalar);
		}
		
		OSSpinLockLock(&spinlockNormalSize);
		_normalSize = newDisplaySize;
		OSSpinLockUnlock(&spinlockNormalSize);
		
		[self setIsMinSizeNormal:[self isMinSizeNormal]];
		[self resizeWithTransform:[self normalSize] scalar:[self displayScale] rotation:[self displayRotation]];
	}
	
	[CocoaDSUtil messageSendOneWayWithFloat:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_DISPLAY_GAP floatValue:(float)gapScalar];
}

- (double) displayGap
{
	OSSpinLockLock(&spinlockDisplayGap);
	const double gapScalar = _displayGap;
	OSSpinLockUnlock(&spinlockDisplayGap);
	
	return gapScalar;
}

- (void) setVideoFilterType:(NSInteger)typeID
{
	OSSpinLockLock(&spinlockVideoFilterType);
	_videoFilterType = typeID;
	OSSpinLockUnlock(&spinlockVideoFilterType);
	
	[CocoaDSUtil messageSendOneWayWithInteger:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_VIDEO_FILTER integerValue:typeID];
}

- (NSInteger) videoFilterType
{
	OSSpinLockLock(&spinlockVideoFilterType);
	const NSInteger typeID = _videoFilterType;
	OSSpinLockUnlock(&spinlockVideoFilterType);
	
	return typeID;
}

- (void) setIsMinSizeNormal:(BOOL)theState
{
	OSSpinLockLock(&spinlockDisplayGap);
	const double gapScalar = _displayGap;
	OSSpinLockUnlock(&spinlockDisplayGap);
	
	_isMinSizeNormal = theState;
	
	if ([self displayMode] == DS_DISPLAY_TYPE_DUAL)
	{
		if ([self displayOrientation] == DS_DISPLAY_ORIENTATION_HORIZONTAL)
		{
			_minDisplayViewSize.width = GPU_DISPLAY_WIDTH*2.0 + (DS_DISPLAY_GAP*gapScalar);
			_minDisplayViewSize.height = GPU_DISPLAY_HEIGHT;
		}
		else
		{
			_minDisplayViewSize.width = GPU_DISPLAY_WIDTH;
			_minDisplayViewSize.height = GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*gapScalar);
		}
	}
	else
	{
		_minDisplayViewSize.width = GPU_DISPLAY_WIDTH;
		_minDisplayViewSize.height = GPU_DISPLAY_HEIGHT;
	}
	
	if (!_isMinSizeNormal)
	{
		_minDisplayViewSize.width /= 4;
		_minDisplayViewSize.height /= 4;
	}
	
	// Set the minimum content size, keeping the display rotation in mind.
	CGSize transformedMinSize = GetTransformedBounds(_minDisplayViewSize.width, _minDisplayViewSize.height, 1.0, CLOCKWISE_DEGREES([self displayRotation]));
	transformedMinSize.height += _statusBarHeight;
	[[self window] setContentMinSize:NSMakeSize(transformedMinSize.width, transformedMinSize.height)];
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

- (void) setupUserDefaults
{
	// Set the display window per user preferences.
	[self setIsShowingStatusBar:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_ShowStatusBar"]];
	
	// Set the display settings per user preferences.
	[self setDisplayGap:([[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayViewCombo_Gap"] / 100.0)];
	[self setDisplayMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_Mode"]];
	[self setDisplayOrientation:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Orientation"]];
	[self setDisplayOrder:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Order"]];
	[self setDisplayScale:([[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayView_Size"] / 100.0)];
	[self setDisplayRotation:[[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayView_Rotation"]];
	[self setVideoFilterType:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"]];
	[self setUseBilinearOutput:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseBilinearOutput"]];
	[self setUseVerticalSync:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseVerticalSync"]];
}

- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees
{
	if ([self assignedScreen] != nil)
	{
		return scalar;
	}
		
	// Convert angle to clockwise-direction degrees.
	angleDegrees = CLOCKWISE_DEGREES(angleDegrees);
	
	// Get the maximum scalar size within drawBounds. Constrain scalar to maxScalar if necessary.
	const CGSize checkSize = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, angleDegrees);
	const double maxScalar = [self maxScalarForContentBoundsWidth:checkSize.width height:checkSize.height];
	if (scalar > maxScalar)
	{
		scalar = maxScalar;
	}
	
	// Get the new bounds for the window's content view based on the transformed draw bounds.
	const CGSize transformedBounds = GetTransformedBounds(normalBounds.width, normalBounds.height, scalar, angleDegrees);
	
	// Get the center of the content view in screen coordinates.
	const NSRect windowContentRect = [[masterWindow contentView] bounds];
	const double translationX = (windowContentRect.size.width - transformedBounds.width) / 2.0;
	const double translationY = ((windowContentRect.size.height - _statusBarHeight) - transformedBounds.height) / 2.0;
	
	// Resize the window.
	const NSRect windowFrame = [masterWindow frame];
	const NSRect newFrame = [masterWindow frameRectForContentRect:NSMakeRect(windowFrame.origin.x + translationX, windowFrame.origin.y + translationY, transformedBounds.width, transformedBounds.height + _statusBarHeight)];
	[masterWindow setFrame:newFrame display:YES animate:NO];
	
	// Return the actual scale used for the view (may be constrained).
	return scalar;
}

- (double) maxScalarForContentBoundsWidth:(double)contentBoundsWidth height:(double)contentBoundsHeight
{
	// Determine the maximum scale based on the visible screen size (which
	// doesn't include the menu bar or dock).
	const NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
	const NSRect windowFrame = [[self window] frameRectForContentRect:NSMakeRect(0.0, 0.0, contentBoundsWidth, contentBoundsHeight + _statusBarHeight)];
	const NSSize visibleScreenBounds = { (screenFrame.size.width - (windowFrame.size.width - contentBoundsWidth)), (screenFrame.size.height - (windowFrame.size.height - contentBoundsHeight)) };
	
	return GetMaxScalarInBounds(contentBoundsWidth, contentBoundsHeight, visibleScreenBounds.width, visibleScreenBounds.height);
}

- (void) saveScreenshotAsFinish:(NSNotification *)aNotification
{
	NSURL *fileURL = (NSURL *)[[aNotification userInfo] valueForKey:@"fileURL"];
	NSBitmapImageFileType fileType = (NSBitmapImageFileType)[(NSNumber *)[[aNotification userInfo] valueForKey:@"fileType"] integerValue];
	NSImage *screenshotImage = (NSImage *)[[aNotification userInfo] valueForKey:@"screenshotImage"];
	
	const BOOL fileSaved = [CocoaDSFile saveScreenshot:fileURL bitmapData:[[screenshotImage representations] objectAtIndex:0] fileType:fileType];
	if (!fileSaved)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSSTRING_ERROR_TITLE_LEGACY message:NSSTRING_ERROR_SCREENSHOT_FAILED_LEGACY];
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
	[newFullScreenWindow setInitialFirstResponder:view];
	[view setFrame:screenRect];
	[[newFullScreenWindow contentView] addSubview:view];
	[newFullScreenWindow setDelegate:self];
	
	// If the target screen is the main screen (index 0), then autohide the menu bar and dock.
	if (targetScreen == [[NSScreen screens] objectAtIndex:0])
	{
		SetSystemUIMode(kUIModeAllHidden, kUIOptionAutoShowMenuBar);
	}
	
	// Show the full screen window.
	[self setWindow:newFullScreenWindow];
	[newFullScreenWindow makeKeyAndOrderFront:self];
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
	[self resizeWithTransform:[self normalSize] scalar:[self displayScale] rotation:[self displayRotation]];
	
	NSRect viewFrame = [[masterWindow contentView] frame];
	viewFrame.size.height -= _statusBarHeight;
	viewFrame.origin.y = _statusBarHeight;
	[view setFrame:viewFrame];
	[[masterWindow contentView] addSubview:view];
	[masterWindow makeKeyAndOrderFront:self];
	[masterWindow display];
}

- (void) respondToScreenChange:(NSNotification *)aNotification
{
	// This method only applies for displays in full screen mode. For displays in
	// windowed mode, we don't need to do anything.
	if ([self assignedScreen] == nil)
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

#pragma mark IBActions

- (IBAction) copy:(id)sender
{
	[CocoaDSUtil messageSendOneWay:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_COPY_TO_PASTEBOARD];
}

- (IBAction) changeVolume:(id)sender
{
	[emuControl changeVolume:sender];
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
		CGSize transformedMinSize = GetTransformedBounds(_minDisplayViewSize.width, _minDisplayViewSize.height, 1.0, CLOCKWISE_DEGREES([self displayRotation]));
		transformedMinSize.height += _statusBarHeight;
		
		// Resize the window if it's smaller than the minimum content size.
		NSRect windowContentRect = [masterWindow contentRectForFrameRect:[masterWindow frame]];
		if (windowContentRect.size.width < transformedMinSize.width || windowContentRect.size.height < transformedMinSize.height)
		{
			// Prepare to resize.
			NSRect oldFrameRect = [masterWindow frame];
			windowContentRect.size = NSMakeSize(transformedMinSize.width, transformedMinSize.height);
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
	if ([self assignedScreen] == nil)
	{
		[self enterFullScreen];
	}
	else
	{
		[self exitFullScreen];
	}
}

- (IBAction) toggleExecutePause:(id)sender
{
	[emuControl toggleExecutePause:sender];
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

- (IBAction) toggleBilinearFilteredOutput:(id)sender
{
	[self setUseBilinearOutput:([self useBilinearOutput]) ? NO : YES];
}

- (IBAction) toggleVerticalSync:(id)sender
{
	[self setUseVerticalSync:([self useVerticalSync]) ? NO : YES];
}

- (IBAction) changeVideoFilter:(id)sender
{
	[self setVideoFilterType:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) writeDefaultsDisplayRotation:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setDouble:[self displayRotation] forKey:@"DisplayView_Rotation"];
}

- (IBAction) writeDefaultsDisplayGap:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setDouble:([self displayGap] * 100.0) forKey:@"DisplayViewCombo_Gap"];
}

- (IBAction) writeDefaultsHUDSettings:(id)sender
{
	// TODO: Not implemented.
}

- (IBAction) writeDefaultsDisplayVideoSettings:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setInteger:[self videoFilterType] forKey:@"DisplayView_VideoFilter"];
	[[NSUserDefaults standardUserDefaults] setBool:[self useBilinearOutput] forKey:@"DisplayView_UseBilinearOutput"];
	[[NSUserDefaults standardUserDefaults] setBool:[self useVerticalSync] forKey:@"DisplayView_UseVerticalSync"];
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
	else if (theAction == @selector(toggleBilinearFilteredOutput:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self useBilinearOutput]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleVerticalSync:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self useVerticalSync]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(changeVideoFilter:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self videoFilterType] == [theItem tag]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(toggleStatusBar:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setTitle:([self isShowingStatusBar]) ? NSSTRING_TITLE_HIDE_STATUS_BAR : NSSTRING_TITLE_SHOW_STATUS_BAR];
		}
		
		if ([self assignedScreen] != nil)
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleFullScreenDisplay:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setTitle:([self assignedScreen] != nil) ? NSSTRING_TITLE_EXIT_FULL_SCREEN : NSSTRING_TITLE_ENTER_FULL_SCREEN];
		}
	}
	else if (theAction == @selector(toggleKeepMinDisplaySizeAtNormal:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setState:([self isMinSizeNormal]) ? NSOnState : NSOffState];
		}
		
		if ([self assignedScreen] != nil)
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
	[view setInputManager:[emuControl inputManager]];
	[[emuControl windowList] addObject:self];
	[emuControl updateAllWindowTitles];
	
	// Set up the video output thread.
	cdsVideoOutput = [[CocoaDSDisplayVideo alloc] init];
	[cdsVideoOutput setDelegate:view];
	
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:cdsVideoOutput withObject:nil];
	while ([cdsVideoOutput thread] == nil)
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
	
	// Setup default values per user preferences.
	[self setupUserDefaults];
	
	// Set the video filter source size now since the proper size is needed on initialization.
	// If we don't do this, new windows could draw incorrectly.
	[CocoaDSUtil messageSendOneWayWithInteger:[cdsVideoOutput receivePort] msgID:MESSAGE_CHANGE_VIDEO_FILTER integerValue:[self videoFilterType]];
	
	// Add the video thread to the output list.
	[emuControl addOutputToCore:cdsVideoOutput];
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
	[emuControl setMainWindow:self];
	[view setNextResponder:[self window]];
	[[view inputManager] setHidInputTarget:view];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
	if ([self assignedScreen] != nil)
	{
		return frameSize;
	}
	
	_isWindowResizing = YES;
	
	// Get a content Rect so that we can make our comparison.
	// This will be based on the proposed frameSize.
	const NSRect frameRect = NSMakeRect(0.0f, 0.0f, frameSize.width, frameSize.height);
	const NSRect contentRect = [sender contentRectForFrameRect:frameRect];
	
	// Find the maximum scalar we can use for the display view, bounded by the
	// content Rect.
	const NSSize normalBounds = [self normalSize];
	const CGSize checkSize = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, [self displayRotation]);
	const NSSize contentBounds = NSMakeSize(contentRect.size.width, contentRect.size.height - _statusBarHeight);
	const double maxS = GetMaxScalarInBounds(checkSize.width, checkSize.height, contentBounds.width, contentBounds.height);
	
	// Make a new content Rect with our max scalar, and convert it back to a frame Rect.
	const NSRect finalContentRect = NSMakeRect(0.0f, 0.0f, checkSize.width * maxS, (checkSize.height * maxS) + _statusBarHeight);
	const NSRect finalFrameRect = [sender frameRectForContentRect:finalContentRect];
	
	// Set the final size based on our new frame Rect.
	return finalFrameRect.size;
}

- (void)windowDidResize:(NSNotification *)notification
{
	if ([self assignedScreen] != nil)
	{
		return;
	}
	
	_isWindowResizing = YES;
	
	// Get the max scalar within the window's current content bounds.
	const NSSize normalBounds = [self normalSize];
	const CGSize checkSize = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, [self displayRotation]);
	NSSize contentBounds = [[[self window] contentView] bounds].size;
	contentBounds.height -= _statusBarHeight;
	const double maxS = GetMaxScalarInBounds(checkSize.width, checkSize.height, contentBounds.width, contentBounds.height);
	
	// Set the display view's properties.
	[self setDisplayScale:maxS];
	
	// Resize the view.
	NSRect newContentFrame = [[[self window] contentView] bounds];
	newContentFrame.origin.y = _statusBarHeight;
	newContentFrame.size.height -= _statusBarHeight;
	[view setFrame:newContentFrame];
	
	_isWindowResizing = NO;
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
}

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
		
		if ([emuControl executionState] == CORESTATE_PAUSE)
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
		
		if (speedScalar == (NSInteger)(SPEED_SCALAR_DOUBLE * 100.0))
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
	
	return enable;
}

@end

#pragma mark -
@implementation DisplayView

@synthesize inputManager;

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self == nil)
	{
		return self;
	}
	
	inputManager = nil;
	oglv = new OGLVideoOutput();
	
	// Initialize the OpenGL context
	NSOpenGLPixelFormatAttribute attributes[] = {
		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)0,
		NSOpenGLPFAStencilSize, (NSOpenGLPixelFormatAttribute)0,
		NSOpenGLPFADoubleBuffer,
		(NSOpenGLPixelFormatAttribute)0 };
	
	NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
	[format release];
	cglDisplayContext = (CGLContextObj)[context CGLContextObj];
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglv->InitializeOGL();
	CGLSetCurrentContext(prevContext);
	
	return self;
}

- (void)dealloc
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	oglv->TerminateOGL();
	CGLSetCurrentContext(prevContext);
	
	[self setInputManager:nil];
	[context clearDrawable];
	[context release];
	
	delete oglv;
	
	[super dealloc];
}

#pragma mark Class Methods

- (void) drawVideoFrame
{
	oglv->RenderOGL();
	CGLFlushDrawable(cglDisplayContext);
}

- (NSPoint) dsPointFromEvent:(NSEvent *)theEvent
{
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	NSPoint touchLoc = [theEvent locationInWindow];
	touchLoc = [self convertPoint:touchLoc fromView:nil];
	touchLoc = [self convertPointToDS:touchLoc];
	
	return touchLoc;
}

- (NSPoint) convertPointToDS:(NSPoint)clickLoc
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	
	double viewAngle = [windowController displayRotation];
	if (viewAngle != 0.0)
	{
		viewAngle = CLOCKWISE_DEGREES(viewAngle);
	}
	
	const NSSize normalBounds = [windowController normalSize];
	const NSSize viewSize = [self bounds].size;
	const CGSize transformBounds = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, _displayRotation);
	const double s = GetMaxScalarInBounds(transformBounds.width, transformBounds.height, viewSize.width, viewSize.height);
	
	CGPoint touchLoc = GetNormalPointFromTransformedPoint(clickLoc.x, clickLoc.y,
														  normalBounds.width, normalBounds.height,
														  viewSize.width, viewSize.height,
														  s,
														  viewAngle);
	
	// Normalize the touch location to the DS.
	if ([windowController displayMode] == DS_DISPLAY_TYPE_DUAL)
	{
		const NSInteger theOrientation = [windowController displayOrientation];
		const NSInteger theOrder = [windowController displayOrder];
		const double gap = DS_DISPLAY_GAP * [windowController displayGap];
		
		if (theOrientation == DS_DISPLAY_ORIENTATION_VERTICAL && theOrder == DS_DISPLAY_ORDER_TOUCH_FIRST)
		{
			touchLoc.y -= (GPU_DISPLAY_HEIGHT+gap);
		}
		else if (theOrientation == DS_DISPLAY_ORIENTATION_HORIZONTAL && theOrder == DS_DISPLAY_ORDER_MAIN_FIRST)
		{
			touchLoc.x -= (GPU_DISPLAY_WIDTH+gap);
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
	
	return NSMakePoint(touchLoc.x, touchLoc.y);
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
	const NSInteger displayModeID = [windowController displayMode];
	
	// Convert the clicked location from window coordinates, to view coordinates,
	// and finally to DS touchscreen coordinates.
	const NSPoint touchLoc = (displayModeID == DS_DISPLAY_TYPE_MAIN) ? NSMakePoint(0.0, 0.0) : [self dsPointFromEvent:theEvent];
	const InputAttributes inputAttr = InputManagerEncodeMouseButtonInput([theEvent buttonNumber], touchLoc, buttonPressed);
	
	if (buttonPressed && [theEvent window] != nil)
	{
		NSString *newStatusText = (displayModeID == DS_DISPLAY_TYPE_MAIN) ? [NSString stringWithFormat:@"%s:%s", inputAttr.deviceName, inputAttr.elementName] : [NSString stringWithFormat:@"%s:%s X:%i Y:%i", inputAttr.deviceName, inputAttr.elementName, (int)inputAttr.intCoordX, (int)inputAttr.intCoordY];
		[[windowController emuControl] setStatusText:newStatusText];
	}
	
	isHandled = [inputManager dispatchCommandUsingInputAttributes:&inputAttr];
	return isHandled;
}

- (void) clearToBlack
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	[CocoaDSUtil messageSendOneWay:[[windowController cdsVideoOutput] receivePort] msgID:MESSAGE_SET_VIEW_TO_BLACK];
}

- (void) clearToWhite
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	[CocoaDSUtil messageSendOneWay:[[windowController cdsVideoOutput] receivePort] msgID:MESSAGE_SET_VIEW_TO_WHITE];
}

- (void) requestScreenshot:(NSURL *)fileURL fileType:(NSBitmapImageFileType)fileType
{
	NSString *fileURLString = [fileURL absoluteString];
	NSData *fileURLStringData = [fileURLString dataUsingEncoding:NSUTF8StringEncoding];
	NSData *bitmapImageFileTypeData = [[NSData alloc] initWithBytes:&fileType length:sizeof(NSBitmapImageFileType)];
	NSArray *messageComponents = [[NSArray alloc] initWithObjects:fileURLStringData, bitmapImageFileTypeData, nil];
	
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	[CocoaDSUtil messageSendOneWayWithMessageComponents:[[windowController cdsVideoOutput] receivePort] msgID:MESSAGE_REQUEST_SCREENSHOT array:messageComponents];
	
	[bitmapImageFileTypeData release];
	[messageComponents release];
}

#pragma mark NSView Methods

- (BOOL)isOpaque
{
	return YES;
}

- (void)lockFocus
{
	[super lockFocus];
	
	if ([context view] != self)
	{
		[context setView:self];
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	[CocoaDSUtil messageSendOneWay:[[windowController cdsVideoOutput] receivePort] msgID:MESSAGE_REDRAW_VIEW];
}

- (void)setFrame:(NSRect)rect
{
	NSRect oldFrame = [self frame];
	[super setFrame:rect];
	
	if (rect.size.width != oldFrame.size.width || rect.size.height != oldFrame.size.height)
	{
		[context update];
		DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
		[CocoaDSUtil messageSendOneWayWithRect:[[windowController cdsVideoOutput] receivePort] msgID:MESSAGE_RESIZE_VIEW rect:rect];
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

#pragma mark CocoaDSDisplayVideoDelegate Protocol

- (void)doInitVideoOutput:(NSDictionary *)properties
{
	// No init needed, so do nothing.
}

- (void)doProcessVideoFrame:(const void *)videoFrameData displayMode:(const NSInteger)frameDisplayMode width:(const NSInteger)frameWidth height:(const NSInteger)frameHeight
{
	const bool didDisplayModeChange = (oglv->GetDisplayMode() != frameDisplayMode);
	if (didDisplayModeChange)
	{
		oglv->SetDisplayMode(frameDisplayMode);
	}
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	if (didDisplayModeChange)
	{
		oglv->UploadVerticesOGL();
	}
	
	oglv->PrerenderOGL(videoFrameData, frameWidth, frameHeight);
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doResizeView:(NSRect)rect
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	oglv->SetViewportSizeOGL(rect.size.width, rect.size.height);
	[self drawVideoFrame];
	CGLUnlockContext(cglDisplayContext);
}

- (void)doTransformView:(const DisplayOutputTransformData *)transformData
{
	_displayRotation = (GLfloat)transformData->rotation;
	oglv->SetRotation((GLfloat)transformData->rotation);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	oglv->UpdateDisplayTransformationOGL();
	[self drawVideoFrame];
	CGLUnlockContext(cglDisplayContext);
}

- (void)doRedraw
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	[self drawVideoFrame];
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayModeChanged:(NSInteger)displayModeID
{
	oglv->SetDisplayMode(displayModeID);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	oglv->UpdateDisplayTransformationOGL();
	oglv->UploadVerticesOGL();
	[self drawVideoFrame];
	CGLUnlockContext(cglDisplayContext);
}

- (void)doBilinearOutputChanged:(BOOL)useBilinear
{
	const bool c99_useBilinear = (useBilinear) ? true : false;
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	oglv->SetDisplayBilinearOGL(c99_useBilinear);
	[self drawVideoFrame];
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayOrientationChanged:(NSInteger)displayOrientationID
{
	oglv->SetDisplayOrientation(displayOrientationID);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	oglv->UploadVerticesOGL();
	
	if (oglv->GetDisplayMode() == DS_DISPLAY_TYPE_DUAL)
	{
		oglv->UpdateDisplayTransformationOGL();
		[self drawVideoFrame];
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayOrderChanged:(NSInteger)displayOrderID
{
	oglv->SetDisplayOrder(displayOrderID);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	oglv->UploadVerticesOGL();
	if (oglv->GetDisplayMode() == DS_DISPLAY_TYPE_DUAL)
	{
		[self drawVideoFrame];
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayGapChanged:(float)displayGapScalar
{
	oglv->SetGapScalar((GLfloat)displayGapScalar);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	oglv->UploadVerticesOGL();
	
	if (oglv->GetDisplayMode() == DS_DISPLAY_TYPE_DUAL)
	{
		oglv->UpdateDisplayTransformationOGL();
		[self drawVideoFrame];
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doVerticalSyncChanged:(BOOL)useVerticalSync
{
	const GLint swapInt = (useVerticalSync) ? 1 : 0;
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	CGLSetParameter(cglDisplayContext, kCGLCPSwapInterval, &swapInt);
	CGLUnlockContext(cglDisplayContext);
}

- (void)doVideoFilterChanged:(NSInteger)videoFilterTypeID
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	oglv->SetVideoFilterOGL((const VideoFilterTypeID)videoFilterTypeID);
	CGLUnlockContext(cglDisplayContext);
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
