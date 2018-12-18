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

#import "EmuControllerDelegate.h"
#import "DisplayWindowController.h"
#import "InputManager.h"
#import "cheatWindowDelegate.h"
#import "Slot2WindowDelegate.h"
#import "MacAVCaptureTool.h"
#import "MacScreenshotCaptureTool.h"
#import "preferencesWindowDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_cheat.h"
#import "cocoa_core.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_GPU.h"
#import "cocoa_input.h"
#import "cocoa_output.h"
#import "cocoa_rom.h"
#import "cocoa_slot2.h"

@implementation EmuControllerDelegate

@synthesize inputManager;

@synthesize currentRom;
@dynamic cdsFirmware;
@dynamic cdsSpeaker;
@synthesize cdsCheats;

@synthesize cheatWindowDelegate;
@synthesize screenshotCaptureToolDelegate;
@synthesize avCaptureToolDelegate;
@synthesize prefWindowDelegate;
@synthesize firmwarePanelController;
@synthesize romInfoPanelController;
@synthesize cdsCoreController;
@synthesize cdsSoundController;
@synthesize cheatWindowController;
@synthesize cheatListController;
@synthesize cheatDatabaseController;
@synthesize slot2WindowController;
@synthesize inputDeviceListController;

@synthesize romInfoPanel;

@synthesize displayRotationPanel;
@synthesize displaySeparationPanel;
@synthesize displayVideoSettingsPanel;
@synthesize displayHUDSettingsPanel;

@synthesize executionControlWindow;
@synthesize slot1ManagerWindow;
@synthesize saveFileMigrationSheet;
@synthesize saveStatePrecloseSheet;
@synthesize ndsErrorSheet;
@synthesize ndsErrorStatusTextField;
@synthesize exportRomSavePanelAccessoryView;

@synthesize openglMSAAPopUpButton;

@synthesize iconExecute;
@synthesize iconPause;
@synthesize iconSpeedNormal;
@synthesize iconSpeedDouble;

@synthesize lastSetSpeedScalar;

@synthesize isWorking;
@synthesize isRomLoading;
@synthesize statusText;
@synthesize isHardwareMicAvailable;
@synthesize currentMicGainValue;
@dynamic currentVolumeValue;
@synthesize currentMicStatusIcon;
@synthesize currentVolumeIcon;

@synthesize isShowingSaveStateDialog;
@synthesize isShowingFileMigrationDialog;
@synthesize isUserInterfaceBlockingExecution;

@synthesize currentSaveStateURL;
@synthesize selectedExportRomSaveID;
@synthesize selectedRomSaveTypeID;

@synthesize mainWindow;
@synthesize windowList;


- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	spinlockFirmware = OS_SPINLOCK_INIT;
	spinlockSpeaker = OS_SPINLOCK_INIT;
	
	mainWindow = nil;
	windowList = [[NSMutableArray alloc] initWithCapacity:32];
	
	_displayRotationPanelTitle = nil;
	_displaySeparationPanelTitle = nil;
	_displayVideoSettingsPanelTitle = nil;
	_displayHUDSettingsPanelTitle = nil;
	
	currentRom = nil;
	cdsFirmware = nil;
	cdsSpeaker = nil;
	dummyCheatList = nil;
	
	isSaveStateEdited = NO;
	isShowingSaveStateDialog = NO;
	isShowingFileMigrationDialog = NO;
	isUserInterfaceBlockingExecution = NO;
	_isNDSErrorSheetAlreadyShown = NO;
	
	currentSaveStateURL = nil;
	selectedRomSaveTypeID = ROMSAVETYPE_AUTOMATIC;
	selectedExportRomSaveID = 0;
	
	lastSetSpeedScalar = 1.0f;
	isHardwareMicAvailable = NO;
	currentMicGainValue = 0.0f;
	isSoundMuted = NO;
	lastSetVolumeValue = MAX_VOLUME;
	
	iconExecute				= [[NSImage imageNamed:@"Icon_Execute_420x420"] retain];
	iconPause				= [[NSImage imageNamed:@"Icon_Pause_420x420"] retain];
	iconSpeedNormal			= [[NSImage imageNamed:@"Icon_Speed1x_420x420"] retain];
	iconSpeedDouble			= [[NSImage imageNamed:@"Icon_Speed2x_420x420"] retain];
	
	iconMicDisabled			= [[NSImage imageNamed:@"Icon_MicrophoneBlack_256x256"] retain];
	iconMicIdle				= [[NSImage imageNamed:@"Icon_MicrophoneDarkGreen_256x256"] retain];
	iconMicActive			= [[NSImage imageNamed:@"Icon_MicrophoneGreen_256x256"] retain];
	iconMicInClip			= [[NSImage imageNamed:@"Icon_MicrophoneRed_256x256"] retain];
	iconMicManualOverride	= [[NSImage imageNamed:@"Icon_MicrophoneGray_256x256"] retain];
	
	iconVolumeFull			= [[NSImage imageNamed:@"Icon_VolumeFull_16x16"] retain];
	iconVolumeTwoThird		= [[NSImage imageNamed:@"Icon_VolumeTwoThird_16x16"] retain];
	iconVolumeOneThird		= [[NSImage imageNamed:@"Icon_VolumeOneThird_16x16"] retain];
	iconVolumeMute			= [[NSImage imageNamed:@"Icon_VolumeMute_16x16"] retain];
	
	isWorking = NO;
	isRomLoading = NO;
	statusText = NSSTRING_STATUS_READY;
	currentVolumeValue = MAX_VOLUME;
	currentMicStatusIcon = [iconMicDisabled retain];
	currentVolumeIcon = [iconVolumeFull retain];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(loadRomDidFinish:)
												 name:@"org.desmume.DeSmuME.loadRomDidFinish"
											   object:nil];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(handleNDSError:)
												 name:@"org.desmume.DeSmuME.handleNDSError"
											   object:nil];
	
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[iconExecute release];
	[iconPause release];
	[iconSpeedNormal release];
	[iconSpeedDouble release];
	
	[iconMicDisabled release];
	[iconMicIdle release];
	[iconMicActive release];
	[iconMicInClip release];
	[iconMicManualOverride release];
	
	[iconVolumeFull release];
	[iconVolumeTwoThird release];
	[iconVolumeOneThird release];
	[iconVolumeMute release];
	
	[[self currentRom] release];
	[self setCurrentRom:nil];
	
	[self setCdsCheats:nil];
	[self setCdsSpeaker:nil];
	
	[self setIsWorking:NO];
	[self setStatusText:@""];
	[self setCurrentVolumeValue:0.0f];
	[self setCurrentVolumeIcon:nil];
	
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	[cdsSoundController setContent:nil];
	[firmwarePanelController setContent:nil];
	
	[self setMainWindow:nil];
	[windowList release];
	
	[super dealloc];
}

#pragma mark Dynamic Property Methods

- (void) setCdsFirmware:(CocoaDSFirmware *)theFirmware
{
	OSSpinLockLock(&spinlockFirmware);
	
	if (theFirmware == cdsFirmware)
	{
		OSSpinLockUnlock(&spinlockFirmware);
		return;
	}
	
	if (theFirmware != nil)
	{
		[theFirmware retain];
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCdsFirmware:theFirmware];
	[cdsCore updateFirmwareMACAddressString];
	[firmwarePanelController setContent:theFirmware];
	
	[cdsFirmware release];
	cdsFirmware = theFirmware;
	
	OSSpinLockUnlock(&spinlockFirmware);
}

- (CocoaDSFirmware *) cdsFirmware
{
	OSSpinLockLock(&spinlockFirmware);
	CocoaDSFirmware *theFirmware = cdsFirmware;
	OSSpinLockUnlock(&spinlockFirmware);
	
	return theFirmware;
}

- (void) setCdsSpeaker:(CocoaDSSpeaker *)theSpeaker
{
	OSSpinLockLock(&spinlockSpeaker);
	
	if (theSpeaker == cdsSpeaker)
	{
		OSSpinLockUnlock(&spinlockSpeaker);
		return;
	}
	
	if (theSpeaker != nil)
	{
		[theSpeaker retain];
	}
	
	[cdsSoundController setContent:[theSpeaker property]];
	
	[cdsSpeaker release];
	cdsSpeaker = theSpeaker;
	
	OSSpinLockUnlock(&spinlockSpeaker);
}

- (CocoaDSSpeaker *) cdsSpeaker
{
	OSSpinLockLock(&spinlockSpeaker);
	CocoaDSSpeaker *theSpeaker = cdsSpeaker;
	OSSpinLockUnlock(&spinlockSpeaker);
	
	return theSpeaker;
}

- (void) setCurrentVolumeValue:(float)vol
{
	currentVolumeValue = vol;
	
	// Update the icon.
	NSImage *currentImage = [self currentVolumeIcon];
	NSImage *newImage = nil;
	if (vol <= 0.0f)
	{
		newImage = iconVolumeMute;
	}
	else if (vol > 0.0f && vol <= VOLUME_THRESHOLD_LOW)
	{
		newImage = iconVolumeOneThird;
		isSoundMuted = NO;
		lastSetVolumeValue = vol;
	}
	else if (vol > VOLUME_THRESHOLD_LOW && vol <= VOLUME_THRESHOLD_HIGH)
	{
		newImage = iconVolumeTwoThird;
		isSoundMuted = NO;
		lastSetVolumeValue = vol;
	}
	else
	{
		newImage = iconVolumeFull;
		isSoundMuted = NO;
		lastSetVolumeValue = vol;
	}
	
	if (newImage != currentImage)
	{
		[self setCurrentVolumeIcon:newImage];
	}
}

- (float) currentVolumeValue
{
	return currentVolumeValue;
}

#pragma mark IBActions

- (IBAction) newDisplayWindow:(id)sender
{
	DisplayWindowController *newWindowController = [[DisplayWindowController alloc] initWithWindowNibName:@"DisplayWindow" emuControlDelegate:self];
	
	[newWindowController window]; // Just reference the window to force the nib to load.
	[[newWindowController view] setAllowViewUpdates:YES];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	const BOOL useVerticalSync = ([[newWindowController view] layer] != nil) || [cdsCore isFrameSkipEnabled] || (([cdsCore speedScalar] < 1.03f) && [cdsCore isSpeedLimitEnabled]);
	[[newWindowController view] setUseVerticalSync:useVerticalSync];
	
	[[[newWindowController view] cdsVideoOutput] handleReloadReprocessRedraw];
	[[newWindowController window] makeKeyAndOrderFront:self];
	[[newWindowController window] makeMainWindow];
}

- (IBAction) openRom:(id)sender
{
	if ([self isRomLoading])
	{
		return;
	}
	
	NSURL *selectedFile = nil;
	
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_OPEN_ROM_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ROM_DS, @FILE_EXT_ROM_GBA, nil]; 
	
	// The NSOpenPanel method -(NSInt)runModalForDirectory:file:types:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	const NSInteger buttonClicked = [panel runModal];
#else
	const NSInteger buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
#endif
	
	if (buttonClicked == NSFileHandlingPanelOKButton)
	{
		selectedFile = [[panel URLs] lastObject];
		if(selectedFile == nil)
		{
			return;
		}
		
		[self handleLoadRomByURL:selectedFile];
	}
}

- (IBAction) loadRecentRom:(id)sender
{
	// Dummy selector, used for UI validation only.
}

- (IBAction) closeRom:(id)sender
{
	[self handleUnloadRom:REASONFORCLOSE_NORMAL romToLoad:nil];
}

- (IBAction) revealRomInFinder:(id)sender
{
	NSURL *romURL = [[self currentRom] fileURL];
	
	if (romURL != nil)
	{
		[[NSWorkspace sharedWorkspace] selectFile:[romURL path] inFileViewerRootedAtPath:@""];
	}
}

- (IBAction) revealGameDataFolderInFinder:(id)sender
{
	NSURL *folderURL = [CocoaDSFile userAppSupportURL:nil version:nil];
	[[NSWorkspace sharedWorkspace] selectFile:[folderURL path] inFileViewerRootedAtPath:@""];
}

- (IBAction) openEmuSaveState:(id)sender
{
	NSURL *selectedFile = nil;
	
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_OPEN_STATE_FILE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_SAVE_STATE, nil];
	
	// The NSOpenPanel method -(NSInt)runModalForDirectory:file:types:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	const NSInteger buttonClicked = [panel runModal];
#else
	const NSInteger buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
#endif
	
	if (buttonClicked == NSFileHandlingPanelOKButton)
	{
		selectedFile = [[panel URLs] lastObject];
		if(selectedFile == nil)
		{
			return;
		}
	}
	else
	{
		return;
	}
	
	[self pauseCore];
	
	const BOOL isStateLoaded = [CocoaDSFile loadState:selectedFile];
	if (!isStateLoaded)
	{
		[self setStatusText:NSSTRING_STATUS_SAVESTATE_LOADING_FAILED];
		[self restoreCoreState];
		return;
	}
	
	isSaveStateEdited = YES;
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] setDocumentEdited:isSaveStateEdited];
	}
	
	[self setStatusText:NSSTRING_STATUS_SAVESTATE_LOADED];
	[self restoreCoreState];
	
	[self setCurrentSaveStateURL:selectedFile];
}

- (IBAction) saveEmuSaveState:(id)sender
{
	if (isSaveStateEdited && [self currentSaveStateURL] != nil)
	{
		[self pauseCore];
		
		const BOOL isStateSaved = [CocoaDSFile saveState:[self currentSaveStateURL]];
		if (!isStateSaved)
		{
			[self setStatusText:NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
			return;
		}
		
		isSaveStateEdited = YES;
		for (DisplayWindowController *windowController in windowList)
		{
			[[windowController window] setDocumentEdited:isSaveStateEdited];
		}
		
		[self setStatusText:NSSTRING_STATUS_SAVESTATE_SAVED];
		[self restoreCoreState];
	}
	else
	{
		[self saveEmuSaveStateAs:sender];
	}
}

- (IBAction) saveEmuSaveStateAs:(id)sender
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setCanCreateDirectories:YES];
	[panel setTitle:NSSTRING_TITLE_SAVE_STATE_FILE_PANEL];
	
	// The NSSavePanel method -(void)setRequiredFileType:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_SAVE_STATE, nil];
	[panel setAllowedFileTypes:fileTypes];
#else
	[panel setRequiredFileType:@FILE_EXT_SAVE_STATE];
#endif
	
	const NSInteger buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		NSURL *saveFileURL = [panel URL];
		
		[self pauseCore];
		
		const BOOL isStateSaved = [CocoaDSFile saveState:saveFileURL];
		if (!isStateSaved)
		{
			[self setStatusText:NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
			return;
		}
		
		isSaveStateEdited = YES;
		for (DisplayWindowController *windowController in windowList)
		{
			[[windowController window] setDocumentEdited:isSaveStateEdited];
		}
		
		[self setStatusText:NSSTRING_STATUS_SAVESTATE_SAVED];
		[self restoreCoreState];
		
		[self setCurrentSaveStateURL:saveFileURL];
	}
}

- (IBAction) revertEmuSaveState:(id)sender
{
	if (!isSaveStateEdited || [self currentSaveStateURL] == nil)
	{
		return;
	}
	
	[self pauseCore];
	
	const BOOL isStateLoaded = [CocoaDSFile loadState:[self currentSaveStateURL]];
	if (!isStateLoaded)
	{
		[self setStatusText:NSSTRING_STATUS_SAVESTATE_REVERTING_FAILED];
		return;
	}
	
	isSaveStateEdited = YES;
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] setDocumentEdited:isSaveStateEdited];
	}
	
	[self setStatusText:NSSTRING_STATUS_SAVESTATE_REVERTED];
	[self restoreCoreState];
}

- (IBAction) loadEmuSaveStateSlot:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) saveEmuSaveStateSlot:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) openReplay:(id)sender
{
	NSURL *selectedFile = nil;
	
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:@"Load Replay"];
	NSArray *fileTypes = [NSArray arrayWithObjects:@"dsm", nil];
	
	// The NSOpenPanel method -(NSInt)runModalForDirectory:file:types:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	const NSInteger buttonClicked = [panel runModal];
#else
	const NSInteger buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
#endif
	
	if (buttonClicked == NSFileHandlingPanelOKButton)
	{
		selectedFile = [[panel URLs] lastObject];
		if(selectedFile == nil)
		{
			return;
		}
		
		[self pauseCore];
		const BOOL isMovieLoaded = [CocoaDSFile loadReplay:selectedFile];
		[self setStatusText:(isMovieLoaded) ? @"Replay loaded successfully." : @"Replay loading failed!"];
		[self restoreCoreState];
	}
}

- (IBAction) recordReplay:(id)sender
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setCanCreateDirectories:YES];
	[panel setTitle:@"Record Replay"];
	
	// The NSSavePanel method -(void)setRequiredFileType:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	NSArray *fileTypes = [NSArray arrayWithObjects:@"dsm", nil];
	[panel setAllowedFileTypes:fileTypes];
#else
	[panel setRequiredFileType:@"dsm"];
#endif
	
	const NSInteger buttonClicked = [panel runModal];
	if (buttonClicked == NSFileHandlingPanelOKButton)
	{
		NSURL *fileURL = [panel URL];
		if(fileURL == nil)
		{
			return;
		}
		
		[self pauseCore];
		CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
		NSURL *sramURL = [CocoaDSFile fileURLFromRomURL:[[self currentRom] fileURL] toKind:@"ROM Save"];
		
		NSFileManager *fileManager = [[NSFileManager alloc] init];
		const BOOL exists = [fileManager isReadableFileAtPath:[sramURL path]];
		[fileManager release];
		
		const BOOL isMovieStarted = [cdsCore startReplayRecording:fileURL sramURL:sramURL];
		[self setStatusText:(isMovieStarted) ? @"Replay recording started." : @"Replay creation failed!"];
		[self restoreCoreState];
	}
}

- (IBAction) stopReplay:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[self pauseCore];
	[cdsCore stopReplay];
	[self setStatusText:@"Replay stopped."];
	[self restoreCoreState];
}

- (IBAction) importRomSave:(id)sender
{
	NSURL *selectedFile = nil;
	
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_IMPORT_ROM_SAVE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:
						  @FILE_EXT_ROM_SAVE,
						  @FILE_EXT_ROM_SAVE_RAW,
						  @FILE_EXT_ACTION_REPLAY_SAVE,
						  @FILE_EXT_ACTION_REPLAY_MAX_SAVE,
						  nil];
	
	[self pauseCore];
	
	// The NSOpenPanel method -(NSInt)runModalForDirectory:file:types:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	const NSInteger buttonClicked = [panel runModal];
#else
	const NSInteger buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
#endif
	
	if (buttonClicked == NSFileHandlingPanelOKButton)
	{
		selectedFile = [[panel URLs] lastObject];
		if(selectedFile == nil)
		{
			return;
		}
		
		const BOOL isRomSaveImported = [CocoaDSFile importRomSave:selectedFile];
		[self setStatusText:(isRomSaveImported) ? NSSTRING_STATUS_ROM_SAVE_IMPORTED : NSSTRING_STATUS_ROM_SAVE_IMPORT_FAILED];
	}
	
	[self restoreCoreState];
}

- (IBAction) exportRomSave:(id)sender
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setTitle:NSSTRING_TITLE_EXPORT_ROM_SAVE_PANEL];
	[panel setCanCreateDirectories:YES];
	[panel setAccessoryView:exportRomSavePanelAccessoryView];
	
	[self pauseCore];
	
	const NSInteger buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		NSURL *romSaveURL = [CocoaDSFile fileURLFromRomURL:[[self currentRom] fileURL] toKind:@"ROM Save"];
		if (romSaveURL != nil)
		{
			const BOOL isRomSaveExported = [CocoaDSFile exportRomSaveToURL:[panel URL] romSaveURL:romSaveURL fileType:[self selectedExportRomSaveID]];
			[self setStatusText:(isRomSaveExported) ? NSSTRING_STATUS_ROM_SAVE_EXPORTED : NSSTRING_STATUS_ROM_SAVE_EXPORT_FAILED];
		}
	}
	
	[self restoreCoreState];
}

- (IBAction) toggleExecutePause:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) coreExecute:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) corePause:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) frameAdvance:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) frameJump:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) reset:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) toggleSpeedLimiter:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) toggleAutoFrameSkip:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) toggleGPUState:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) toggleGDBStubActivate:(id)sender
{
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	const BOOL willStart = ([cdsCore isGdbStubStarted]) ? NO : YES;
	[cdsCore setIsGdbStubStarted:willStart];
	[(NSButton *)sender setTitle:([cdsCore isGdbStubStarted]) ? @"Stop" : @"Start"];
}

- (IBAction) changeRomSaveType:(id)sender
{
	const NSInteger saveTypeID = [CocoaDSUtil getIBActionSenderTag:sender];
	[self setSelectedRomSaveTypeID:saveTypeID];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore changeRomSaveType:saveTypeID];
}

- (IBAction) toggleCheats:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) changeCoreSpeed:(id)sender
{
	if ([sender isKindOfClass:[NSSlider class]])
	{
		lastSetSpeedScalar = [(NSSlider *)sender floatValue];
	}
	else
	{
		const CGFloat newSpeedScalar = (CGFloat)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0f;
		[self changeCoreSpeedWithDouble:newSpeedScalar];
	}
	
	[self setVerticalSyncForNonLayerBackedViews:sender];
}

- (IBAction) changeFirmwareSettings:(id)sender
{
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsFirmware] applySettings];
	[cdsCore updateFirmwareMACAddressString];
}

- (IBAction) changeHardwareMicGain:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] setHardwareMicGain:([self currentMicGainValue]/100.0f)];
}

- (IBAction) changeHardwareMicMute:(id)sender
{
	const BOOL muteState = [CocoaDSUtil getIBActionSenderButtonStateBool:sender];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] setHardwareMicMute:muteState];
	[self updateMicStatusIcon];
}

- (IBAction) changeVolume:(id)sender
{
	const float vol = [self currentVolumeValue];
	[self setCurrentVolumeValue:vol];
	[[self cdsSpeaker] setVolume:vol];
}

- (IBAction) changeAudioEngine:(id)sender
{
	[[self cdsSpeaker] setAudioOutputEngine:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeSpuAdvancedLogic:(id)sender
{
	[[self cdsSpeaker] setSpuAdvancedLogic:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) changeSpuInterpolationMode:(id)sender
{
	[[self cdsSpeaker] setSpuInterpolationMode:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeSpuSyncMode:(id)sender
{
	[[self cdsSpeaker] setSpuSyncMode:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeSpuSyncMethod:(id)sender
{
	[[self cdsSpeaker] setSpuSyncMethod:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) toggleAllDisplays:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) autoholdSet:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) autoholdClear:(id)sender
{
	[inputManager dispatchCommandUsingIBAction:_cmd sender:sender];
}

- (IBAction) setVerticalSyncForNonLayerBackedViews:(id)sender
{
	if ( ([[(DisplayWindowController *)[windowList objectAtIndex:0] view] layer] == nil) && ([windowList count] > 0) )
	{
		CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
		const BOOL useVerticalSync = [cdsCore isFrameSkipEnabled] || (([cdsCore speedScalar] < 1.03f) && [cdsCore isSpeedLimitEnabled]);
		
		for (DisplayWindowController *windowController in windowList)
		{
			[[windowController view] setUseVerticalSync:useVerticalSync];
		}
	}
}

- (IBAction) chooseSlot1R4Directory:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:YES];
	[panel setCanChooseFiles:NO];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:@"Select R4 Directory"];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel beginSheetModalForWindow:slot1ManagerWindow
				  completionHandler:^(NSInteger result) {
					  [self didEndChooseSlot1R4Directory:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:nil
				   modalForWindow:slot1ManagerWindow
					modalDelegate:self
				   didEndSelector:@selector(didEndChooseSlot1R4Directory:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (IBAction) slot1Eject:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore slot1Eject];
}

- (IBAction) simulateEmulationCrash:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setMasterExecute:NO];
}

- (IBAction) generateFirmwareMACAddress:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore generateFirmwareMACAddress];
}

- (IBAction) writeDefaults3DRenderingSettings:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[[cdsCore cdsGPU] render3DRenderingEngine] forKey:@"Render3D_RenderingEngine"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DHighPrecisionColorInterpolation] forKey:@"Render3D_HighPrecisionColorInterpolation"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DEdgeMarking] forKey:@"Render3D_EdgeMarking"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DFog] forKey:@"Render3D_Fog"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DTextures] forKey:@"Render3D_Textures"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[cdsCore cdsGPU] render3DThreads] forKey:@"Render3D_Threads"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DLineHack] forKey:@"Render3D_LineHack"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[cdsCore cdsGPU] render3DMultisampleSize] forKey:@"Render3D_MultisampleSize"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[cdsCore cdsGPU] render3DTextureScalingFactor] forKey:@"Render3D_TextureScalingFactor"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DTextureDeposterize] forKey:@"Render3D_TextureDeposterize"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DTextureSmoothing] forKey:@"Render3D_TextureSmoothing"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] render3DFragmentSamplingHack] forKey:@"Render3D_FragmentSamplingHack"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[cdsCore cdsGPU] gpuScale] forKey:@"Render3D_ScalingFactor"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[cdsCore cdsGPU] gpuColorFormat] forKey:@"Render3D_ColorFormat"];
	
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] openGLEmulateShadowPolygon] forKey:@"Render3D_OpenGL_EmulateShadowPolygon"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] openGLEmulateSpecialZeroAlphaBlending] forKey:@"Render3D_OpenGL_EmulateSpecialZeroAlphaBlending"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] openGLEmulateNDSDepthCalculation] forKey:@"Render3D_OpenGL_EmulateNDSDepthCalculation"];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsGPU] openGLEmulateDepthLEqualPolygonFacing] forKey:@"Render3D_OpenGL_EmulateDepthLEqualPolygonFacing"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsEmulationSettings:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	CocoaDSFirmware *writeFirmware = (CocoaDSFirmware *)[firmwarePanelController content];
	
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	// Saving the user defaults also applies the current firmware settings.
	[writeFirmware applySettings];
	
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagAdvancedBusLevelTiming] forKey:@"Emulation_AdvancedBusLevelTiming"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagRigorousTiming] forKey:@"Emulation_RigorousTiming"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagUseGameSpecificHacks] forKey:@"Emulation_UseGameSpecificHacks"];
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsCore cpuEmulationEngine] forKey:@"Emulation_CPUEmulationEngine"];
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsCore maxJITBlockSize] forKey:@"Emulation_MaxJITBlockSize"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagUseExternalBios] forKey:@"Emulation_UseExternalBIOSImages"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagEmulateBiosInterrupts] forKey:@"Emulation_BIOSEmulateSWI"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagPatchDelayLoop] forKey:@"Emulation_BIOSPatchDelayLoopSWI"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagUseExternalFirmware] forKey:@"Emulation_UseExternalFirmwareImage"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagFirmwareBoot] forKey:@"Emulation_FirmwareBoot"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagEmulateEnsata] forKey:@"Emulation_EmulateEnsata"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagDebugConsole] forKey:@"Emulation_UseDebugConsole"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware firmwareMACAddressValue] forKey:@"FirmwareConfig_FirmwareMACAddress"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware customMACAddressValue] forKey:@"FirmwareConfig_CustomMACAddress"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP1_1] forKey:@"FirmwareConfig_IPv4Address_AP1_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP1_2] forKey:@"FirmwareConfig_IPv4Address_AP1_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP1_3] forKey:@"FirmwareConfig_IPv4Address_AP1_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP1_4] forKey:@"FirmwareConfig_IPv4Address_AP1_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP1_1] forKey:@"FirmwareConfig_IPv4Gateway_AP1_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP1_2] forKey:@"FirmwareConfig_IPv4Gateway_AP1_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP1_3] forKey:@"FirmwareConfig_IPv4Gateway_AP1_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP1_4] forKey:@"FirmwareConfig_IPv4Gateway_AP1_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP1_1] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP1_2] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP1_3] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP1_4] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP1_1] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP1_2] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP1_3] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP1_4] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware subnetMask_AP1] forKey:@"FirmwareConfig_SubnetMask_AP1"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP2_1] forKey:@"FirmwareConfig_IPv4Address_AP2_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP2_2] forKey:@"FirmwareConfig_IPv4Address_AP2_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP2_3] forKey:@"FirmwareConfig_IPv4Address_AP2_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP2_4] forKey:@"FirmwareConfig_IPv4Address_AP2_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP2_1] forKey:@"FirmwareConfig_IPv4Gateway_AP2_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP2_2] forKey:@"FirmwareConfig_IPv4Gateway_AP2_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP2_3] forKey:@"FirmwareConfig_IPv4Gateway_AP2_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP2_4] forKey:@"FirmwareConfig_IPv4Gateway_AP2_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP2_1] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP2_2] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP2_3] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP2_4] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP2_1] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP2_2] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP2_3] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP2_4] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware subnetMask_AP2] forKey:@"FirmwareConfig_SubnetMask_AP2"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP3_1] forKey:@"FirmwareConfig_IPv4Address_AP3_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP3_2] forKey:@"FirmwareConfig_IPv4Address_AP3_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP3_3] forKey:@"FirmwareConfig_IPv4Address_AP3_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Address_AP3_4] forKey:@"FirmwareConfig_IPv4Address_AP3_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP3_1] forKey:@"FirmwareConfig_IPv4Gateway_AP3_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP3_2] forKey:@"FirmwareConfig_IPv4Gateway_AP3_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP3_3] forKey:@"FirmwareConfig_IPv4Gateway_AP3_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4Gateway_AP3_4] forKey:@"FirmwareConfig_IPv4Gateway_AP3_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP3_1] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP3_2] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP3_3] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4PrimaryDNS_AP3_4] forKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP3_1] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_1"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP3_2] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_2"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP3_3] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_3"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware ipv4SecondaryDNS_AP3_4] forKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_4"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware subnetMask_AP3] forKey:@"FirmwareConfig_SubnetMask_AP3"];
	
	//[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware consoleType] forKey:@"FirmwareConfig_ConsoleType"];
	[[NSUserDefaults standardUserDefaults] setObject:[writeFirmware nickname] forKey:@"FirmwareConfig_Nickname"];
	[[NSUserDefaults standardUserDefaults] setObject:[writeFirmware message] forKey:@"FirmwareConfig_Message"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware favoriteColor] forKey:@"FirmwareConfig_FavoriteColor"];
	[[NSUserDefaults standardUserDefaults] setObject:[writeFirmware birthday] forKey:@"FirmwareConfig_Birthday"];
	[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware language] forKey:@"FirmwareConfig_Language"];
	//[[NSUserDefaults standardUserDefaults] setInteger:[writeFirmware backlightLevel] forKey:@"FirmwareConfig_BacklightLevel"];
	
	[prefWindowDelegate updateFirmwareMACAddressString:nil];
	[prefWindowDelegate updateSubnetMaskString_AP1:nil];
	[prefWindowDelegate updateSubnetMaskString_AP2:nil];
	[prefWindowDelegate updateSubnetMaskString_AP3:nil];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsSlot1Settings:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsCore slot1DeviceType] forKey:@"EmulationSlot1_DeviceType"];
	[[NSUserDefaults standardUserDefaults] setObject:[[cdsCore slot1R4URL] path] forKey:@"EmulationSlot1_R4StoragePath"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsSoundSettings:(id)sender
{
	NSMutableDictionary *speakerBindings = (NSMutableDictionary *)[cdsSoundController content];
	
	[[NSUserDefaults standardUserDefaults] setFloat:[[speakerBindings valueForKey:@"volume"] floatValue] forKey:@"Sound_Volume"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[speakerBindings valueForKey:@"audioOutputEngine"] integerValue] forKey:@"Sound_AudioOutputEngine"];
	[[NSUserDefaults standardUserDefaults] setBool:[[speakerBindings valueForKey:@"spuAdvancedLogic"] boolValue] forKey:@"SPU_AdvancedLogic"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[speakerBindings valueForKey:@"spuInterpolationMode"] integerValue] forKey:@"SPU_InterpolationMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[speakerBindings valueForKey:@"spuSyncMode"] integerValue] forKey:@"SPU_SyncMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[speakerBindings valueForKey:@"spuSyncMethod"] integerValue] forKey:@"SPU_SyncMethod"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) writeDefaultsStylusSettings:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	CocoaDSController *cdsController = [cdsCore cdsController];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsController stylusPressure] forKey:@"Emulation_StylusPressure"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (IBAction) closeSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	const NSInteger code = [(NSControl *)sender tag];
	
    [NSApp endSheet:sheet returnCode:code];
}

#pragma mark Class Methods

- (void) cmdUpdateDSController:(const ClientCommandAttributes &)cmdAttr
{
	const BOOL theState = (cmdAttr.input.state == ClientInputDeviceState_On) ? YES : NO;
	const NSUInteger controlID = cmdAttr.intValue[0];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] setControllerState:theState controlID:controlID];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[[windowController view] cdsVideoOutput] setNDSFrameInfo:[cdsCore execControl]->GetNDSFrameInfo()];
		[[[windowController view] cdsVideoOutput] hudUpdate];
		
		if ([[windowController view] isHUDInputVisible])
		{
			[[windowController view] clientDisplayView]->SetViewNeedsFlush();
		}
	}
}

- (void) cmdUpdateDSControllerWithTurbo:(const ClientCommandAttributes &)cmdAttr
{
	const BOOL theState = (cmdAttr.input.state == ClientInputDeviceState_On) ? YES : NO;
	const NSUInteger controlID = cmdAttr.intValue[0];
	const BOOL isTurboEnabled = (BOOL)cmdAttr.intValue[1];
	const uint32_t turboPattern = (uint32_t)cmdAttr.intValue[2];
	const uint32_t turboPatternLength = (uint32_t)cmdAttr.intValue[3];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] setControllerState:theState controlID:controlID turbo:isTurboEnabled turboPattern:turboPattern turboPatternLength:turboPatternLength];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[[windowController view] cdsVideoOutput] setNDSFrameInfo:[cdsCore execControl]->GetNDSFrameInfo()];
		[[[windowController view] cdsVideoOutput] hudUpdate];
		
		if ([[windowController view] isHUDInputVisible])
		{
			[[windowController view] clientDisplayView]->SetViewNeedsFlush();
		}
	}
}

- (void) cmdUpdateDSTouch:(const ClientCommandAttributes &)cmdAttr
{
	const BOOL theState = (cmdAttr.input.state == ClientInputDeviceState_On) ? YES : NO;
	
	const NSPoint touchLoc = (cmdAttr.useInputForIntCoord) ? NSMakePoint(cmdAttr.input.intCoordX, cmdAttr.input.intCoordY) : NSMakePoint(cmdAttr.intValue[1], cmdAttr.intValue[2]);
	if (touchLoc.x >= 0.0 && touchLoc.y >= 0.0)
	{
		CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
		[[cdsCore cdsController] setTouchState:theState location:touchLoc];
		
		for (DisplayWindowController *windowController in windowList)
		{
			[[[windowController view] cdsVideoOutput] setNDSFrameInfo:[cdsCore execControl]->GetNDSFrameInfo()];
			[[[windowController view] cdsVideoOutput] hudUpdate];
			
			if ([[windowController view] isHUDInputVisible])
			{
				[[windowController view] clientDisplayView]->SetViewNeedsFlush();
			}
		}
	}
}

- (void) cmdUpdateDSMicrophone:(const ClientCommandAttributes &)cmdAttr
{
	const BOOL theState = (cmdAttr.input.state == ClientInputDeviceState_On) ? YES : NO;
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	CocoaDSController *cdsController = [cdsCore cdsController];
	
	const NSInteger micMode = cmdAttr.intValue[1];
	[cdsController setSoftwareMicState:theState mode:micMode];
	
	const float sineWaveFrequency = cmdAttr.floatValue[0];
	[cdsController setSineWaveGeneratorFrequency:sineWaveFrequency];
	
	NSString *audioFilePath = (NSString *)cmdAttr.object[0];
	[cdsController setSelectedAudioFileGenerator:[inputManager audioFileGeneratorFromFilePath:audioFilePath]];
}

- (void) cmdUpdateDSPaddle:(const ClientCommandAttributes &)cmdAttr
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if (cmdAttr.input.isAnalog)
	{
		const float paddleSensitivity = cmdAttr.floatValue[0];
		const float paddleScalar = cmdAttr.input.scalar;
		float paddleAdjust = (paddleScalar * 2.0f) - 1.0f;
		
		// Clamp the paddle value.
		if (paddleAdjust < -1.0f)
		{
			paddleAdjust = -1.0f;
		}
		
		if (paddleAdjust > 1.0f)
		{
			paddleAdjust = 1.0f;
		}
		
		// Normalize the input value for the paddle.
		paddleAdjust *= paddleSensitivity;
		[[cdsCore cdsController] setPaddleAdjust:(NSInteger)(paddleAdjust + 0.5f)];
	}
	else
	{
		const BOOL theState = (cmdAttr.input.state == ClientInputDeviceState_On) ? YES : NO;
		const NSInteger paddleAdjust = (theState) ? cmdAttr.intValue[1] : 0;
		[[cdsCore cdsController] setPaddleAdjust:paddleAdjust];
	}
}

- (void) cmdAutoholdSet:(const ClientCommandAttributes &)cmdAttr
{
	const BOOL theState = (cmdAttr.input.state == ClientInputDeviceState_On) ? YES : NO;
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] setAutohold:theState];
	[self setStatusText:(theState) ? NSSTRING_STATUS_AUTOHOLD_SET : NSSTRING_STATUS_AUTOHOLD_SET_RELEASE];
}

- (void) cmdAutoholdClear:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] clearAutohold];
	[self setStatusText:NSSTRING_STATUS_AUTOHOLD_CLEAR];
	
}

- (void) cmdLoadEmuSaveStateSlot:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	NSString *saveStatePath = [[CocoaDSFile saveStateURL] path];
	if (saveStatePath == nil)
	{
		// Should throw an error message here...
		return;
	}
	
	const NSInteger slotNumber = (cmdAttr.useInputForObject) ? [CocoaDSUtil getIBActionSenderTag:(id)cmdAttr.input.object] : cmdAttr.intValue[0];
	if (slotNumber < 0 || slotNumber > MAX_SAVESTATE_SLOTS)
	{
		return;
	}
	
	NSURL *currentRomURL = [[self currentRom] fileURL];
	NSString *fileName = [CocoaDSFile saveSlotFileName:currentRomURL slotNumber:(NSUInteger)(slotNumber + 1)];
	if (fileName == nil)
	{
		return;
	}
	
	[self pauseCore];
	
	const BOOL isStateLoaded = [CocoaDSFile loadState:[NSURL fileURLWithPath:[saveStatePath stringByAppendingPathComponent:fileName]]];
	[self setStatusText:(isStateLoaded) ? NSSTRING_STATUS_SAVESTATE_LOADED : NSSTRING_STATUS_SAVESTATE_LOADING_FAILED];
	
	[self restoreCoreState];
}

- (void) cmdSaveEmuSaveStateSlot:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	NSString *saveStatePath = [[CocoaDSFile saveStateURL] path];
	if (saveStatePath == nil)
	{
		[self setStatusText:NSSTRING_STATUS_CANNOT_GENERATE_SAVE_PATH];
		return;
	}
	
	const BOOL isDirectoryCreated = [CocoaDSFile createUserAppSupportDirectory:@"States"];
	if (!isDirectoryCreated)
	{
		[self setStatusText:NSSTRING_STATUS_CANNOT_CREATE_SAVE_DIRECTORY];
		return;
	}
	
	const NSInteger slotNumber = (cmdAttr.useInputForObject) ? [CocoaDSUtil getIBActionSenderTag:(id)cmdAttr.input.object] : cmdAttr.intValue[0];
	if (slotNumber < 0 || slotNumber > MAX_SAVESTATE_SLOTS)
	{
		return;
	}
	
	NSURL *currentRomURL = [[self currentRom] fileURL];
	NSString *fileName = [CocoaDSFile saveSlotFileName:currentRomURL slotNumber:(NSUInteger)(slotNumber + 1)];
	if (fileName == nil)
	{
		return;
	}
	
	[self pauseCore];
	
	const BOOL isStateSaved = [CocoaDSFile saveState:[NSURL fileURLWithPath:[saveStatePath stringByAppendingPathComponent:fileName]]];
	[self setStatusText:(isStateSaved) ? NSSTRING_STATUS_SAVESTATE_SAVED : NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
	
	[self restoreCoreState];
}

- (void) cmdCopyScreen:(const ClientCommandAttributes &)cmdAttr
{
	[mainWindow copy:nil];
}

- (void) cmdRotateDisplayRelative:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	const double relativeDegrees = (cmdAttr.useInputForObject) ? (double)[CocoaDSUtil getIBActionSenderTag:(id)cmdAttr.input.object] : (double)cmdAttr.intValue[0];
	const double angleDegrees = [mainWindow displayRotation] + relativeDegrees;
	[mainWindow setDisplayRotation:angleDegrees];
}

- (void) cmdToggleAllDisplays:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	for (DisplayWindowController *theWindow in windowList)
	{
		const NSInteger displayMode = [theWindow displayMode];
		switch (displayMode)
		{
			case ClientDisplayMode_Main:
				[theWindow setDisplayMode:ClientDisplayMode_Touch];
				break;
				
			case ClientDisplayMode_Touch:
				[theWindow setDisplayMode:ClientDisplayMode_Main];
				break;
				
			case ClientDisplayMode_Dual:
			{
				const NSInteger displayOrder = [theWindow displayOrder];
				if (displayOrder == ClientDisplayOrder_MainFirst)
				{
					[theWindow setDisplayOrder:ClientDisplayOrder_TouchFirst];
				}
				else
				{
					[theWindow setDisplayOrder:ClientDisplayOrder_MainFirst];
				}
				break;
			}
				
			default:
				break;
		}
	}
}

- (void) cmdHoldToggleSpeedScalar:(const ClientCommandAttributes &)cmdAttr
{
	const float inputSpeedScalar = (cmdAttr.useInputForScalar) ? cmdAttr.input.scalar : cmdAttr.floatValue[0];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[cdsCore setSpeedScalar:(cmdAttr.input.state == ClientInputDeviceState_Off) ? lastSetSpeedScalar : inputSpeedScalar];
	[self setVerticalSyncForNonLayerBackedViews:nil];
}

- (void) cmdToggleSpeedLimiter:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore isSpeedLimitEnabled])
	{
		[cdsCore setIsSpeedLimitEnabled:NO];
		[self setStatusText:NSSTRING_STATUS_SPEED_LIMIT_DISABLED];
		[[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"CoreControl_EnableSpeedLimit"];
	}
	else
	{
		[cdsCore setIsSpeedLimitEnabled:YES];
		[self setStatusText:NSSTRING_STATUS_SPEED_LIMIT_ENABLED];
		[[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"CoreControl_EnableSpeedLimit"];
	}
	
	[self setVerticalSyncForNonLayerBackedViews:nil];
}

- (void) cmdToggleAutoFrameSkip:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore isFrameSkipEnabled])
	{
		[cdsCore setIsFrameSkipEnabled:NO];
		[self setStatusText:NSSTRING_STATUS_AUTO_FRAME_SKIP_DISABLED];
		[[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"CoreControl_EnableAutoFrameSkip"];
	}
	else
	{
		[cdsCore setIsFrameSkipEnabled:YES];
		[self setStatusText:NSSTRING_STATUS_AUTO_FRAME_SKIP_ENABLED];
		[[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"CoreControl_EnableAutoFrameSkip"];
	}
	
	[self setVerticalSyncForNonLayerBackedViews:nil];
}

- (void) cmdToggleCheats:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore isCheatingEnabled])
	{
		[cdsCore setIsCheatingEnabled:NO];
		[self setStatusText:NSSTRING_STATUS_CHEATS_DISABLED];
		[[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"CoreControl_EnableCheats"];
	}
	else
	{
		[cdsCore setIsCheatingEnabled:YES];
		[self setStatusText:NSSTRING_STATUS_CHEATS_ENABLED];
		[[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"CoreControl_EnableCheats"];
	}
}

- (void) cmdToggleExecutePause:(const ClientCommandAttributes &)cmdAttr
{
	if ( (cmdAttr.input.state == ClientInputDeviceState_Off) || ([self currentRom] == nil) )
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore coreState] == ExecutionBehavior_Pause)
	{
		[self executeCore];
	}
	else
	{
		[self pauseCore];
	}
}

- (void) cmdCoreExecute:(const ClientCommandAttributes &)cmdAttr
{
	if ( (cmdAttr.input.state == ClientInputDeviceState_Off) || ([self currentRom] == nil) )
	{
		return;
	}
	
	[self executeCore];
}

- (void) cmdCorePause:(const ClientCommandAttributes &)cmdAttr
{
	if ( (cmdAttr.input.state == ClientInputDeviceState_Off) || ([self currentRom] == nil) )
	{
		return;
	}
	
	[self pauseCore];
}

- (void) cmdFrameAdvance:(const ClientCommandAttributes &)cmdAttr
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ( (cmdAttr.input.state == ClientInputDeviceState_Off) ||
	     ([cdsCore coreState] != ExecutionBehavior_Pause) ||
	     ([self currentRom] == nil) )
	{
		return;
	}
	
	[cdsCore setCoreState:ExecutionBehavior_FrameAdvance];
}

- (void) cmdFrameJump:(const ClientCommandAttributes &)cmdAttr
{
	if ( (cmdAttr.input.state == ClientInputDeviceState_Off) || ([self currentRom] == nil) )
	{
		return;
	}
	
	[executionControlWindow makeFirstResponder:nil];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCoreState:ExecutionBehavior_FrameJump];
}

- (void) cmdReset:(const ClientCommandAttributes &)cmdAttr
{
	if ( (cmdAttr.input.state == ClientInputDeviceState_Off) || ([self currentRom] == nil) )
	{
		return;
	}
	
	[self setStatusText:NSSTRING_STATUS_EMULATOR_RESETTING];
	[self setIsWorking:YES];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] displayIfNeeded];
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore reset];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[[windowController view] cdsVideoOutput] signalMessage:MESSAGE_RELOAD_REPROCESS_REDRAW];
	}
	
	[self setStatusText:NSSTRING_STATUS_EMULATOR_RESET];
	[self setIsWorking:NO];
	[self writeDefaultsSlot1Settings:nil];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] displayIfNeeded];
	}
}

- (void) cmdToggleMute:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	float vol = 0.0f;
	
	if (isSoundMuted)
	{
		isSoundMuted = NO;
		vol = lastSetVolumeValue;
		[self setStatusText:@"Sound unmuted."];
	}
	else
	{
		isSoundMuted = YES;
		[self setStatusText:@"Sound muted."];
	}

	[self setCurrentVolumeValue:vol];
	[[self cdsSpeaker] setVolume:vol];
}

- (void) cmdToggleGPUState:(const ClientCommandAttributes &)cmdAttr
{
	if (cmdAttr.input.state == ClientInputDeviceState_Off)
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	const NSInteger bitNumber = (cmdAttr.useInputForObject) ? [CocoaDSUtil getIBActionSenderTag:(id)cmdAttr.input.object] : cmdAttr.intValue[0];
	const UInt32 flagBit = [cdsCore.cdsGPU gpuStateFlags] ^ (1 << bitNumber);
	
	[cdsCore.cdsGPU setGpuStateFlags:flagBit];
}

- (BOOL) handleLoadRomByURL:(NSURL *)fileURL
{
	BOOL result = NO;
	
	if (fileURL == nil || [self isRomLoading])
	{
		return result;
	}
	
	if ([self currentRom] != nil)
	{
		const BOOL closeResult = [self handleUnloadRom:REASONFORCLOSE_OPEN romToLoad:fileURL];
		if ([self isShowingSaveStateDialog])
		{
			return result;
		}
		
		if (![self isShowingFileMigrationDialog] && !closeResult)
		{
			return result;
		}
	}
	
	// Check for the v0.9.7 ROM Save File
	if ([CocoaDSFile romSaveExistsWithRom:fileURL] && ![CocoaDSFile romSaveExists:fileURL])
	{
		[fileURL retain];
		[self setIsUserInterfaceBlockingExecution:YES];
		[self setIsShowingFileMigrationDialog:YES];
		
		[NSApp beginSheet:saveFileMigrationSheet
		   modalForWindow:[[windowList objectAtIndex:0] window]
            modalDelegate:self
		   didEndSelector:@selector(didEndFileMigrationSheet:returnCode:contextInfo:)
			  contextInfo:fileURL];
	}
	else
	{
		result = [self loadRomByURL:fileURL asynchronous:YES];
	}
	
	return result;
}

- (BOOL) handleUnloadRom:(NSInteger)reasonID romToLoad:(NSURL *)romURL
{
	BOOL result = NO;
	
	if ([self isRomLoading] || [self currentRom] == nil)
	{
		return result;
	}
	
	[self pauseCore];
	
	if (isSaveStateEdited && [self currentSaveStateURL] != nil)
	{
		SEL endSheetSelector = @selector(didEndSaveStateSheet:returnCode:contextInfo:);
		
		switch (reasonID)
		{
			case REASONFORCLOSE_OPEN:
				[romURL retain];
				endSheetSelector = @selector(didEndSaveStateSheetOpen:returnCode:contextInfo:);
				break;	
				
			case REASONFORCLOSE_TERMINATE:
				endSheetSelector = @selector(didEndSaveStateSheetTerminate:returnCode:contextInfo:);
				break;
				
			default:
				break;
		}
		
		[self setIsUserInterfaceBlockingExecution:YES];
		[self setIsShowingSaveStateDialog:YES];
		
		[NSApp beginSheet:saveStatePrecloseSheet
		   modalForWindow:(NSWindow *)[[windowList objectAtIndex:0] window]
            modalDelegate:self
		   didEndSelector:endSheetSelector
			  contextInfo:romURL];
	}
	else
	{
		result = [self unloadRom];
	}
	
	return result;
}

- (BOOL) loadRomByURL:(NSURL *)romURL asynchronous:(BOOL)willLoadAsync
{
	BOOL result = NO;
	
	if (romURL == nil)
	{
		return result;
	}
	
	[self setStatusText:NSSTRING_STATUS_ROM_LOADING];
	[self setIsWorking:YES];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] displayIfNeeded];
	}
	
	// Need to pause the core before loading the ROM.
	[self pauseCore];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	if (![cdsCore emuFlagUseExternalBios] || ![cdsCore emuFlagUseExternalFirmware])
	{
		[[cdsCore cdsFirmware] updateFirmwareConfigSessionValues];
	}
	
	[cdsCore execControl]->ApplySettingsOnReset();
	[cdsCore updateSlot1DeviceStatus];
	[self writeDefaultsSlot1Settings:nil];
	
	CocoaDSRom *newRom = [[CocoaDSRom alloc] init];
	if (newRom != nil)
	{
		[self setIsRomLoading:YES];
		[romURL retain];
		[newRom setSaveType:selectedRomSaveTypeID];
		[newRom setWillStreamLoadData:[[NSUserDefaults standardUserDefaults] boolForKey:@"General_StreamLoadRomData"]];
		
		if (willLoadAsync)
		{
			[NSThread detachNewThreadSelector:@selector(loadDataOnThread:) toTarget:newRom withObject:romURL];
		}
		else
		{
			[newRom loadData:romURL];
		}
		
		[romURL release];
	}
	
	result = YES;
	
	return result;
}

- (void) loadRomDidFinish:(NSNotification *)aNotification
{
	CocoaDSRom *theRom = [aNotification object];
	NSDictionary *userInfo = [aNotification userInfo];
	const BOOL didLoad = [(NSNumber *)[userInfo valueForKey:@"DidLoad"] boolValue];
	
	if ( theRom == nil || !didLoad || (![theRom willStreamLoadData] && ![theRom isDataLoaded]) )
	{
		// If ROM loading fails, restore the core state, but only if a ROM is already loaded.
		if([self currentRom] != nil)
		{
			[self restoreCoreState];
		}
		
		[self setStatusText:NSSTRING_STATUS_ROM_LOADING_FAILED];
		[self setIsWorking:NO];
		[self setIsRomLoading:NO];
		
		return;
	}
	
	// Set the core's ROM to our newly allocated ROM object.
	[self setCurrentRom:theRom];
	[romInfoPanelController setContent:[theRom bindings]];
	
	// If the ROM has an associated cheat file, load it now.
	NSString *cheatsPath = [[CocoaDSFile fileURLFromRomURL:[theRom fileURL] toKind:@"Cheat"] path];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	CocoaDSCheatManager *newCheatList = [[[CocoaDSCheatManager alloc] initWithFileURL:[NSURL fileURLWithPath:cheatsPath]] autorelease];
	if (newCheatList != nil)
	{
		NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
		
		[CocoaDSCheatManager setMasterCheatList:newCheatList];
		[cheatListController setContent:[newCheatList list]];
		[self setCdsCheats:newCheatList];
		[cheatWindowBindings setValue:newCheatList forKey:@"cheatList"];
		
		NSString *filePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"R4Cheat_DatabasePath"];
		if (filePath != nil)
		{
			NSURL *fileURL = [NSURL fileURLWithPath:filePath];
			NSInteger error = 0;
			NSMutableArray *dbList = [[self cdsCheats] cheatListFromDatabase:fileURL errorCode:&error];
			if (dbList != nil)
			{
				[cheatDatabaseController setContent:dbList];
				
				NSString *titleString = [[self cdsCheats] dbTitle];
				NSString *dateString = [[self cdsCheats] dbDate];
				
				[cheatWindowBindings setValue:titleString forKey:@"cheatDBTitle"];
				[cheatWindowBindings setValue:dateString forKey:@"cheatDBDate"];
				[cheatWindowBindings setValue:[NSString stringWithFormat:@"%ld", (unsigned long)[dbList count]] forKey:@"cheatDBItemCount"];
			}
			else
			{
				[cheatWindowBindings setValue:@"---" forKey:@"cheatDBItemCount"];
				
				switch (error)
				{
					case CHEATEXPORT_ERROR_FILE_NOT_FOUND:
						NSLog(@"R4 Cheat Database read failed! Could not load the database file!");
						[cheatWindowBindings setValue:@"Database not loaded." forKey:@"cheatDBTitle"];
						[cheatWindowBindings setValue:@"CANNOT LOAD FILE" forKey:@"cheatDBDate"];
						break;
						
					case CHEATEXPORT_ERROR_WRONG_FILE_FORMAT:
						NSLog(@"R4 Cheat Database read failed! Wrong file format!");
						[cheatWindowBindings setValue:@"Database load error." forKey:@"cheatDBTitle"];
						[cheatWindowBindings setValue:@"FAILED TO LOAD FILE" forKey:@"cheatDBDate"];
						break;
						
					case CHEATEXPORT_ERROR_SERIAL_NOT_FOUND:
						NSLog(@"R4 Cheat Database read failed! Could not find the serial number for this game in the database!");
						[cheatWindowBindings setValue:@"ROM not found in database." forKey:@"cheatDBTitle"];
						[cheatWindowBindings setValue:@"ROM not found." forKey:@"cheatDBDate"];
						break;
						
					case CHEATEXPORT_ERROR_EXPORT_FAILED:
						NSLog(@"R4 Cheat Database read failed! Could not read the database file!");
						[cheatWindowBindings setValue:@"Database read error." forKey:@"cheatDBTitle"];
						[cheatWindowBindings setValue:@"CANNOT READ FILE" forKey:@"cheatDBDate"];
						break;
						
					default:
						break;
				}
			}
		}
		
		[cheatWindowDelegate setCdsCheats:newCheatList];
		[[cheatWindowDelegate cdsCheatSearch] setRwlockCoreExecute:[cdsCore rwlockCoreExecute]];
		[cheatWindowDelegate setCheatSearchViewByStyle:CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE];
	}
	
	// Add the last loaded ROM to the Recent ROMs list.
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[theRom fileURL]];
	
	// Update the UI to indicate that a ROM has indeed been loaded.
	[self updateAllWindowTitles];
	[screenshotCaptureToolDelegate setRomName:[currentRom internalName]];
	[avCaptureToolDelegate setRomName:[currentRom internalName]];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[[windowController view] cdsVideoOutput] signalMessage:MESSAGE_RELOAD_REPROCESS_REDRAW];
	}
	
	[self setStatusText:NSSTRING_STATUS_ROM_LOADED];
	[self setIsWorking:NO];
	[self setIsRomLoading:NO];
	
	Slot2WindowDelegate *slot2WindowDelegate = (Slot2WindowDelegate *)[slot2WindowController content];
	[slot2WindowDelegate setAutoSelectedDeviceText:[[slot2WindowDelegate deviceManager] autoSelectedDeviceName]];
	[[slot2WindowDelegate deviceManager] updateStatus];
		
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] displayIfNeeded];
	}
	
	[cdsCore updateCurrentSessionMACAddressString:YES];
	[cdsCore setMasterExecute:YES];
	
	// After the ROM loading is complete, send an execute message to the Cocoa DS per
	// user preferences.
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"General_ExecuteROMOnLoad"])
	{
		[self executeCore];
	}
}

- (BOOL) unloadRom
{
	BOOL result = NO;
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[self setCurrentSaveStateURL:nil];
	
	isSaveStateEdited = NO;
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] setDocumentEdited:isSaveStateEdited];
	}
	
	// Save the ROM's cheat list before unloading.
	[[self cdsCheats] save];
	
	// Update the UI to indicate that the ROM has started the process of unloading.
	[self setStatusText:NSSTRING_STATUS_ROM_UNLOADING];
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	[cheatListController setContent:nil];
	[cheatWindowDelegate resetSearch:nil];
	[cheatWindowDelegate setCdsCheats:nil];
	[cheatDatabaseController setContent:nil];
	
	NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
	[cheatWindowBindings setValue:@"No ROM loaded." forKey:@"cheatDBTitle"];
	[cheatWindowBindings setValue:@"No ROM loaded." forKey:@"cheatDBDate"];
	[cheatWindowBindings setValue:@"---" forKey:@"cheatDBItemCount"];
	[cheatWindowBindings setValue:nil forKey:@"cheatList"];
	
	[self setIsWorking:YES];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] displayIfNeeded];
	}
	
	// Unload the ROM.
	if (![cdsCore emuFlagUseExternalBios] || ![cdsCore emuFlagUseExternalFirmware])
	{
		[[cdsCore cdsFirmware] writeUserDefaultWFCUserID];
	}
	
	[[self currentRom] release];
	[self setCurrentRom:nil];
	
	// Release the current cheat list and assign the empty list.
	[self setCdsCheats:nil];
	if (dummyCheatList == nil)
	{
		dummyCheatList = [[CocoaDSCheatManager alloc] init];
	}
	[CocoaDSCheatManager setMasterCheatList:dummyCheatList];
	
	// Update the UI to indicate that the ROM has finished unloading.
	[self updateAllWindowTitles];
	[screenshotCaptureToolDelegate setRomName:[currentRom internalName]];
	[avCaptureToolDelegate setRomName:[currentRom internalName]];
	
	[[cdsCore cdsGPU] clearWithColor:0x8000];
	for (DisplayWindowController *windowController in windowList)
	{
		[[[windowController view] cdsVideoOutput] signalMessage:MESSAGE_RELOAD_REPROCESS_REDRAW];
	}
	
	[self setStatusText:NSSTRING_STATUS_ROM_UNLOADED];
	[self setIsWorking:NO];
	
	Slot2WindowDelegate *slot2WindowDelegate = (Slot2WindowDelegate *)[slot2WindowController content];
	[slot2WindowDelegate setAutoSelectedDeviceText:[[slot2WindowDelegate deviceManager] autoSelectedDeviceName]];
	[[slot2WindowDelegate deviceManager] updateStatus];
	
	for (DisplayWindowController *windowController in windowList)
	{
		[[windowController window] displayIfNeeded];
	}
	
	[cdsCore setSlot1StatusText:NSSTRING_STATUS_EMULATION_NOT_RUNNING];
	[cdsCore updateCurrentSessionMACAddressString:NO];
	[[cdsCore cdsController] reset];
	
	result = YES;
	
	return result;
}

- (void) handleNDSError:(NSNotification *)aNotification
{
	[self setIsUserInterfaceBlockingExecution:YES];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	NSString *ndsErrorString = [cdsCore errorStatus];
	const char *ndsErrorCString = [ndsErrorString cStringUsingEncoding:NSUTF8StringEncoding];
	size_t lineCount = 1;
	
	for (size_t i = 0; i < [ndsErrorString length]; i++)
	{
		if (ndsErrorCString[i] == '\n')
		{
			lineCount++;
		}
	}
	
	NSRect newSheetFrameRect = [ndsErrorSheet frame];
	newSheetFrameRect.size.height = 98.0f + (16.0f * lineCount);
	
	// For some reason, when the sheet is shown for the very first time,
	// the sheet doesn't drop down as low as it should. However, upon
	// subsequent showings, the sheet drops down to the intended distance.
	// To compensate for this difference, the first sheet showing will
	// have a little added height to it.
	if (!_isNDSErrorSheetAlreadyShown)
	{
		_isNDSErrorSheetAlreadyShown = YES;
		newSheetFrameRect.size.height += 22.0f;
	}
	
	[ndsErrorSheet setFrame:newSheetFrameRect display:NO];
	
	NSRect newTextFieldRect = [ndsErrorStatusTextField frame];
	newTextFieldRect.size.height = 16.0f * lineCount;
	[ndsErrorStatusTextField setFrame:newTextFieldRect];
	
	[NSApp beginSheet:ndsErrorSheet
	   modalForWindow:(NSWindow *)[[windowList objectAtIndex:0] window]
		modalDelegate:self
	   didEndSelector:@selector(didEndErrorSheet:returnCode:contextInfo:)
		  contextInfo:nil];
}

- (void) addOutputToCore:(CocoaDSOutput *)theOutput
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore addOutput:theOutput];
}

- (void) removeOutputFromCore:(CocoaDSOutput *)theOutput
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore removeOutput:theOutput];
}

- (void) changeCoreSpeedWithDouble:(double)newSpeedScalar
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setSpeedScalar:newSpeedScalar];
	lastSetSpeedScalar = newSpeedScalar;
}

- (void) executeCore
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCoreState:ExecutionBehavior_Run];
	[self updateMicStatusIcon];
}

- (void) pauseCore
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCoreState:ExecutionBehavior_Pause];
	[self updateMicStatusIcon];
}

- (void) restoreCoreState
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore restoreCoreState];
	[self updateMicStatusIcon];
}

- (void) updateMicStatusIcon
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	CocoaDSController *cdsController = [cdsCore cdsController];
	NSImage *micIcon = iconMicDisabled;
	
	if ([cdsController softwareMicState])
	{
		micIcon = iconMicManualOverride;
	}
	else
	{
		if ([cdsController isHardwareMicAvailable])
		{
			if ([cdsController hardwareMicPause])
			{
				micIcon = iconMicDisabled;
			}
			else if ([cdsController isHardwareMicInClip])
			{
				micIcon = iconMicInClip;
			}
			else if ([cdsController isHardwareMicIdle])
			{
				micIcon = iconMicIdle;
			}
			else
			{
				micIcon = iconMicActive;
			}
		}
	}
	
	if (micIcon != [self currentMicStatusIcon])
	{
		[self performSelectorOnMainThread:@selector(setCurrentMicStatusIcon:) withObject:micIcon waitUntilDone:NO];
	}
}

- (AudioSampleBlockGenerator *) selectedAudioFileGenerator
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	return [[cdsCore cdsController] selectedAudioFileGenerator];
}

- (void) setSelectedAudioFileGenerator:(AudioSampleBlockGenerator *)theGenerator
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsController] setSelectedAudioFileGenerator:theGenerator];
}

- (void) didEndFileMigrationSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	NSURL *romURL = (NSURL *)contextInfo;
	NSURL *romSaveURL = [CocoaDSFile romSaveURLFromRomURL:romURL];
	
    [sheet orderOut:self];	
	
	switch (returnCode)
	{
		case NSOKButton:
			[CocoaDSFile moveFileToCurrentDirectory:romSaveURL];
			break;
			
		default:
			break;
	}
	
	[self setIsUserInterfaceBlockingExecution:NO];
	[self setIsShowingFileMigrationDialog:NO];
	[self loadRomByURL:romURL asynchronous:YES];
	
	// We retained this when we initially put up the sheet, so we need to release it now.
	[romURL release];
}

- (void) didEndSaveStateSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	switch (returnCode)
	{
		case NSCancelButton: // Cancel
			[self restoreCoreState];
			[self setIsUserInterfaceBlockingExecution:NO];
			[self setIsShowingSaveStateDialog:NO];
			return;
			break;
			
		case COCOA_DIALOG_DEFAULT: // Save
		{
			const BOOL isStateSaved = [CocoaDSFile saveState:[self currentSaveStateURL]];
			if (!isStateSaved)
			{
				// Throw an error here...
				[self setStatusText:NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
				return;
			}
			break;
		}
			
		case COCOA_DIALOG_OPTION: // Don't Save
			break;
			
		default:
			break;
	}
	
	[self unloadRom];
	[self setIsUserInterfaceBlockingExecution:NO];
	[self setIsShowingSaveStateDialog:NO];
}

- (void) didEndSaveStateSheetOpen:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[self didEndSaveStateSheet:sheet returnCode:returnCode contextInfo:contextInfo];
	
	NSURL *romURL = (NSURL *)contextInfo;
	[self handleLoadRomByURL:romURL];
	[romURL release];
}

- (void) didEndSaveStateSheetTerminate:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[self didEndSaveStateSheet:sheet returnCode:returnCode contextInfo:contextInfo];
	
	if (returnCode == NSCancelButton)
	{
		[NSApp replyToApplicationShouldTerminate:NO];
	}
	else
	{
		if ([self currentSaveStateURL] == nil)
		{
			[NSApp replyToApplicationShouldTerminate:YES];
		}
	}
}

- (void) didEndChooseSlot1R4Directory:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedDirURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedDirURL == nil)
	{
		return;
	}
	
	[[NSUserDefaults standardUserDefaults] setObject:[selectedDirURL path] forKey:@"EmulationSlot1_R4StoragePath"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setSlot1R4URL:selectedDirURL];
}

- (void) didEndErrorSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	[self setIsUserInterfaceBlockingExecution:NO];
	
	switch (returnCode)
	{
		case COCOA_DIALOG_OPTION: // Reset
			[self reset:self];
			break;
			
		case NSCancelButton: // Stop
		default:
			break;
	}
}

- (void) updateAllWindowTitles
{
	if ([windowList count] < 1)
	{
		return;
	}
	
	NSString *romName = nil;
	NSURL *repURL = nil;
	NSImage *titleIcon = nil;
	
	if ([self currentRom] == nil)
	{
		romName = (NSString *)[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
	}
	else
	{
		romName = [currentRom internalName];
		repURL = [currentRom fileURL];
		titleIcon = [currentRom icon];
	}
	
	if ([windowList count] > 1)
	{
		for (DisplayWindowController *windowController in windowList)
		{
			NSString *newWindowTitle = [romName stringByAppendingFormat:@":%ld", (unsigned long)([windowList indexOfObject:windowController] + 1)];
			
			[[windowController masterWindow] setTitle:newWindowTitle];
			[[windowController masterWindow] setRepresentedURL:repURL];
			[[[windowController masterWindow] standardWindowButton:NSWindowDocumentIconButton] setImage:titleIcon];
		}
	}
	else
	{
		NSWindow *theWindow = [[windowList objectAtIndex:0] masterWindow];
		[theWindow setTitle:romName];
		[theWindow setRepresentedURL:repURL];
		[[theWindow standardWindowButton:NSWindowDocumentIconButton] setImage:titleIcon];
	}
}

- (void) updateDisplayPanelTitles
{
	// If the original panel titles haven't been saved yet, then save them now.
	if (_displayRotationPanelTitle == nil)
	{
		_displayRotationPanelTitle = [[displayRotationPanel title] copy];
		_displaySeparationPanelTitle = [[displaySeparationPanel title] copy];
		_displayVideoSettingsPanelTitle = [[displayVideoSettingsPanel title] copy];
		_displayHUDSettingsPanelTitle = [[displayHUDSettingsPanel title] copy];
	}
	
	// Set the panel titles to the window number.
	if ([windowList count] <= 1)
	{
		[displayRotationPanel setTitle:_displayRotationPanelTitle];
		[displaySeparationPanel setTitle:_displaySeparationPanelTitle];
		[displayVideoSettingsPanel setTitle:_displayVideoSettingsPanelTitle];
		[displayHUDSettingsPanel setTitle:_displayHUDSettingsPanelTitle];
	}
	else
	{
		unsigned long windowNumber = (unsigned long)[windowList indexOfObject:mainWindow] + 1;
		[displayRotationPanel setTitle:[_displayRotationPanelTitle stringByAppendingFormat:@":%ld", windowNumber]];
		[displaySeparationPanel setTitle:[_displaySeparationPanelTitle stringByAppendingFormat:@":%ld", windowNumber]];
		[displayVideoSettingsPanel setTitle:[_displayVideoSettingsPanelTitle stringByAppendingFormat:@":%ld", windowNumber]];
		[displayHUDSettingsPanel setTitle:[_displayHUDSettingsPanelTitle stringByAppendingFormat:@":%ld", windowNumber]];
	}
}

- (void) appInit
{
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	[cheatWindowController setContent:[cheatWindowDelegate bindings]];
	
	// Update the SLOT-2 device list.
	Slot2WindowDelegate *slot2WindowDelegate = (Slot2WindowDelegate *)[slot2WindowController content];
	[slot2WindowDelegate update];
	[slot2WindowDelegate setHidManager:[inputManager hidManager]];
	[slot2WindowDelegate setAutoSelectedDeviceText:[[slot2WindowDelegate deviceManager] autoSelectedDeviceName]];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore updateCurrentSessionMACAddressString:NO];
	[screenshotCaptureToolDelegate setSharedData:[[cdsCore cdsGPU] sharedData]];
	[avCaptureToolDelegate setSharedData:[[cdsCore cdsGPU] sharedData]];
	[self fillOpenGLMSAAMenu];
}

- (void) fillOpenGLMSAAMenu
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	NSUInteger maxSamples = [[cdsCore cdsGPU] openglDeviceMaxMultisamples];
	size_t itemCount = 0;
	
	while (maxSamples > 1)
	{
		itemCount++;
		maxSamples >>= 1;
	}
	
	if (itemCount <= 0)
	{
		return;
	}
	
	int itemValue = 2;
	
	for (size_t i = 0; i < itemCount; i++, itemValue<<=1)
	{
		NSString *itemTitle = [NSString stringWithFormat:@"%d", itemValue];
		[openglMSAAPopUpButton addItemWithTitle:itemTitle];
		
		NSMenuItem *menuItem = [openglMSAAPopUpButton itemAtIndex:i+1];
		[menuItem setTag:itemValue];
	}
	
	[openglMSAAPopUpButton selectItemAtIndex:0];
}

- (void) readUserDefaults
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	Slot2WindowDelegate *slot2WindowDelegate = (Slot2WindowDelegate *)[slot2WindowController content];
	
	// Set up the user's default input settings.
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	if (userMappings == nil)
	{
		NSDictionary *defaultKeyMappingsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
		NSArray *internalDefaultProfilesList = (NSArray *)[defaultKeyMappingsDict valueForKey:@"DefaultInputProfiles"];
		userMappings = [(NSDictionary *)[internalDefaultProfilesList objectAtIndex:0] valueForKey:@"Mappings"];
	}
	
	[inputManager setMappingsWithMappings:userMappings];
	[[inputManager hidManager] setDeviceListController:inputDeviceListController];
	[[inputManager hidManager] setRunLoop:[NSRunLoop currentRunLoop]];
	
	// Set the microphone settings per user preferences.
	[[cdsCore cdsController] setHardwareMicMute:[[NSUserDefaults standardUserDefaults] boolForKey:@"Microphone_HardwareMicMute"]];
	
	// Set the SPU settings per user preferences.
	[self setCurrentVolumeValue:[[NSUserDefaults standardUserDefaults] floatForKey:@"Sound_Volume"]];
	[[self cdsSpeaker] setVolume:[[NSUserDefaults standardUserDefaults] floatForKey:@"Sound_Volume"]];
	[[self cdsSpeaker] setAudioOutputEngine:[[NSUserDefaults standardUserDefaults] integerForKey:@"Sound_AudioOutputEngine"]];
	[[self cdsSpeaker] setSpuAdvancedLogic:[[NSUserDefaults standardUserDefaults] boolForKey:@"SPU_AdvancedLogic"]];
	[[self cdsSpeaker] setSpuInterpolationMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_InterpolationMode"]];
	[[self cdsSpeaker] setSpuSyncMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"]];
	[[self cdsSpeaker] setSpuSyncMethod:[[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMethod"]];
	
	// Set the 3D rendering options per user preferences.
	[[cdsCore cdsGPU] setRender3DThreads:(NSUInteger)[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_Threads"]];
	[[cdsCore cdsGPU] setRender3DRenderingEngine:[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_RenderingEngine"]];
	[[cdsCore cdsGPU] setRender3DHighPrecisionColorInterpolation:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_HighPrecisionColorInterpolation"]];
	[[cdsCore cdsGPU] setRender3DEdgeMarking:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_EdgeMarking"]];
	[[cdsCore cdsGPU] setRender3DFog:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_Fog"]];
	[[cdsCore cdsGPU] setRender3DTextures:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_Textures"]];
	[[cdsCore cdsGPU] setRender3DLineHack:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_LineHack"]];
	[[cdsCore cdsGPU] setRender3DMultisampleSize:[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_MultisampleSize"]];
	[[cdsCore cdsGPU] setRender3DTextureScalingFactor:(NSUInteger)[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_TextureScalingFactor"]];
	[[cdsCore cdsGPU] setRender3DTextureDeposterize:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_TextureDeposterize"]];
	[[cdsCore cdsGPU] setRender3DTextureSmoothing:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_TextureSmoothing"]];
	[[cdsCore cdsGPU] setRender3DFragmentSamplingHack:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_FragmentSamplingHack"]];
	[[cdsCore cdsGPU] setGpuScale:(NSUInteger)[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_ScalingFactor"]];
	[[cdsCore cdsGPU] setGpuColorFormat:(NSUInteger)[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_ColorFormat"]];
	
	[[cdsCore cdsGPU] setOpenGLEmulateShadowPolygon:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_OpenGL_EmulateShadowPolygon"]];
	[[cdsCore cdsGPU] setOpenGLEmulateSpecialZeroAlphaBlending:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_OpenGL_EmulateSpecialZeroAlphaBlending"]];
	[[cdsCore cdsGPU] setOpenGLEmulateNDSDepthCalculation:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_OpenGL_EmulateNDSDepthCalculation"]];
	[[cdsCore cdsGPU] setOpenGLEmulateDepthLEqualPolygonFacing:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_OpenGL_EmulateDepthLEqualPolygonFacing"]];
	
	[[[cdsCore cdsGPU] sharedData] fetchSynchronousAtIndex:0];
	
	// Set the stylus options per user preferences.
	[[cdsCore cdsController] setStylusPressure:[[NSUserDefaults standardUserDefaults] integerForKey:@"Emulation_StylusPressure"]];
	
	// Set up the default SLOT-2 device.
	[slot2WindowDelegate setupUserDefaults];
	
	// Set up the ROM Info panel.
	[romInfoPanel setupUserDefaults];
	
	// Set up the capture tool defaults.
	[screenshotCaptureToolDelegate readUserDefaults];
	[avCaptureToolDelegate readUserDefaults];
}

- (void) writeUserDefaults
{
	[romInfoPanel writeDefaults];
	[screenshotCaptureToolDelegate writeUserDefaults];
	[avCaptureToolDelegate writeUserDefaults];
}

- (void) restoreDisplayWindowStates
{
	NSArray *windowPropertiesList = [[NSUserDefaults standardUserDefaults] arrayForKey:@"General_DisplayWindowRestorableStates"];
	const BOOL willRestoreWindows = [[NSUserDefaults standardUserDefaults] boolForKey:@"General_WillRestoreDisplayWindows"];
	
	if (!willRestoreWindows || windowPropertiesList == nil || [windowPropertiesList count] < 1)
	{
		// If no windows were saved for restoring (the user probably closed all windows before
		// app termination), then simply create a new display window per user defaults.
		[self newDisplayWindow:self];
	}
	else
	{
		for (NSDictionary *windowProperties in windowPropertiesList)
		{
			DisplayWindowController *windowController = [[DisplayWindowController alloc] initWithWindowNibName:@"DisplayWindow" emuControlDelegate:self];
			
			if (windowController == nil)
			{
				continue;
			}
			
			const NSInteger displayMode				= ([windowProperties objectForKey:@"displayMode"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayMode"] integerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_Mode"];
			const double displayScale				= ([windowProperties objectForKey:@"displayScale"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayScale"] doubleValue] : ([[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayView_Size"] / 100.0);
			const double displayRotation			= ([windowProperties objectForKey:@"displayRotation"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayRotation"] doubleValue] : [[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayView_Rotation"];
			const NSInteger displayOrientation		= ([windowProperties objectForKey:@"displayOrientation"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayOrientation"] integerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Orientation"];
			const NSInteger displayOrder			= ([windowProperties objectForKey:@"displayOrder"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayOrder"] integerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Order"];
			const double displayGap					= ([windowProperties objectForKey:@"displayGap"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayGap"] doubleValue] : ([[NSUserDefaults standardUserDefaults] doubleForKey:@"DisplayViewCombo_Gap"] / 100.0);
			
			const NSInteger displayMainSource		= ([windowProperties objectForKey:@"displayMainVideoSource"]  != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayMainVideoSource"] integerValue]  : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_DisplayMainVideoSource"];
			const NSInteger displayTouchSource		= ([windowProperties objectForKey:@"displayTouchVideoSource"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"displayTouchVideoSource"] integerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_DisplayTouchVideoSource"];
			
			const BOOL videoFiltersPreferGPU		= ([windowProperties objectForKey:@"videoFiltersPreferGPU"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"videoFiltersPreferGPU"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_FiltersPreferGPU"];
			const BOOL videoSourceDeposterize		= ([windowProperties objectForKey:@"videoSourceDeposterize"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"videoSourceDeposterize"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_Deposterize"];
			const NSInteger videoPixelScaler		= ([windowProperties objectForKey:@"videoFilterType"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"videoFilterType"] integerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"];
			const NSInteger videoOutputFilter		= ([windowProperties objectForKey:@"videoOutputFilter"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"videoOutputFilter"] integerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_OutputFilter"];
			
			const BOOL hudEnable					= ([windowProperties objectForKey:@"hudEnable"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudEnable"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_EnableHUD"];
			const BOOL hudShowExecutionSpeed		= ([windowProperties objectForKey:@"hudShowExecutionSpeed"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowExecutionSpeed"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowExecutionSpeed"];
			const BOOL hudShowVideoFPS				= ([windowProperties objectForKey:@"hudShowVideoFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowVideoFPS"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowVideoFPS"];
			const BOOL hudShowRender3DFPS			= ([windowProperties objectForKey:@"hudShowRender3DFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowRender3DFPS"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowRender3DFPS"];
			const BOOL hudShowFrameIndex			= ([windowProperties objectForKey:@"hudShowFrameIndex"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowFrameIndex"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowFrameIndex"];
			const BOOL hudShowLagFrameCount			= ([windowProperties objectForKey:@"hudShowLagFrameCount"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowLagFrameCount"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowLagFrameCount"];
			const BOOL hudShowCPULoadAverage		= ([windowProperties objectForKey:@"hudShowCPULoadAverage"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowCPULoadAverage"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowCPULoadAverage"];
			const BOOL hudShowRTC					= ([windowProperties objectForKey:@"hudShowRTC"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowRTC"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowRTC"];
			const BOOL hudShowInput					= ([windowProperties objectForKey:@"hudShowInput"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowInput"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowInput"];
			
			const NSUInteger hudColorExecutionSpeed	= ([windowProperties objectForKey:@"hudColorExecutionSpeed"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorExecutionSpeed"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_ExecutionSpeed"];
			const NSUInteger hudColorVideoFPS		= ([windowProperties objectForKey:@"hudColorVideoFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorVideoFPS"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_VideoFPS"];
			const NSUInteger hudColorRender3DFPS	= ([windowProperties objectForKey:@"hudColorRender3DFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorRender3DFPS"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Render3DFPS"];
			const NSUInteger hudColorFrameIndex		= ([windowProperties objectForKey:@"hudColorFrameIndex"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorFrameIndex"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_FrameIndex"];
			const NSUInteger hudColorLagFrameCount	= ([windowProperties objectForKey:@"hudColorLagFrameCount"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorLagFrameCount"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_LagFrameCount"];
			const NSUInteger hudColorCPULoadAverage	= ([windowProperties objectForKey:@"hudColorCPULoadAverage"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorCPULoadAverage"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_CPULoadAverage"];
			const NSUInteger hudColorRTC			= ([windowProperties objectForKey:@"hudColorRTC"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorRTC"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_RTC"];
			const NSUInteger hudColorInputPendingAndApplied = ([windowProperties objectForKey:@"hudColorInputPendingAndApplied"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorInputPendingAndApplied"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Input_PendingAndApplied"];
			const NSUInteger hudColorInputAppliedOnly = ([windowProperties objectForKey:@"hudColorInputAppliedOnly"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorInputAppliedOnly"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Input_AppliedOnly"];
			const NSUInteger hudColorInputPendingOnly = ([windowProperties objectForKey:@"hudColorInputPendingOnly"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorInputPendingOnly"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Input_PendingOnly"];
			
			const BOOL useVerticalSync				= ([windowProperties objectForKey:@"useVerticalSync"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"useVerticalSync"] boolValue] : YES;
			const BOOL isMinSizeNormal				= ([windowProperties objectForKey:@"isMinSizeNormal"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"isMinSizeNormal"] boolValue] : YES;
			const BOOL isShowingStatusBar			= ([windowProperties objectForKey:@"isShowingStatusBar"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"isShowingStatusBar"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_ShowStatusBar"];
			const BOOL isInFullScreenMode			= ([windowProperties objectForKey:@"isInFullScreenMode"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"isInFullScreenMode"] boolValue] : NO;
			const NSUInteger screenIndex			= ([windowProperties objectForKey:@"screenIndex"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"screenIndex"] unsignedIntegerValue] : 0;
			NSString *windowFrameStr				= (NSString *)[windowProperties valueForKey:@"windowFrame"];
			
			int frameX = 0;
			int frameY = 0;
			int frameWidth = 256;
			int frameHeight = 192*2;
			const char *frameCStr = [windowFrameStr cStringUsingEncoding:NSUTF8StringEncoding];
			sscanf(frameCStr, "%i %i %i %i", &frameX, &frameY, &frameWidth, &frameHeight);
			
			// Force the window to load now so that we can overwrite its internal defaults with the user's defaults.
			[windowController window];
			
			[windowController setDisplayMode:(ClientDisplayMode)displayMode
								   viewScale:displayScale
									rotation:displayRotation
									  layout:(ClientDisplayLayout)displayOrientation
									   order:(ClientDisplayOrder)displayOrder
									gapScale:displayGap
							 isMinSizeNormal:isMinSizeNormal
						  isShowingStatusBar:isShowingStatusBar];
			
			[[windowController view] setDisplayMainVideoSource:displayMainSource];
			[[windowController view] setDisplayTouchVideoSource:displayTouchSource];
			
			[windowController setVideoPropertiesWithoutUpdateUsingPreferGPU:videoFiltersPreferGPU
														  sourceDeposterize:videoSourceDeposterize
															   outputFilter:videoOutputFilter
																pixelScaler:videoPixelScaler];
			
			[[windowController view] setUseVerticalSync:useVerticalSync];
			[[windowController view] setIsHUDVisible:hudEnable];
			[[windowController view] setIsHUDExecutionSpeedVisible:hudShowExecutionSpeed];
			[[windowController view] setIsHUDVideoFPSVisible:hudShowVideoFPS];
			[[windowController view] setIsHUDRender3DFPSVisible:hudShowRender3DFPS];
			[[windowController view] setIsHUDFrameIndexVisible:hudShowFrameIndex];
			[[windowController view] setIsHUDLagFrameCountVisible:hudShowLagFrameCount];
			[[windowController view] setIsHUDCPULoadAverageVisible:hudShowCPULoadAverage];
			[[windowController view] setIsHUDRealTimeClockVisible:hudShowRTC];
			[[windowController view] setIsHUDInputVisible:hudShowInput];
			
			[[windowController view] setHudColorExecutionSpeed:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorExecutionSpeed]];
			[[windowController view] setHudColorVideoFPS:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorVideoFPS]];
			[[windowController view] setHudColorRender3DFPS:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorRender3DFPS]];
			[[windowController view] setHudColorFrameIndex:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorFrameIndex]];
			[[windowController view] setHudColorLagFrameCount:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorLagFrameCount]];
			[[windowController view] setHudColorCPULoadAverage:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorCPULoadAverage]];
			[[windowController view] setHudColorRTC:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorRTC]];
			[[windowController view] setHudColorInputPendingAndApplied:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorInputPendingAndApplied]];
			[[windowController view] setHudColorInputAppliedOnly:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorInputAppliedOnly]];
			[[windowController view] setHudColorInputPendingOnly:[CocoaDSUtil NSColorFromRGBA8888:(uint32_t)hudColorInputPendingOnly]];
			
			[[windowController masterWindow] setFrameOrigin:NSMakePoint(frameX, frameY)];
			
			// If this is the last window in the list, make this window key and main.
			// Otherwise, just order the window to the front so that the windows will
			// stack in a deterministic order.
			[[windowController view] setAllowViewUpdates:YES];
			[[[windowController view] cdsVideoOutput] handleReloadReprocessRedraw];
			
			if (windowProperties == [windowPropertiesList lastObject])
			{
				[[windowController window] makeKeyAndOrderFront:self];
				[[windowController window] makeMainWindow];
			}
			else
			{
				[[windowController window] orderFront:self];
				[[windowController window] makeMainWindow];
			}
			
			// If this window is set to full screen mode, its associated screen index must
			// exist. If not, this window will not enter full screen mode. This is necessary,
			// since the user's screen configuration could change in between app launches,
			// and since we don't want a window to go full screen on the wrong screen.
			if (isInFullScreenMode &&
				([[NSScreen screens] indexOfObject:[[windowController window] screen]] == screenIndex))
			{
				[windowController toggleFullScreenDisplay:self];
			}
		}
	}
}

- (void) saveDisplayWindowStates
{
	const BOOL willRestoreWindows = [[NSUserDefaults standardUserDefaults] boolForKey:@"General_WillRestoreDisplayWindows"];
	
	if (willRestoreWindows && [windowList count] > 0)
	{
		NSMutableArray *windowPropertiesList = [NSMutableArray arrayWithCapacity:[windowList count]];
		
		for (DisplayWindowController *windowController in windowList)
		{
			const NSUInteger screenIndex = [[NSScreen screens] indexOfObject:[[windowController masterWindow] screen]];
			
			const NSRect windowFrame = [windowController masterWindowFrame];
			NSString *windowFrameStr = [NSString stringWithFormat:@"%i %i %i %i",
										(int)windowFrame.origin.x, (int)windowFrame.origin.y, (int)windowFrame.size.width, (int)windowFrame.size.height];
			
			NSDictionary *windowProperties = [NSDictionary dictionaryWithObjectsAndKeys:
											  [NSNumber numberWithInteger:[windowController displayMode]], @"displayMode",
											  [NSNumber numberWithDouble:[windowController masterWindowScale]], @"displayScale",
											  [NSNumber numberWithDouble:[windowController displayRotation]], @"displayRotation",
											  [NSNumber numberWithInteger:[[windowController view] displayMainVideoSource]], @"displayMainVideoSource",
											  [NSNumber numberWithInteger:[[windowController view] displayTouchVideoSource]], @"displayTouchVideoSource",
											  [NSNumber numberWithInteger:[windowController displayOrientation]], @"displayOrientation",
											  [NSNumber numberWithInteger:[windowController displayOrder]], @"displayOrder",
											  [NSNumber numberWithDouble:[windowController displayGap]], @"displayGap",
											  [NSNumber numberWithBool:[[windowController view] videoFiltersPreferGPU]], @"videoFiltersPreferGPU",
											  [NSNumber numberWithInteger:[[windowController view] pixelScaler]], @"videoFilterType",
											  [NSNumber numberWithInteger:[[windowController view] outputFilter]], @"videoOutputFilter",
											  [NSNumber numberWithBool:[[windowController view] sourceDeposterize]], @"videoSourceDeposterize",
											  [NSNumber numberWithBool:[[windowController view] useVerticalSync]], @"useVerticalSync",
											  [NSNumber numberWithBool:[[windowController view] isHUDVisible]], @"hudEnable",
											  [NSNumber numberWithBool:[[windowController view] isHUDExecutionSpeedVisible]], @"hudShowExecutionSpeed",
											  [NSNumber numberWithBool:[[windowController view] isHUDVideoFPSVisible]], @"hudShowVideoFPS",
											  [NSNumber numberWithBool:[[windowController view] isHUDRender3DFPSVisible]], @"hudShowRender3DFPS",
											  [NSNumber numberWithBool:[[windowController view] isHUDFrameIndexVisible]], @"hudShowFrameIndex",
											  [NSNumber numberWithBool:[[windowController view] isHUDLagFrameCountVisible]], @"hudShowLagFrameCount",
											  [NSNumber numberWithBool:[[windowController view] isHUDCPULoadAverageVisible]], @"hudShowCPULoadAverage",
											  [NSNumber numberWithBool:[[windowController view] isHUDRealTimeClockVisible]], @"hudShowRTC",
											  [NSNumber numberWithBool:[[windowController view] isHUDInputVisible]], @"hudShowInput",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorExecutionSpeed]]], @"hudColorExecutionSpeed",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorVideoFPS]]], @"hudColorVideoFPS",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorRender3DFPS]]], @"hudColorRender3DFPS",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorFrameIndex]]], @"hudColorFrameIndex",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorLagFrameCount]]], @"hudColorLagFrameCount",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorCPULoadAverage]]], @"hudColorCPULoadAverage",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorRTC]]], @"hudColorRTC",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorInputPendingAndApplied]]], @"hudColorInputPendingAndApplied",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorInputAppliedOnly]]], @"hudColorInputAppliedOnly",
											  [NSNumber numberWithUnsignedInteger:[CocoaDSUtil RGBA8888FromNSColor:[[windowController view] hudColorInputPendingOnly]]], @"hudColorInputPendingOnly",
											  [NSNumber numberWithBool:[windowController isMinSizeNormal]], @"isMinSizeNormal",
											  [NSNumber numberWithBool:[windowController masterStatusBarState]], @"isShowingStatusBar",
											  [NSNumber numberWithBool:[windowController isFullScreen]], @"isInFullScreenMode",
											  [NSNumber numberWithUnsignedInteger:screenIndex], @"screenIndex",
											  windowFrameStr, @"windowFrame",
											  nil];
			
			[windowPropertiesList addObject:windowProperties];
		}
		
		[[NSUserDefaults standardUserDefaults] setObject:windowPropertiesList forKey:@"General_DisplayWindowRestorableStates"];
	}
	else
	{
		[[NSUserDefaults standardUserDefaults] removeObjectForKey:@"General_DisplayWindowRestorableStates"];
	}
}

#pragma mark NSUserInterfaceValidations Protocol

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)theItem
{
	BOOL enable = YES;
    const SEL theAction = [theItem action];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if (theAction == @selector(importRomSave:))
    {
		if ([self currentRom] == nil)
		{
			enable = NO;
		}
    }
	else if (theAction == @selector(exportRomSave:))
	{
		NSFileManager *fileManager = [[NSFileManager alloc] init];
		
		if ( ([self currentRom] == nil) ||
		     ![fileManager isReadableFileAtPath:[[CocoaDSFile fileURLFromRomURL:[[self currentRom] fileURL] toKind:@"ROM Save"] path]] )
		{
			enable = NO;
		}
		
		[fileManager release];
	}
    else if (theAction == @selector(toggleExecutePause:))
    {
		if (![cdsCore masterExecute] ||
			[self currentRom] == nil ||
			[self isUserInterfaceBlockingExecution])
		{
			enable = NO;
		}
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore coreState] == ExecutionBehavior_Pause)
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_EXECUTE_CONTROL];
			}
			else if ([cdsCore coreState] == ExecutionBehavior_Run)
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_PAUSE_CONTROL];
			}
		}
		else if ([(id)theItem isMemberOfClass:[NSToolbarItem class]])
		{
			if ([cdsCore coreState] == ExecutionBehavior_Pause)
			{
				[(NSToolbarItem*)theItem setLabel:NSSTRING_TITLE_EXECUTE_CONTROL];
				[(NSToolbarItem*)theItem setImage:iconExecute];
			}
			else if ([cdsCore coreState] == ExecutionBehavior_Run)
			{
				[(NSToolbarItem*)theItem setLabel:NSSTRING_TITLE_PAUSE_CONTROL];
				[(NSToolbarItem*)theItem setImage:iconPause];
			}
		}
    }
	else if (theAction == @selector(frameAdvance:))
	{
		if ([cdsCore coreState] != ExecutionBehavior_Pause ||
			![cdsCore masterExecute] ||
			[self currentRom] == nil ||
			[self isShowingSaveStateDialog])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(frameJump:))
	{
		if (![cdsCore masterExecute] ||
			[self currentRom] == nil ||
			[self isShowingSaveStateDialog])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(coreExecute:))
    {
		if ([cdsCore coreState] == ExecutionBehavior_Run ||
			[cdsCore coreState] == ExecutionBehavior_FrameAdvance ||
			![cdsCore masterExecute] ||
			[self currentRom] == nil ||
			[self isShowingSaveStateDialog])
		{
			enable = NO;
		}
    }
	else if (theAction == @selector(corePause:))
    {
		if ([cdsCore coreState] == ExecutionBehavior_Pause ||
			![cdsCore masterExecute] ||
			[self currentRom] == nil ||
			[self isShowingSaveStateDialog])
		{
			enable = NO;
		}
    }
	else if (theAction == @selector(reset:))
	{
		if ([self currentRom] == nil || [self isUserInterfaceBlockingExecution] || [cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(loadRecentRom:))
	{
		if ([self isRomLoading] || [self isShowingSaveStateDialog] || [cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(openRom:))
	{
		if ([self isRomLoading] || [self isShowingSaveStateDialog] || [cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(closeRom:))
	{
		if ([self currentRom] == nil || [self isRomLoading] || [self isShowingSaveStateDialog] || [cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(revealRomInFinder:))
	{
		if ([self currentRom] == nil || [self isRomLoading])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(loadEmuSaveStateSlot:))
	{
		if ([self currentRom] == nil ||
			[self isShowingSaveStateDialog] ||
			![CocoaDSFile saveStateExistsForSlot:[[self currentRom] fileURL] slotNumber:[theItem tag] + 1] ||
			[cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(saveEmuSaveStateSlot:))
	{
		if ([self currentRom] == nil || [self isShowingSaveStateDialog])
		{
			enable = NO;
		}
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([CocoaDSFile saveStateExistsForSlot:[[self currentRom] fileURL] slotNumber:[theItem tag] + 1])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(openReplay:))
	{
		if ([self currentRom] == nil || [self isRomLoading] || [self isShowingSaveStateDialog])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(recordReplay:))
	{
		if ([self currentRom] == nil || [self isRomLoading] || [self isShowingSaveStateDialog])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(stopReplay:))
	{
		if ([self currentRom] == nil || [self isRomLoading] || [self isShowingSaveStateDialog])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(changeCoreSpeed:))
	{
		NSInteger speedScalar = (NSInteger)([cdsCore speedScalar] * 100.0);
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([theItem tag] == -1)
			{
				if (speedScalar == (NSInteger)(SPEED_SCALAR_HALF * 100.0) ||
					speedScalar == (NSInteger)(SPEED_SCALAR_NORMAL * 100.0) ||
					speedScalar == (NSInteger)(SPEED_SCALAR_DOUBLE * 100.0))
				{
					[(NSMenuItem*)theItem setState:NSOffState];
				}
				else
				{
					[(NSMenuItem*)theItem setState:NSOnState];
				}
			}
			else
			{
				[(NSMenuItem*)theItem setState:(speedScalar == [theItem tag]) ? NSOnState : NSOffState];
			}
		}
		else if ([(id)theItem isMemberOfClass:[NSToolbarItem class]])
		{
			if (speedScalar == (NSInteger)(SPEED_SCALAR_DOUBLE * 100.0))
			{
				[(NSToolbarItem*)theItem setLabel:NSSTRING_TITLE_SPEED_1X];
				[(NSToolbarItem*)theItem setTag:100];
				[(NSToolbarItem*)theItem setImage:iconSpeedNormal];
			}
			else
			{
				[(NSToolbarItem*)theItem setLabel:NSSTRING_TITLE_SPEED_2X];
				[(NSToolbarItem*)theItem setTag:200];
				[(NSToolbarItem*)theItem setImage:iconSpeedDouble];
			}
		}
	}
	else if (theAction == @selector(toggleSpeedLimiter:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem*)theItem setTitle:([cdsCore isSpeedLimitEnabled]) ? NSSTRING_TITLE_DISABLE_SPEED_LIMIT : NSSTRING_TITLE_ENABLE_SPEED_LIMIT];
		}
	}
	else if (theAction == @selector(toggleAutoFrameSkip:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem*)theItem setTitle:([cdsCore isFrameSkipEnabled]) ? NSSTRING_TITLE_DISABLE_AUTO_FRAME_SKIP : NSSTRING_TITLE_ENABLE_AUTO_FRAME_SKIP];
		}
		
		if ([avCaptureToolDelegate isRecording])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleCheats:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem*)theItem setTitle:([cdsCore isCheatingEnabled]) ? NSSTRING_TITLE_DISABLE_CHEATS : NSSTRING_TITLE_ENABLE_CHEATS];
		}
	}
	else if (theAction == @selector(changeRomSaveType:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem*)theItem setState:([self selectedRomSaveTypeID] == [theItem tag]) ? NSOnState : NSOffState];
		}
	}
	else if (theAction == @selector(openEmuSaveState:) ||
			 theAction == @selector(saveEmuSaveState:) ||
			 theAction == @selector(saveEmuSaveStateAs:))
	{
		if ([self currentRom] == nil || [self isShowingSaveStateDialog] || [cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(revertEmuSaveState:))
	{
		if ([self currentRom] == nil ||
			[self isShowingSaveStateDialog] ||
			[self currentSaveStateURL] == nil ||
			[cdsCore isInDebugTrap])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleGPUState:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			[(NSMenuItem*)theItem setState:([cdsCore.cdsGPU gpuStateByBit:[theItem tag]]) ? NSOnState : NSOffState];
		}
	}
	
	return enable;
}

#pragma mark CocoaDSControllerDelegate Protocol

- (void) doMicLevelUpdateFromController:(CocoaDSController *)cdsController
{
	[self updateMicStatusIcon];
}

- (void) doMicHardwareStateChangedFromController:(CocoaDSController *)cdsController
									   isEnabled:(BOOL)isHardwareEnabled
										isLocked:(BOOL)isHardwareLocked
{
	const BOOL hwMicAvailable = (isHardwareEnabled && !isHardwareLocked);
	[self setIsHardwareMicAvailable:hwMicAvailable];
	[self updateMicStatusIcon];
}

- (void) doMicHardwareGainChangedFromController:(CocoaDSController *)cdsController gain:(float)gainValue
{
	[self setCurrentMicGainValue:gainValue*100.0f];
}

@end
