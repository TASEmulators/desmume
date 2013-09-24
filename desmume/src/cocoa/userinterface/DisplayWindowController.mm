/*
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

#import "DisplayWindowController.h"
#import "EmuControllerDelegate.h"
#import "InputManager.h"

#import "cocoa_file.h"
#import "cocoa_input.h"
#import "cocoa_globals.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"

#include <Carbon/Carbon.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#if defined(__ppc__) || defined(__ppc64__)
#include <map>
#else
#include <tr1/unordered_map>
#endif

// VERTEX SHADER FOR DISPLAY OUTPUT
static const char *vertexProgram_100 = {"\
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
static const char *fragmentProgram_100 = {"\
	varying vec2 vtxTexCoord; \n\
	uniform sampler2D tex; \n\
	\n\
	void main() \n\
	{ \n\
		gl_FragColor = texture2D(tex, vtxTexCoord); \n\
	} \n\
"};

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position = 0,
	OGLVertexAttributeID_TexCoord0 = 8
};


@implementation DisplayWindowController

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
	
	[CocoaDSUtil messageSendOneWayWithBool:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_BILINEAR_OUTPUT boolValue:theState];
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
	
	[CocoaDSUtil messageSendOneWayWithBool:[[self cdsVideoOutput] receivePort] msgID:MESSAGE_CHANGE_VERTICAL_SYNC boolValue:theState];
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
	else if (theAction == @selector(hudDisable:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem *)theItem setTitle:([[self view] isHudEnabled]) ? NSSTRING_TITLE_DISABLE_HUD : NSSTRING_TITLE_ENABLE_HUD];
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
	const NSSize vfSrcSize = NSMakeSize(GPU_DISPLAY_WIDTH, ([self displayMode] == DS_DISPLAY_TYPE_DUAL) ? GPU_DISPLAY_HEIGHT * 2 : GPU_DISPLAY_HEIGHT);
	[[cdsVideoOutput vf] setSourceSize:vfSrcSize];
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
@synthesize isHudEnabled;
@synthesize isHudEditingModeEnabled;

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self == nil)
	{
		return self;
	}
	
	inputManager = nil;
	
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
	
	_currentDisplayMode = DS_DISPLAY_TYPE_DUAL;
	_currentDisplayOrientation = DS_DISPLAY_ORIENTATION_VERTICAL;
	_currentGapScalar = 0.0f;
	_currentNormalSize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*_currentGapScalar));
	glTexPixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	
	const UInt32 w = GetNearestPositivePOT((UInt32)_currentNormalSize.width);
	const UInt32 h = GetNearestPositivePOT((UInt32)_currentNormalSize.height);
	glTexBack = (GLvoid *)calloc(w * h, sizeof(UInt16));
	glTexBackSize = NSMakeSize(w, h);
	vtxBufferOffset = 0;
	
	[self updateDisplayVerticesUsingDisplayMode:_currentDisplayMode orientation:_currentDisplayOrientation gap:_currentGapScalar];
	[self updateTexCoordS:1.0f T:2.0f];
	
	// Set up initial vertex elements
	vtxIndexBuffer[0]	= 0;	vtxIndexBuffer[1]	= 1;	vtxIndexBuffer[2]	= 2;
	vtxIndexBuffer[3]	= 2;	vtxIndexBuffer[4]	= 3;	vtxIndexBuffer[5]	= 0;
	
	vtxIndexBuffer[6]	= 4;	vtxIndexBuffer[7]	= 5;	vtxIndexBuffer[8]	= 6;
	vtxIndexBuffer[9]	= 6;	vtxIndexBuffer[10]	= 7;	vtxIndexBuffer[11]	= 4;
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglDisplayContext);
	[self startupOpenGL];
	CGLSetCurrentContext(prevContext);
	
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
	
	[self setInputManager:nil];
	[context clearDrawable];
	[context release];
	
	[super dealloc];
}

#pragma mark Class Methods

- (void) startupOpenGL
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
			glUniform2f(uniformViewSize, GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*_currentGapScalar));
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
	if (_currentDisplayMode != displayModeID)
	{
		_currentDisplayMode = displayModeID;
		[self updateDisplayVerticesUsingDisplayMode:displayModeID orientation:_currentDisplayOrientation gap:_currentGapScalar];
		[self uploadVertices];
	}
	
	const GLsizei vtxElementCount = (displayModeID == DS_DISPLAY_TYPE_DUAL) ? 12 : 6;
	const GLubyte *elementPointer = !(displayModeID == DS_DISPLAY_TYPE_TOUCH) ? 0 : (GLubyte *)(vtxElementCount * sizeof(GLubyte));
	
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glDrawElements(GL_TRIANGLES, vtxElementCount, GL_UNSIGNED_BYTE, elementPointer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Disable vertex attributes
	glBindVertexArrayAPPLE(0);
}

- (void) updateDisplayVerticesUsingDisplayMode:(const NSInteger)displayModeID orientation:(const NSInteger)displayOrientationID gap:(const GLfloat)gapScalar
{
	const GLfloat w = GPU_DISPLAY_WIDTH;
	const GLfloat h = GPU_DISPLAY_HEIGHT;
	const GLfloat gap = DS_DISPLAY_GAP * gapScalar / 2.0;
	
	if (displayModeID == DS_DISPLAY_TYPE_DUAL)
	{
		// displayOrder == DS_DISPLAY_ORDER_MAIN_FIRST
		if (displayOrientationID == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			vtxBuffer[0]	= -w/2;			vtxBuffer[1]		= h+gap;	// Top display, top left
			vtxBuffer[2]	= w/2;			vtxBuffer[3]		= h+gap;	// Top display, top right
			vtxBuffer[4]	= w/2;			vtxBuffer[5]		= gap;		// Top display, bottom right
			vtxBuffer[6]	= -w/2;			vtxBuffer[7]		= gap;		// Top display, bottom left
			
			vtxBuffer[8]	= -w/2;			vtxBuffer[9]		= -gap;		// Bottom display, top left
			vtxBuffer[10]	= w/2;			vtxBuffer[11]		= -gap;		// Bottom display, top right
			vtxBuffer[12]	= w/2;			vtxBuffer[13]		= -(h+gap);	// Bottom display, bottom right
			vtxBuffer[14]	= -w/2;			vtxBuffer[15]		= -(h+gap);	// Bottom display, bottom left
		}
		else // displayOrientationID == DS_DISPLAY_ORIENTATION_HORIZONTAL
		{
			vtxBuffer[0]	= -(w+gap);		vtxBuffer[1]		= h/2;		// Left display, top left
			vtxBuffer[2]	= -gap;			vtxBuffer[3]		= h/2;		// Left display, top right
			vtxBuffer[4]	= -gap;			vtxBuffer[5]		= -h/2;		// Left display, bottom right
			vtxBuffer[6]	= -(w+gap);		vtxBuffer[7]		= -h/2;		// Left display, bottom left
			
			vtxBuffer[8]	= gap;			vtxBuffer[9]		= h/2;		// Right display, top left
			vtxBuffer[10]	= w+gap;		vtxBuffer[11]		= h/2;		// Right display, top right
			vtxBuffer[12]	= w+gap;		vtxBuffer[13]		= -h/2;		// Right display, bottom right
			vtxBuffer[14]	= gap;			vtxBuffer[15]		= -h/2;		// Right display, bottom left
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
	const CGSize transformBounds = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, _currentRotation);
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
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue
{
	BOOL isHandled = NO;
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	
	InputAttributesList inputList = InputManagerEncodeHIDQueue(hidQueue);
	char inputStr[INPUT_HANDLER_STRING_LENGTH*2];
	memset(inputStr, '\0', INPUT_HANDLER_STRING_LENGTH*2);
	
	const size_t inputCount = inputList.size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = inputList[i];
		
		if (inputAttr.state == INPUT_ATTRIBUTE_STATE_ON)
		{
			strlcpy(inputStr, inputAttr.deviceName, INPUT_HANDLER_STRING_LENGTH*2);
			strlcat(inputStr, ":", INPUT_HANDLER_STRING_LENGTH*2);
			strlcat(inputStr, inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH*2);
			break;
		}
	}
	
	if (inputStr[0] != '\0' && inputStr[0] != ':')
	{
		[[windowController emuControl] setStatusText:[NSString stringWithCString:inputStr encoding:NSUTF8StringEncoding]];
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
		char inputStr[INPUT_HANDLER_STRING_LENGTH*2];
		strlcpy(inputStr, inputAttr.deviceName, INPUT_HANDLER_STRING_LENGTH*2);
		strlcat(inputStr, ":", INPUT_HANDLER_STRING_LENGTH*2);
		strlcat(inputStr, inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH*2);
		
		[[windowController emuControl] setStatusText:[NSString stringWithCString:inputStr encoding:NSUTF8StringEncoding]];
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
	NSPoint touchLoc = NSMakePoint(-2.0, -2.0);
	
	if (displayModeID == DS_DISPLAY_TYPE_TOUCH || displayModeID == DS_DISPLAY_TYPE_DUAL)
	{
		touchLoc = [self dsPointFromEvent:theEvent];
	}
	
	const InputAttributes inputAttr = InputManagerEncodeMouseButtonInput([theEvent buttonNumber], touchLoc, buttonPressed);
	
	if (buttonPressed && [theEvent window] != nil)
	{
		static char inputStr[INPUT_HANDLER_STRING_LENGTH*2] = {0};
		strlcpy(inputStr, inputAttr.deviceName, INPUT_HANDLER_STRING_LENGTH*2);
		strlcat(inputStr, ":", INPUT_HANDLER_STRING_LENGTH*2);
		strlcat(inputStr, inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH*2);
		
		if (inputAttr.intCoordX >= 0)
		{
			static char inputCoordBuf[64] = {0};
			snprintf(inputCoordBuf, 64, " X:%i Y:%i", (int)inputAttr.intCoordX, (int)inputAttr.intCoordY);
			strlcat(inputStr, inputCoordBuf, INPUT_HANDLER_STRING_LENGTH*2);
		}
		
		[[windowController emuControl] setStatusText:[NSString stringWithCString:inputStr encoding:NSUTF8StringEncoding]];
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
	const NSSize viewSize = [self frame].size;
	const CGSize checkSize = GetTransformedBounds(_currentNormalSize.width, _currentNormalSize.height, 1.0, _currentRotation);
	const double s = GetMaxScalarInBounds(checkSize.width, checkSize.height, viewSize.width, viewSize.height);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glViewport(0, 0, rect.size.width, rect.size.height);
	
	if (isShaderSupported)
	{
		glUniform2f(uniformViewSize, rect.size.width, rect.size.height);
		glUniform1f(uniformScalar, s);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-rect.size.width/2, -rect.size.width/2 + rect.size.width, -rect.size.height/2, -rect.size.height/2 + rect.size.height, -1.0, 1.0);
		glRotatef(CLOCKWISE_DEGREES(_currentRotation), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
	
	[self renderDisplayUsingDisplayMode:_currentDisplayMode];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doTransformView:(const DisplayOutputTransformData *)transformData
{
	_currentRotation = (GLfloat)transformData->rotation;
	
	const NSSize viewSize = [self bounds].size;
	const CGSize checkSize = GetTransformedBounds(_currentNormalSize.width, _currentNormalSize.height, 1.0, _currentRotation);
	const double s = GetMaxScalarInBounds(checkSize.width, checkSize.height, viewSize.width, viewSize.height);
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	if (isShaderSupported)
	{
		glUniform1f(uniformAngleDegrees, _currentRotation);
		glUniform1f(uniformScalar, s);
	}
	else
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(CLOCKWISE_DEGREES(_currentRotation), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
	
	[self renderDisplayUsingDisplayMode:_currentDisplayMode];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doRedraw
{
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self renderDisplayUsingDisplayMode:_currentDisplayMode];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayModeChanged:(NSInteger)displayModeID
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	_currentNormalSize = [windowController normalSize];
	
	const NSSize viewSize = [self bounds].size;
	const CGSize checkSize = GetTransformedBounds(_currentNormalSize.width, _currentNormalSize.height, 1.0, _currentRotation);
	const double s = GetMaxScalarInBounds(checkSize.width, checkSize.height, viewSize.width, viewSize.height);
	
	_currentDisplayMode = displayModeID;
	[self updateDisplayVerticesUsingDisplayMode:displayModeID orientation:_currentDisplayOrientation gap:_currentGapScalar];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	if (isShaderSupported)
	{
		glUniform1f(uniformScalar, s);
	}
	else
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(CLOCKWISE_DEGREES(_currentRotation), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
	
	[self uploadVertices];
	[self renderDisplayUsingDisplayMode:_currentDisplayMode];
	[self drawVideoFrame];
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doBilinearOutputChanged:(BOOL)useBilinear
{
	const GLint textureFilter = (useBilinear) ? GL_LINEAR : GL_NEAREST;
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	glBindTexture(GL_TEXTURE_2D, displayTexID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureFilter);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayOrientationChanged:(NSInteger)displayOrientationID
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	_currentNormalSize = [windowController normalSize];
	
	_currentDisplayOrientation = displayOrientationID;
	[self updateDisplayVerticesUsingDisplayMode:_currentDisplayMode orientation:displayOrientationID gap:_currentGapScalar];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self uploadVertices];
	
	if (_currentDisplayMode == DS_DISPLAY_TYPE_DUAL)
	{
		const NSSize viewSize = [self bounds].size;
		const CGSize checkSize = GetTransformedBounds(_currentNormalSize.width, _currentNormalSize.height, 1.0, _currentRotation);
		const double s = GetMaxScalarInBounds(checkSize.width, checkSize.height, viewSize.width, viewSize.height);
		
		if (isShaderSupported)
		{
			glUniform1f(uniformScalar, s);
		}
		else
		{
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glRotatef(CLOCKWISE_DEGREES(_currentRotation), 0.0f, 0.0f, 1.0f);
			glScalef(s, s, 1.0f);
		}
		
		[self renderDisplayUsingDisplayMode:_currentDisplayMode];
		[self drawVideoFrame];
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayOrderChanged:(NSInteger)displayOrderID
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
	
	if (_currentDisplayMode == DS_DISPLAY_TYPE_DUAL)
	{
		[self renderDisplayUsingDisplayMode:_currentDisplayMode];
		[self drawVideoFrame];
	}
	
	CGLUnlockContext(cglDisplayContext);
}

- (void)doDisplayGapChanged:(float)displayGapScalar
{
	DisplayWindowController *windowController = (DisplayWindowController *)[[self window] delegate];
	_currentNormalSize = [windowController normalSize];
	
	_currentGapScalar = (GLfloat)displayGapScalar;
	[self updateDisplayVerticesUsingDisplayMode:_currentDisplayMode orientation:_currentDisplayOrientation gap:(GLfloat)displayGapScalar];
	
	CGLLockContext(cglDisplayContext);
	CGLSetCurrentContext(cglDisplayContext);
	
	[self uploadVertices];
	
	if (_currentDisplayMode == DS_DISPLAY_TYPE_DUAL)
	{
		const NSSize viewSize = [self bounds].size;
		const CGSize checkSize = GetTransformedBounds(_currentNormalSize.width, _currentNormalSize.height, 1.0, _currentRotation);
		const double s = GetMaxScalarInBounds(checkSize.width, checkSize.height, viewSize.width, viewSize.height);
		
		if (isShaderSupported)
		{
			glUniform1f(uniformScalar, s);
		}
		else
		{
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glRotatef(CLOCKWISE_DEGREES(_currentRotation), 0.0f, 0.0f, 1.0f);
			glScalef(s, s, 1.0f);
		}
		
		[self renderDisplayUsingDisplayMode:_currentDisplayMode];
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
	
	if (_currentDisplayMode != DS_DISPLAY_TYPE_DUAL)
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
