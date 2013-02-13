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

#import <Cocoa/Cocoa.h>

@protocol DisplayViewDelegate;
@class DisplayViewDelegate;


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface EmuWindowDelegate : NSObject <NSWindowDelegate>
#else
@interface EmuWindowDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	NSWindow *saveFileMigrationSheet;
	NSWindow *saveStatePrecloseSheet;
	NSView <DisplayViewDelegate> *displayView;
	NSView *saveScreenshotPanelAccessoryView;
	
	DisplayViewDelegate *dispViewDelegate;
	NSObjectController *cdsDisplayController;
	NSObjectController *emuControlController;
	NSObjectController *emuWindowController;
	
	BOOL isShowingStatusBar;
	NSUInteger statusBarHeight;
	NSSize minDisplayViewSize;
	BOOL isMinSizeNormal;
	NSBitmapImageFileType screenshotFileFormat;
	
	NSMutableDictionary *bindings;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSView <DisplayViewDelegate> *displayView;
@property (readonly) IBOutlet NSView *saveScreenshotPanelAccessoryView;

@property (retain) DisplayViewDelegate *dispViewDelegate;
@property (readonly) IBOutlet NSObjectController *cdsDisplayController;
@property (readonly) IBOutlet NSObjectController *emuControlController;
@property (readonly) IBOutlet NSObjectController *emuWindowController;

@property (assign) BOOL isMinSizeNormal;

@property (readonly) NSMutableDictionary *bindings;

- (void) setContentScalar:(double)s;
- (void) setContentRotation:(double)angleDegrees;
- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees;
- (double) maxContentScalar:(NSSize)contentBounds;

// Edit Menu
- (IBAction) copy:(id)sender;

// View Menu
- (IBAction) hudDisable:(id)sender;
- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender;
- (IBAction) toggleStatusBar:(id)sender;

// Tools Menu
- (IBAction) selectScreenshotFileFormat:(id)sender;
- (IBAction) saveScreenshotAs:(id)sender;
- (void) saveScreenshotAsFinish:(NSNotification *)aNotification;

- (IBAction) changeScale:(id)sender;
- (IBAction) changeRotation:(id)sender;
- (IBAction) changeRotationRelative:(id)sender;
- (IBAction) changeBilinearOutput:(id)sender;
- (IBAction) changeVerticalSync:(id)sender;
- (IBAction) changeDisplayMode:(id)sender;
- (IBAction) changeDisplayOrientation:(id)sender;
- (IBAction) changeDisplayOrder:(id)sender;
- (IBAction) changeVideoFilter:(id)sender;

- (void) setShowStatusBar:(BOOL)showStatusBar;

- (void) setupUserDefaults;
- (IBAction) writeDefaultsDisplayRotation:(id)sender;
- (IBAction) writeDefaultsHUDSettings:(id)sender;
- (IBAction) writeDefaultsVideoOutputSettings:(id)sender;

@end
