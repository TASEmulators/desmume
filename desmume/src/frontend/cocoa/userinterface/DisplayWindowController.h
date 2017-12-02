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

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include <map>

#import "DisplayViewCALayer.h"

#include "../ClientDisplayView.h"
#undef BOOL

#define DISPLAY_VIDEO_SOURCE_MAIN_TAG_BASE		1000
#define DISPLAY_VIDEO_SOURCE_TOUCH_TAG_BASE		2000

@class CocoaDSController;
@class CocoaDSDisplayVideo;
@class EmuControllerDelegate;
@class InputManager;
class OGLVideoOutput;

// Subclass NSWindow for full screen windows so that we can override some methods.
@interface DisplayFullScreenWindow : NSWindow
{ }
@end

@interface DisplayView : NSView<CocoaDisplayViewProtocol>
{
	InputManager *inputManager;
	CocoaDSDisplayVideo *cdsVideoOutput;
	CALayer<DisplayViewCALayer> *localLayer;
	NSOpenGLContext *localOGLContext;
}

- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed;

@end

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface DisplayWindowController : NSWindowController <NSWindowDelegate>
#else
@interface DisplayWindowController : NSWindowController
#endif
{
	NSObject *dummyObject;
	
	ClientDisplayPresenterProperties _localViewProps;
	NSView *outputVolumeControlView;
	NSView *microphoneGainControlView;
	NSMenuItem *outputVolumeMenuItem;
	NSMenuItem *microphoneGainMenuItem;
	NSSlider *microphoneGainSlider;
	NSButton *microphoneMuteButton;
	
	NSView<CocoaDisplayViewProtocol> *view;
	EmuControllerDelegate *emuControl;
	NSScreen *assignedScreen;
	NSWindow *masterWindow;
	
	NSSize _minDisplayViewSize;
	BOOL _isMinSizeNormal;
	NSUInteger _statusBarHeight;
	BOOL _isUpdatingDisplayScaleValueOnly;
	BOOL _canUseMavericksFullScreen;
	BOOL _masterStatusBarState;
	NSRect _masterWindowFrame;
	double _masterWindowScale;
	double _localViewScale;
	double _localRotation;
}

@property (readonly) IBOutlet NSObject *dummyObject;

@property (readonly) IBOutlet NSView *outputVolumeControlView;
@property (readonly) IBOutlet NSView *microphoneGainControlView;
@property (readonly) IBOutlet NSMenuItem *outputVolumeMenuItem;
@property (readonly) IBOutlet NSMenuItem *microphoneGainMenuItem;
@property (readonly) IBOutlet NSSlider *microphoneGainSlider;
@property (readonly) IBOutlet NSButton *microphoneMuteButton;

@property (retain) NSView<CocoaDisplayViewProtocol> *view;
@property (retain) EmuControllerDelegate *emuControl;
@property (assign) NSScreen *assignedScreen;
@property (retain) NSWindow *masterWindow;

@property (readonly, nonatomic) BOOL isFullScreen;
@property (assign, nonatomic) NSInteger displayMode;
@property (assign, nonatomic) NSInteger displayOrientation;
@property (assign, nonatomic) NSInteger displayOrder;
@property (assign, nonatomic) double displayGap;
@property (assign, nonatomic) double displayScale;
@property (assign, nonatomic) double displayRotation;
@property (assign) BOOL isMinSizeNormal;
@property (assign) BOOL isShowingStatusBar;

- (id)initWithWindowNibName:(NSString *)windowNibName emuControlDelegate:(EmuControllerDelegate *)theEmuController;

- (ClientDisplayPresenterProperties &) localViewProperties;
- (void) setVideoPropertiesWithoutUpdateUsingPreferGPU:(BOOL)preferGPU
									 sourceDeposterize:(BOOL)useDeposterize
										  outputFilter:(NSInteger)outputFilterID
										   pixelScaler:(NSInteger)pixelScalerID;
- (void) setDisplayMode:(ClientDisplayMode)mode
			  viewScale:(double)viewScale
			   rotation:(double)rotation
				 layout:(ClientDisplayLayout)layout
				  order:(ClientDisplayOrder)order
			   gapScale:(double)gapScale
		isMinSizeNormal:(BOOL)isMinSizeNormal
	 isShowingStatusBar:(BOOL)isShowingStatusBar;

- (void) setupUserDefaults;
- (BOOL) masterStatusBarState;
- (NSRect) masterWindowFrame;
- (double) masterWindowScale;
- (NSRect) updateViewProperties;
- (void) resizeWithTransform;
- (double) maxViewScaleInHostScreen:(double)contentBoundsWidth height:(double)contentBoundsHeight;
- (void) enterFullScreen;
- (void) exitFullScreen;
- (void) updateDisplayID;

- (IBAction) copy:(id)sender;
- (IBAction) changeHardwareMicGain:(id)sender;
- (IBAction) changeHardwareMicMute:(id)sender;
- (IBAction) changeVolume:(id)sender;

- (IBAction) toggleExecutePause:(id)sender;
- (IBAction) frameAdvance:(id)sender;
- (IBAction) reset:(id)sender;
- (IBAction) changeCoreSpeed:(id)sender;
- (IBAction) openRom:(id)sender;

// View Menu
- (IBAction) changeScale:(id)sender;
- (IBAction) changeRotation:(id)sender;
- (IBAction) changeRotationRelative:(id)sender;
- (IBAction) changeDisplayMode:(id)sender;
- (IBAction) changeDisplayOrientation:(id)sender;
- (IBAction) changeDisplayOrder:(id)sender;
- (IBAction) changeDisplayGap:(id)sender;
- (IBAction) changeDisplayVideoSource:(id)sender;
- (IBAction) toggleVideoFiltersPreferGPU:(id)sender;
- (IBAction) toggleVideoSourceDeposterize:(id)sender;
- (IBAction) changeVideoOutputFilter:(id)sender;
- (IBAction) changeVideoPixelScaler:(id)sender;
- (IBAction) toggleHUDVisibility:(id)sender;
- (IBAction) toggleShowHUDExecutionSpeed:(id)sender;
- (IBAction) toggleShowHUDVideoFPS:(id)sender;
- (IBAction) toggleShowHUDRender3DFPS:(id)sender;
- (IBAction) toggleShowHUDFrameIndex:(id)sender;
- (IBAction) toggleShowHUDLagFrameCount:(id)sender;
- (IBAction) toggleShowHUDCPULoadAverage:(id)sender;
- (IBAction) toggleShowHUDRealTimeClock:(id)sender;
- (IBAction) toggleShowHUDInput:(id)sender;
- (IBAction) toggleNDSDisplays:(id)sender;
- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender;
- (IBAction) toggleStatusBar:(id)sender;
- (IBAction) toggleFullScreenDisplay:(id)sender;

- (IBAction) writeDefaultsDisplayRotation:(id)sender;
- (IBAction) writeDefaultsDisplayGap:(id)sender;
- (IBAction) writeDefaultsHUDSettings:(id)sender;
- (IBAction) writeDefaultsDisplayVideoSettings:(id)sender;

@end
