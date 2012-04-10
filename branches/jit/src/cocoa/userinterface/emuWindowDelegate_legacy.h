/*
	Copyright (C) 2007 Jeff Bland
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

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif


@class NintendoDS;
@class VideoOutputView;
@class InputHandler;
@compatibility_alias CocoaDSCore NintendoDS;
@compatibility_alias CocoaDSRom NintendoDS;
@compatibility_alias CocoaDisplayView VideoOutputView;

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
@interface EmuWindowDelegate : NSObject <NSWindowDelegate>
#else
@interface EmuWindowDelegate : NSObject
#endif
{
	NSMutableDictionary *bindings;
	NSImage *iconVolumeFull;
	NSImage *iconVolumeTwoThird;
	NSImage *iconVolumeOneThird;
	NSImage *iconVolumeMute;
	NSImage *iconExecute;
	NSImage *iconPause;
	NSImage *iconSpeedNormal;
	NSImage *iconSpeedDouble;
	
	IBOutlet NSWindow *window;
	IBOutlet CocoaDisplayView *displayView;
	IBOutlet NSView *saveScreenshotPanelAccessoryView;
	IBOutlet NSObjectController *emuWindowController;
	IBOutlet NSObjectController *cdsCoreController;
	IBOutlet NSObjectController *romInfoPanelController;
	
	BOOL isRomLoading;
	NSURL *currentEmuSaveStateURL;
	NSUInteger statusBarHeight;
	NSSize minDisplayViewSize;
	BOOL isSmallestSizeNormal;
	BOOL isShowingStatusBar;
	NSBitmapImageFileType screenshotFileFormat;
}

- (NSMutableDictionary *) bindings;
- (CocoaDisplayView *) displayView;
- (void) setContentScalar:(double)s;
- (void) setContentRotation:(double)angleDegrees;
- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees;
- (double) maxContentScalar:(NSSize)contentBounds;
- (void) setVolume:(float)vol;
- (float) volume;
- (void) setIsRomLoaded:(BOOL)theState;
- (BOOL) isRomLoaded;
- (void) setStatus:(NSString *)theString;
- (NSString *) status;
- (NSURL *) loadedRomURL;
- (void) emulationError;

// File Menu
- (IBAction) openRom:(id)sender;
- (IBAction) closeRom:(id)sender;
- (IBAction) openEmuSaveState:(id)sender;
- (IBAction) saveEmuSaveState:(id)sender;
- (IBAction) saveEmuSaveStateAs:(id)sender;
- (IBAction) revertEmuSaveState:(id)sender;
- (IBAction) loadEmuSaveStateSlot:(id)sender;
- (IBAction) saveEmuSaveStateSlot:(id)sender;
- (IBAction) importRomSave:(id)sender;
- (IBAction) exportRomSave:(id)sender;

// Edit Menu
- (IBAction) copy:(id)sender;

// Emulation Menu
- (IBAction) speedLimitDisable:(id)sender;
- (IBAction) executeCoreToggle:(id)sender;
- (IBAction) resetCore:(id)sender;
- (IBAction) changeRomSaveType:(id)sender;

// Window Elements
- (IBAction) changeCoreSpeed:(id)sender;
- (IBAction) changeVolume:(id)sender;
- (IBAction) changeScale:(id)sender;
- (IBAction) changeRotation:(id)sender;
- (IBAction) changeRotationRelative:(id)sender;
- (IBAction) toggleGPUState:(id)sender;

- (IBAction) toggleMinSize:(id)sender;
- (IBAction) toggleStatusBar:(id)sender;
- (IBAction) changeScreenshotFileFormat:(id)sender;
- (IBAction) saveScreenshotAs:(id)sender;

- (BOOL) handleLoadRom:(NSURL *)fileURL;
- (BOOL) handleUnloadRom:(NSInteger)reasonID romToLoad:(NSURL *)romURL;
- (BOOL) loadRom:(NSURL *)romURL;
- (BOOL) unloadRom;

- (void) executeCore;
- (void) pauseCore;
- (void) restoreCoreState;

@end
