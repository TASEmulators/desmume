/*
	Copyright (C) 2013-2018 DeSmuME Team

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
#import "../cocoa_input.h"

@class InputManager;
@class CocoaDSRom;
@class CocoaDSFirmware;
@class CocoaDSController;
@class CocoaDSOutput;
@class CocoaDSSpeaker;
@class CocoaDSCheatManager;
@class CheatWindowDelegate;
@class DisplayWindowController;
@class RomInfoPanel;
@class MacScreenshotCaptureToolDelegate;
@class MacAVCaptureToolDelegate;
@class PreferencesWindowDelegate;
struct ClientCommandAttributes;
class AudioSampleBlockGenerator;

@interface EmuControllerDelegate : NSObject <NSUserInterfaceValidations, CocoaDSControllerDelegate>
{
	CocoaDSRom *currentRom;
	CocoaDSFirmware *cdsFirmware;
	CocoaDSSpeaker *cdsSpeaker;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSCheatManager *dummyCheatList;
	
	NSString *_displayRotationPanelTitle;
	NSString *_displaySeparationPanelTitle;
	NSString *_displayVideoSettingsPanelTitle;
	NSString *_displayHUDSettingsPanelTitle;
	
	BOOL isSaveStateEdited;
	
	BOOL isWorking;
	BOOL isRomLoading;
	NSString *statusText;
	float currentVolumeValue;
	NSImage *currentMicStatusIcon;
	NSImage *currentVolumeIcon;
	BOOL isShowingSaveStateDialog;
	BOOL isShowingFileMigrationDialog;
	BOOL isUserInterfaceBlockingExecution;
	BOOL _isNDSErrorSheetAlreadyShown;
	NSURL *currentSaveStateURL;
	NSInteger selectedExportRomSaveID;
	NSInteger selectedRomSaveTypeID;
	
	CGFloat lastSetSpeedScalar;
	BOOL isHardwareMicAvailable;
	float currentMicGainValue;
	BOOL isSoundMuted;
	float lastSetVolumeValue;
	
	NSImage *iconMicDisabled;
	NSImage *iconMicIdle;
	NSImage *iconMicActive;
	NSImage *iconMicInClip;
	NSImage *iconMicManualOverride;
	NSImage *iconVolumeFull;
	NSImage *iconVolumeTwoThird;
	NSImage *iconVolumeOneThird;
	NSImage *iconVolumeMute;
	NSImage *iconExecute;
	NSImage *iconPause;
	NSImage *iconSpeedNormal;
	NSImage *iconSpeedDouble;
	
	DisplayWindowController *mainWindow;
	NSMutableArray<DisplayWindowController*> *windowList;
	
	OSSpinLock spinlockFirmware;
	OSSpinLock spinlockSpeaker;
}

@property (weak) IBOutlet InputManager *inputManager;

@property (strong) CocoaDSRom *currentRom; // Don't rely on autorelease since the emulator doesn't support concurrent unloading
@property (strong) CocoaDSFirmware *cdsFirmware;
@property (strong) CocoaDSSpeaker *cdsSpeaker;
@property (strong) CocoaDSCheatManager *cdsCheats;

@property (weak) IBOutlet CheatWindowDelegate *cheatWindowDelegate;
@property (weak) IBOutlet MacScreenshotCaptureToolDelegate *screenshotCaptureToolDelegate;
@property (weak) IBOutlet MacAVCaptureToolDelegate *avCaptureToolDelegate;
@property (weak) IBOutlet PreferencesWindowDelegate *prefWindowDelegate;
@property (weak) IBOutlet NSObjectController *firmwarePanelController;
@property (weak) IBOutlet NSObjectController *romInfoPanelController;
@property (weak) IBOutlet NSObjectController *cdsCoreController;
@property (weak) IBOutlet NSObjectController *cdsSoundController;
@property (weak) IBOutlet NSObjectController *cheatWindowController;
@property (weak) IBOutlet NSObjectController *slot2WindowController;
@property (weak) IBOutlet NSArrayController *inputDeviceListController;
@property (weak) IBOutlet NSArrayController *cheatListController;
@property (weak) IBOutlet NSArrayController *cheatDatabaseController;

@property (weak) IBOutlet RomInfoPanel *romInfoPanel;

@property (weak) IBOutlet NSWindow *displayRotationPanel;
@property (weak) IBOutlet NSWindow *displaySeparationPanel;
@property (weak) IBOutlet NSWindow *displayVideoSettingsPanel;
@property (weak) IBOutlet NSWindow *displayHUDSettingsPanel;

@property (weak) IBOutlet NSWindow *executionControlWindow;
@property (weak) IBOutlet NSWindow *slot1ManagerWindow;
@property (weak) IBOutlet NSWindow *saveFileMigrationSheet;
@property (weak) IBOutlet NSWindow *saveStatePrecloseSheet;
@property (weak) IBOutlet NSWindow *ndsErrorSheet;
@property (weak) IBOutlet NSTextField *ndsErrorStatusTextField;
@property (weak) IBOutlet NSView *exportRomSavePanelAccessoryView;

@property (weak) IBOutlet NSPopUpButton *openglMSAAPopUpButton;

@property (readonly, strong) NSImage *iconExecute;
@property (readonly, strong) NSImage *iconPause;
@property (readonly, strong) NSImage *iconSpeedNormal;
@property (readonly, strong) NSImage *iconSpeedDouble;

@property (readonly) CGFloat lastSetSpeedScalar;

@property (assign) BOOL isWorking;
@property (assign) BOOL isRomLoading;
@property (copy) NSString *statusText;
@property (assign) BOOL isHardwareMicAvailable;
@property (assign) float currentMicGainValue;
@property (nonatomic, assign) float currentVolumeValue;
@property (strong) NSImage *currentMicStatusIcon;
@property (strong) NSImage *currentVolumeIcon;
@property (assign) BOOL isShowingSaveStateDialog;
@property (assign) BOOL isShowingFileMigrationDialog;
@property (assign) BOOL isUserInterfaceBlockingExecution;
@property (copy) NSURL *currentSaveStateURL;
@property (assign) NSInteger selectedExportRomSaveID;
@property (assign) NSInteger selectedRomSaveTypeID;

@property (strong) DisplayWindowController *mainWindow;
@property (readonly, strong) NSMutableArray<DisplayWindowController*> *windowList;

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
- (IBAction) toggleGPUState:(id)sender;
- (IBAction) toggleGDBStubActivate:(id)sender;

- (IBAction) autoholdSet:(id)sender;
- (IBAction) autoholdClear:(id)sender;
- (IBAction) setVerticalSyncForNonLayerBackedViews:(id)sender;

- (IBAction) changeCoreSpeed:(id)sender;
- (IBAction) changeFirmwareSettings:(id)sender;
- (IBAction) changeHardwareMicGain:(id)sender;
- (IBAction) changeHardwareMicMute:(id)sender;
- (IBAction) changeVolume:(id)sender;
- (IBAction) changeAudioEngine:(id)sender;
- (IBAction) changeSpuAdvancedLogic:(id)sender;
- (IBAction) changeSpuInterpolationMode:(id)sender;
- (IBAction) changeSpuSyncMode:(id)sender;
- (IBAction) changeSpuSyncMethod:(id)sender;

// Misc IBActions
- (IBAction) chooseSlot1R4Directory:(id)sender;
- (IBAction) slot1Eject:(id)sender;
- (IBAction) simulateEmulationCrash:(id)sender;
- (IBAction) generateFirmwareMACAddress:(id)sender;

- (IBAction) writeDefaults3DRenderingSettings:(id)sender;
- (IBAction) writeDefaultsEmulationSettings:(id)sender;
- (IBAction) writeDefaultsSlot1Settings:(id)sender;
- (IBAction) writeDefaultsSoundSettings:(id)sender;
- (IBAction) writeDefaultsStylusSettings:(id)sender;

- (IBAction) closeSheet:(id)sender;

- (void) cmdUpdateDSController:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdUpdateDSControllerWithTurbo:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdUpdateDSTouch:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdUpdateDSMicrophone:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdUpdateDSPaddle:(const ClientCommandAttributes &)cmdAttr;

- (void) cmdAutoholdSet:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdAutoholdClear:(const ClientCommandAttributes &)cmdAttr;

- (void) cmdLoadEmuSaveStateSlot:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdSaveEmuSaveStateSlot:(const ClientCommandAttributes &)cmdAttr;

- (void) cmdCopyScreen:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdRotateDisplayRelative:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleAllDisplays:(const ClientCommandAttributes &)cmdAttr;

- (void) cmdHoldToggleSpeedScalar:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleSpeedLimiter:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleAutoFrameSkip:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleCheats:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleExecutePause:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdCoreExecute:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdCorePause:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdFrameAdvance:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdFrameJump:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdReset:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleMute:(const ClientCommandAttributes &)cmdAttr;
- (void) cmdToggleGPUState:(const ClientCommandAttributes &)cmdAttr;

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
- (void) updateMicStatusIcon;

- (AudioSampleBlockGenerator *) selectedAudioFileGenerator;
- (void) setSelectedAudioFileGenerator:(AudioSampleBlockGenerator *)theGenerator;

- (void) didEndFileMigrationSheet:(NSWindow *)sheet returnCode:(NSModalResponse)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheet:(NSWindow *)sheet returnCode:(NSModalResponse)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheetOpen:(NSWindow *)sheet returnCode:(NSModalResponse)returnCode contextInfo:(void *)contextInfo;
- (void) didEndSaveStateSheetTerminate:(NSWindow *)sheet returnCode:(NSModalResponse)returnCode contextInfo:(void *)contextInfo;

- (void) updateAllWindowTitles;
- (void) updateDisplayPanelTitles;
- (void) appInit;
- (void) fillOpenGLMSAAMenu;
- (void) readUserDefaults;
- (void) writeUserDefaults;
- (void) restoreDisplayWindowStates;
- (void) saveDisplayWindowStates;

@end
