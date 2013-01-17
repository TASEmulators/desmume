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

#import <Cocoa/Cocoa.h>

@protocol DisplayViewDelegate;
@class CocoaDSRom;
@class CocoaDSSpeaker;
@class CocoaDSCheatManager;
@class DisplayViewDelegate;
@class CheatWindowDelegate;


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
	NSView *exportRomSavePanelAccessoryView;
	NSView *saveScreenshotPanelAccessoryView;
	CocoaDSRom *currentRom;
	CocoaDSSpeaker *cdsSpeaker;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSCheatManager *dummyCheatList;
	
	DisplayViewDelegate *dispViewDelegate;
	CheatWindowDelegate *cheatWindowDelegate;
	NSObjectController *romInfoPanelController;
	NSObjectController *firmwarePanelController;
	NSObjectController *cdsCoreController;
	NSObjectController *cdsDisplayController;
	NSObjectController *cdsSoundController;
	NSObjectController *emuWindowController;
	NSObjectController *cheatWindowController;
	NSArrayController *cheatListController;
	NSArrayController *cheatDatabaseController;
	
	NSImage *iconVolumeFull;
	NSImage *iconVolumeTwoThird;
	NSImage *iconVolumeOneThird;
	NSImage *iconVolumeMute;
	NSImage *iconExecute;
	NSImage *iconPause;
	NSImage *iconSpeedNormal;
	NSImage *iconSpeedDouble;
	
	BOOL isRomLoading;
	BOOL isSheetControllingExecution;
	BOOL isShowingSaveStateSheet;
	BOOL isShowingFileMigrationSheet;
	BOOL isShowingStatusBar;
	NSUInteger statusBarHeight;
	NSSize minDisplayViewSize;
	BOOL isMinSizeNormal;
	NSBitmapImageFileType screenshotFileFormat;
	
	NSURL *currentEmuSaveStateURL;
	NSInteger selectedExportRomSaveID;
	NSInteger selectedRomSaveTypeID;
	
	NSMutableDictionary *bindings;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSWindow *saveFileMigrationSheet;
@property (readonly) IBOutlet NSWindow *saveStatePrecloseSheet;
@property (readonly) IBOutlet NSView <DisplayViewDelegate> *displayView;
@property (readonly) IBOutlet NSView *exportRomSavePanelAccessoryView;
@property (readonly) IBOutlet NSView *saveScreenshotPanelAccessoryView;
@property (retain) CocoaDSRom *currentRom;
@property (retain) CocoaDSSpeaker *cdsSpeaker;
@property (retain) CocoaDSCheatManager *cdsCheats;

@property (retain) DisplayViewDelegate *dispViewDelegate;
@property (readonly) IBOutlet CheatWindowDelegate *cheatWindowDelegate;
@property (readonly) IBOutlet NSObjectController *romInfoPanelController;
@property (readonly) IBOutlet NSObjectController *firmwarePanelController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet NSObjectController *cdsDisplayController;
@property (readonly) IBOutlet NSObjectController *cdsSoundController;
@property (readonly) IBOutlet NSObjectController *emuWindowController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSArrayController *cheatListController;
@property (readonly) IBOutlet NSArrayController *cheatDatabaseController;

@property (assign) BOOL isSheetControllingExecution;
@property (assign) BOOL isShowingSaveStateSheet;
@property (assign) BOOL isShowingFileMigrationSheet;
@property (assign) BOOL isMinSizeNormal;
@property (assign) NSInteger selectedRomSaveTypeID;

@property (readonly) NSMutableDictionary *bindings;

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
- (IBAction) selectExportRomSaveFormat:(id)sender;

// Edit Menu
- (IBAction) copy:(id)sender;

// Emulation Menu
- (IBAction) speedLimitDisable:(id)sender;
- (IBAction) toggleAutoFrameSkip:(id)sender;
- (IBAction) cheatsDisable:(id)sender;
- (IBAction) executeCoreToggle:(id)sender;
- (IBAction) resetCore:(id)sender;
- (IBAction) changeRomSaveType:(id)sender;

// View Menu
- (IBAction) hudDisable:(id)sender;
- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender;
- (IBAction) toggleStatusBar:(id)sender;

// Tools Menu
- (IBAction) selectScreenshotFileFormat:(id)sender;
- (IBAction) saveScreenshotAs:(id)sender;
- (void) saveScreenshotAsFinish:(NSNotification *)aNotification;
- (IBAction) toggleGPUState:(id)sender;

// Help Menu


// Window Elements

- (BOOL) handleLoadRom:(NSURL *)fileURL;
- (BOOL) handleUnloadRom:(NSInteger)reasonID romToLoad:(NSURL *)romURL;
- (BOOL) loadRom:(NSURL *)romURL;
- (void) loadRomDidFinish:(NSNotification *)aNotification;
- (BOOL) unloadRom;

- (void) executeCore;
- (void) pauseCore;
- (void) restoreCoreState;

- (IBAction) changeCoreSpeed:(id)sender;
- (IBAction) changeCoreEmuFlags:(id)sender;
- (IBAction) changeFirmwareSettings:(id)sender;
- (IBAction) changeVolume:(id)sender;
- (IBAction) changeAudioEngine:(id)sender;
- (IBAction) changeSpuAdvancedLogic:(id)sender;
- (IBAction) changeSpuInterpolationMode:(id)sender;
- (IBAction) changeSpuSyncMode:(id)sender;
- (IBAction) changeSpuSyncMethod:(id)sender;
- (IBAction) changeScale:(id)sender;
- (IBAction) changeRotation:(id)sender;
- (IBAction) changeRotationRelative:(id)sender;
- (IBAction) changeBilinearOutput:(id)sender;
- (IBAction) changeVerticalSync:(id)sender;
- (IBAction) changeDisplayMode:(id)sender;
- (IBAction) changeDisplayOrientation:(id)sender;
- (IBAction) changeDisplayOrder:(id)sender;
- (IBAction) changeVideoFilter:(id)sender;
- (IBAction) change3DRenderMethod:(id)sender;
- (IBAction) change3DRenderHighPrecisionColorInterpolation:(id)sender;
- (IBAction) change3DRenderEdgeMarking:(id)sender;
- (IBAction) change3DRenderFog:(id)sender;
- (IBAction) change3DRenderTextures:(id)sender;
- (IBAction) change3DRenderDepthComparisonThreshold:(id)sender;
- (IBAction) change3DRenderThreads:(id)sender;
- (IBAction) change3DRenderLineHack:(id)sender;
- (IBAction) change3DRenderMultisample:(id)sender;

- (void) setShowStatusBar:(BOOL)showStatusBar;

- (IBAction) closeMigrationSheet:(id)sender;
- (void) didEndFileMigrationSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheetOpen:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheetTerminate:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (void) setupUserDefaults;
- (IBAction) writeDefaultsDisplayRotation:(id)sender;
- (IBAction) writeDefaultsHUDSettings:(id)sender;
- (IBAction) writeDefaultsVideoOutputSettings:(id)sender;
- (IBAction) writeDefaults3DRenderingSettings:(id)sender;
- (IBAction) writeDefaultsEmulationSettings:(id)sender;
- (IBAction) writeDefaultsSoundSettings:(id)sender;

@end
