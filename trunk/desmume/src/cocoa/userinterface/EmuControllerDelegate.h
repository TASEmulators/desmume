/*
	Copyright (C) 2013-2015 DeSmuME Team

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
#include <libkern/OSAtomic.h>

@class InputManager;
@class CocoaDSRom;
@class CocoaDSFirmware;
@class CocoaDSOutput;
@class CocoaDSSpeaker;
@class CocoaDSCheatManager;
@class CheatWindowDelegate;
@class DisplayWindowController;
class AudioSampleBlockGenerator;

@interface EmuControllerDelegate : NSObject <NSUserInterfaceValidations>
{
	InputManager *inputManager;
	
	CocoaDSRom *currentRom;
	CocoaDSFirmware *cdsFirmware;
	CocoaDSSpeaker *cdsSpeaker;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSCheatManager *dummyCheatList;
	
	CheatWindowDelegate *cheatWindowDelegate;
	NSObjectController *firmwarePanelController;
	NSObjectController *romInfoPanelController;
	NSObjectController *cdsCoreController;
	NSObjectController *cdsSoundController;
	NSObjectController *cheatWindowController;
	NSObjectController *slot2WindowController;
	NSArrayController *cheatListController;
	NSArrayController *cheatDatabaseController;
	
	NSWindow *executionControlWindow;
	NSWindow *slot1ManagerWindow;
	NSWindow *saveFileMigrationSheet;
	NSWindow *saveStatePrecloseSheet;
	NSView *exportRomSavePanelAccessoryView;
	
	BOOL isSaveStateEdited;
	
	BOOL isWorking;
	BOOL isRomLoading;
	NSString *statusText;
	float currentVolumeValue;
	NSImage *currentVolumeIcon;
	BOOL isShowingSaveStateDialog;
	BOOL isShowingFileMigrationDialog;
	BOOL isUserInterfaceBlockingExecution;
	NSURL *currentSaveStateURL;
	NSInteger selectedExportRomSaveID;
	NSInteger selectedRomSaveTypeID;
	NSInteger frameJumpType;
	NSInteger frameJumpFramesForward;
	NSInteger frameJumpToFrame;
	
	CGFloat lastSetSpeedScalar;
	BOOL isSoundMuted;
	float lastSetVolumeValue;
	
	NSImage *iconVolumeFull;
	NSImage *iconVolumeTwoThird;
	NSImage *iconVolumeOneThird;
	NSImage *iconVolumeMute;
	NSImage *iconExecute;
	NSImage *iconPause;
	NSImage *iconSpeedNormal;
	NSImage *iconSpeedDouble;
	
	DisplayWindowController *mainWindow;
	NSMutableArray *windowList;
	
	OSSpinLock spinlockFirmware;
	OSSpinLock spinlockSpeaker;
}

@property (readonly) IBOutlet InputManager *inputManager;

@property (assign) CocoaDSRom *currentRom; // Don't rely on autorelease since the emulator doesn't support concurrent unloading
@property (retain) CocoaDSFirmware *cdsFirmware;
@property (retain) CocoaDSSpeaker *cdsSpeaker;
@property (retain) CocoaDSCheatManager *cdsCheats;

@property (readonly) IBOutlet CheatWindowDelegate *cheatWindowDelegate;
@property (readonly) IBOutlet NSObjectController *firmwarePanelController;
@property (readonly) IBOutlet NSObjectController *romInfoPanelController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet NSObjectController *cdsSoundController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSObjectController *slot2WindowController;
@property (readonly) IBOutlet NSArrayController *cheatListController;
@property (readonly) IBOutlet NSArrayController *cheatDatabaseController;

@property (readonly) IBOutlet NSWindow *executionControlWindow;
@property (readonly) IBOutlet NSWindow *slot1ManagerWindow;
@property (readonly) IBOutlet NSWindow *saveFileMigrationSheet;
@property (readonly) IBOutlet NSWindow *saveStatePrecloseSheet;
@property (readonly) IBOutlet NSView *exportRomSavePanelAccessoryView;

@property (readonly) NSImage *iconExecute;
@property (readonly) NSImage *iconPause;
@property (readonly) NSImage *iconSpeedNormal;
@property (readonly) NSImage *iconSpeedDouble;

@property (readonly) BOOL masterExecuteFlag;
@property (readonly) NSInteger executionState;
@property (readonly) CGFloat lastSetSpeedScalar;
@property (readonly) CGFloat speedScalar;

@property (assign) BOOL isWorking;
@property (assign) BOOL isRomLoading;
@property (assign) NSString *statusText;
@property (assign) float currentVolumeValue;
@property (retain) NSImage *currentVolumeIcon;
@property (assign) BOOL isShowingSaveStateDialog;
@property (assign) BOOL isShowingFileMigrationDialog;
@property (assign) BOOL isUserInterfaceBlockingExecution;
@property (retain) NSURL *currentSaveStateURL;
@property (assign) NSInteger selectedExportRomSaveID;
@property (assign) NSInteger selectedRomSaveTypeID;
@property (assign) NSInteger frameJumpType;
@property (assign) NSInteger frameJumpFramesForward;
@property (assign) NSInteger frameJumpToFrame;

@property (retain) DisplayWindowController *mainWindow;
@property (readonly) NSMutableArray *windowList;

// File Menu
- (IBAction) newDisplayWindow:(id)sender;
- (IBAction) openRom:(id)sender;
- (IBAction) loadRecentRom:(id)sender;
- (IBAction) closeRom:(id)sender;
- (IBAction) revealRomInFinder:(id)sender;
- (IBAction) revealGameDataFolderInFinder:(id)sender;
- (IBAction) openEmuSaveState:(id)sender;
- (IBAction) saveEmuSaveState:(id)sender;
- (IBAction) saveEmuSaveStateAs:(id)sender;
- (IBAction) revertEmuSaveState:(id)sender;
- (IBAction) loadEmuSaveStateSlot:(id)sender;
- (IBAction) saveEmuSaveStateSlot:(id)sender;
- (IBAction) openReplay:(id)sender;
- (IBAction) recordReplay:(id)sender;
- (IBAction) stopReplay:(id)sender;
- (IBAction) importRomSave:(id)sender;
- (IBAction) exportRomSave:(id)sender;

// Emulation Menu
- (IBAction) toggleSpeedLimiter:(id)sender;
- (IBAction) toggleAutoFrameSkip:(id)sender;
- (IBAction) toggleCheats:(id)sender;
- (IBAction) toggleExecutePause:(id)sender;
- (IBAction) coreExecute:(id)sender;
- (IBAction) corePause:(id)sender;
- (IBAction) frameAdvance:(id)sender;
- (IBAction) frameJump:(id)sender;
- (IBAction) reset:(id)sender;
- (IBAction) changeRomSaveType:(id)sender;

// View Menu
- (IBAction) toggleAllDisplays:(id)sender;

// Tools Menu
- (IBAction) autoholdSet:(id)sender;
- (IBAction) toggleGPUState:(id)sender;
- (IBAction) toggleGDBStubActivate:(id)sender;

- (IBAction) changeCoreSpeed:(id)sender;
- (IBAction) changeCoreEmuFlags:(id)sender;
- (IBAction) changeFirmwareSettings:(id)sender;
- (IBAction) changeVolume:(id)sender;
- (IBAction) changeAudioEngine:(id)sender;
- (IBAction) changeSpuAdvancedLogic:(id)sender;
- (IBAction) changeSpuInterpolationMode:(id)sender;
- (IBAction) changeSpuSyncMode:(id)sender;
- (IBAction) changeSpuSyncMethod:(id)sender;

// Misc IBActions
- (IBAction) chooseSlot1R4Directory:(id)sender;
- (IBAction) slot1Eject:(id)sender;

- (IBAction) writeDefaults3DRenderingSettings:(id)sender;
- (IBAction) writeDefaultsEmulationSettings:(id)sender;
- (IBAction) writeDefaultsSlot1Settings:(id)sender;
- (IBAction) writeDefaultsSoundSettings:(id)sender;

- (IBAction) closeSheet:(id)sender;

- (void) cmdUpdateDSController:(NSValue *)cmdAttrValue;
- (void) cmdUpdateDSControllerWithTurbo:(NSValue *)cmdAttrValue;
- (void) cmdUpdateDSTouch:(NSValue *)cmdAttrValue;
- (void) cmdUpdateDSMicrophone:(NSValue *)cmdAttrValue;
- (void) cmdUpdateDSPaddle:(NSValue *)cmdAttrValue;

- (void) cmdLoadEmuSaveStateSlot:(NSValue *)cmdAttrValue;
- (void) cmdSaveEmuSaveStateSlot:(NSValue *)cmdAttrValue;

- (void) cmdCopyScreen:(NSValue *)cmdAttrValue;
- (void) cmdRotateDisplayRelative:(NSValue *)cmdAttrValue;
- (void) cmdToggleAllDisplays:(NSValue *)cmdAttrValue;

- (void) cmdHoldToggleSpeedScalar:(NSValue *)cmdAttrValue;
- (void) cmdToggleSpeedLimiter:(NSValue *)cmdAttrValue;
- (void) cmdToggleAutoFrameSkip:(NSValue *)cmdAttrValue;
- (void) cmdToggleCheats:(NSValue *)cmdAttrValue;
- (void) cmdToggleExecutePause:(NSValue *)cmdAttrValue;
- (void) cmdCoreExecute:(NSValue *)cmdAttrValue;
- (void) cmdCorePause:(NSValue *)cmdAttrValue;
- (void) cmdFrameAdvance:(NSValue *)cmdAttrValue;
- (void) cmdFrameJump:(NSValue *)cmdAttrValue;
- (void) cmdReset:(NSValue *)cmdAttrValue;
- (void) cmdToggleGPUState:(NSValue *)cmdAttrValue;

- (BOOL) handleLoadRomByURL:(NSURL *)fileURL;
- (BOOL) handleUnloadRom:(NSInteger)reasonID romToLoad:(NSURL *)romURL;
- (BOOL) loadRomByURL:(NSURL *)romURL asynchronous:(BOOL)willLoadAsync;
- (void) loadRomDidFinish:(NSNotification *)aNotification;
- (BOOL) unloadRom;

- (void) addOutputToCore:(CocoaDSOutput *)theOutput;
- (void) removeOutputFromCore:(CocoaDSOutput *)theOutput;
- (void) changeCoreSpeedWithDouble:(double)newSpeedScalar;
- (void) executeCore;
- (void) pauseCore;
- (void) restoreCoreState;

- (AudioSampleBlockGenerator *) selectedAudioFileGenerator;
- (void) setSelectedAudioFileGenerator:(AudioSampleBlockGenerator *)theGenerator;

- (void) didEndFileMigrationSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheetOpen:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheetTerminate:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndChooseSlot1R4Directory:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (void) updateAllWindowTitles;
- (void) setupUserDefaults;

@end
