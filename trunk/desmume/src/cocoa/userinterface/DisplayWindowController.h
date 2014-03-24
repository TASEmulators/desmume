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

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include <libkern/OSAtomic.h>

#import "InputManager.h"
#import "cocoa_output.h"

@class CocoaDSController;
@class EmuControllerDelegate;
class OGLVideoOutput;


// Subclass NSWindow for full screen windows so that we can override some methods.
@interface DisplayFullScreenWindow : NSWindow
{ }
@end

@interface DisplayView : NSView <CocoaDSDisplayVideoDelegate, InputHIDManagerTarget>
{
	InputManager *inputManager;
	OGLVideoOutput *oglv;
	CGFloat _displayRotation;
	BOOL canUseShaderBasedFilters;
	
	BOOL _useVerticalSync;
	
	OSSpinLock spinlockUseVerticalSync;
	OSSpinLock spinlockVideoFiltersPreferGPU;
	OSSpinLock spinlockOutputFilter;
	OSSpinLock spinlockSourceDeposterize;
	OSSpinLock spinlockPixelScaler;
	
	// OpenGL context
	NSOpenGLContext *context;
	CGLContextObj cglDisplayContext;
}

@property (retain) InputManager *inputManager;
@property (readonly) BOOL canUseShaderBasedFilters;
@property (assign) BOOL useVerticalSync;
@property (assign) BOOL videoFiltersPreferGPU;
@property (assign) BOOL sourceDeposterize;
@property (assign) NSInteger outputFilter;
@property (assign) NSInteger pixelScaler;

- (void) drawVideoFrame;
- (NSPoint) dsPointFromEvent:(NSEvent *)theEvent;
- (NSPoint) convertPointToDS:(NSPoint)clickLoc;
- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed;
- (void) clearToBlack;
- (void) clearToWhite;
- (void) requestScreenshot:(NSURL *)fileURL fileType:(NSBitmapImageFileType)fileType;

@end

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface DisplayWindowController : NSWindowController <NSWindowDelegate>
#else
@interface DisplayWindowController : NSWindowController
#endif
{
	NSObject *dummyObject;
	
	DisplayView *view;
	NSView *saveScreenshotPanelAccessoryView;
	
	EmuControllerDelegate *emuControl;
	CocoaDSDisplayVideo *cdsVideoOutput;
	NSScreen *assignedScreen;
	NSWindow *masterWindow;
	
	NSSize _normalSize;
	double _displayScale;
	double _displayRotation;
	NSInteger _displayMode;
	NSInteger _displayOrientation;
	NSInteger _displayOrder;
	double _displayGap;
	NSInteger screenshotFileFormat;
	
	NSSize _minDisplayViewSize;
	BOOL _isMinSizeNormal;
	NSUInteger _statusBarHeight;
	BOOL _isWindowResizing;
	
	OSSpinLock spinlockNormalSize;
	OSSpinLock spinlockScale;
	OSSpinLock spinlockRotation;
	OSSpinLock spinlockDisplayMode;
	OSSpinLock spinlockDisplayOrientation;
	OSSpinLock spinlockDisplayOrder;
	OSSpinLock spinlockDisplayGap;
}

@property (readonly) IBOutlet NSObject *dummyObject;

@property (readonly) IBOutlet DisplayView *view;
@property (readonly) IBOutlet NSView *saveScreenshotPanelAccessoryView;

@property (retain) EmuControllerDelegate *emuControl;
@property (assign) CocoaDSDisplayVideo *cdsVideoOutput;
@property (assign) NSScreen *assignedScreen;
@property (retain) NSWindow *masterWindow;

@property (readonly) NSSize normalSize;
@property (assign) double displayScale;
@property (assign) double displayRotation;
@property (assign) BOOL videoFiltersPreferGPU;
@property (assign) BOOL videoSourceDeposterize;
@property (assign) NSInteger videoOutputFilter;
@property (assign) NSInteger videoPixelScaler;
@property (assign) NSInteger displayMode;
@property (assign) NSInteger displayOrientation;
@property (assign) NSInteger displayOrder;
@property (assign) double displayGap;
@property (assign) NSInteger screenshotFileFormat;
@property (assign) BOOL isMinSizeNormal;
@property (assign) BOOL isShowingStatusBar;

- (id)initWithWindowNibName:(NSString *)windowNibName emuControlDelegate:(EmuControllerDelegate *)theEmuController;

- (void) setupUserDefaults;
- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees;
- (double) maxScalarForContentBoundsWidth:(double)contentBoundsWidth height:(double)contentBoundsHeight;
- (void) enterFullScreen;
- (void) exitFullScreen;

- (IBAction) copy:(id)sender;
- (IBAction) changeVolume:(id)sender;
- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender;
- (IBAction) toggleStatusBar:(id)sender;
- (IBAction) toggleFullScreenDisplay:(id)sender;

- (IBAction) toggleExecutePause:(id)sender;
- (IBAction) reset:(id)sender;
- (IBAction) changeCoreSpeed:(id)sender;
- (IBAction) openRom:(id)sender;
- (IBAction) saveScreenshotAs:(id)sender;

// View Menu
- (IBAction) changeScale:(id)sender;
- (IBAction) changeRotation:(id)sender;
- (IBAction) changeRotationRelative:(id)sender;
- (IBAction) changeDisplayMode:(id)sender;
- (IBAction) changeDisplayOrientation:(id)sender;
- (IBAction) changeDisplayOrder:(id)sender;
- (IBAction) changeDisplayGap:(id)sender;
- (IBAction) toggleVideoSourceDeposterize:(id)sender;
- (IBAction) changeVideoOutputFilter:(id)sender;
- (IBAction) changeVideoPixelScaler:(id)sender;

- (IBAction) writeDefaultsDisplayRotation:(id)sender;
- (IBAction) writeDefaultsDisplayGap:(id)sender;
- (IBAction) writeDefaultsHUDSettings:(id)sender;
- (IBAction) writeDefaultsDisplayVideoSettings:(id)sender;

@end
