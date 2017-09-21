/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2017 DeSmuME Team

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

#import "appDelegate.h"
#import "DisplayWindowController.h"
#import "EmuControllerDelegate.h"
#import "FileMigrationDelegate.h"
#import "RomInfoPanel.h"
#import "Slot2WindowDelegate.h"
#import "preferencesWindowDelegate.h"
#import "troubleshootingWindowDelegate.h"
#import "cheatWindowDelegate.h"
#import "inputPrefsView.h"

#import "cocoa_core.h"
#import "cocoa_GPU.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_globals.h"
#import "cocoa_input.h"
#import "cocoa_output.h"
#import "cocoa_rom.h"
#import "cocoa_util.h"


@implementation AppDelegate

@dynamic dummyObject;
@synthesize prefWindow;
@synthesize troubleshootingWindow;
@synthesize cheatListWindow;
@synthesize slot2Window;
@synthesize prefGeneralView;
@synthesize mLoadStateSlot;
@synthesize mSaveStateSlot;
@synthesize inputPrefsView;
@synthesize aboutWindowController;
@synthesize emuControlController;
@synthesize cdsSoundController;
@synthesize romInfoPanelController;
@synthesize prefWindowController;
@synthesize cdsCoreController;
@synthesize inputDeviceListController;
@synthesize cheatWindowController;
@synthesize migrationDelegate;
@synthesize inputManager;
@synthesize romInfoPanel;

@synthesize isAppRunningOnIntel;
@synthesize isDeveloperPlusBuild;


- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	// Determine if we're running on Intel or PPC.
#if defined(__i386__) || defined(__x86_64__)
	isAppRunningOnIntel = YES;
#else
	isAppRunningOnIntel = NO;
#endif
    
#if defined(GDB_STUB)
    isDeveloperPlusBuild = YES;
#else
    isDeveloperPlusBuild = NO;
#endif
	
	RGBA8888ToNSColorValueTransformer *nsColorTransformer = [[[RGBA8888ToNSColorValueTransformer alloc] init] autorelease];
	[NSValueTransformer setValueTransformer:nsColorTransformer forName:@"RGBA8888ToNSColorValueTransformer"];
	
	return self;
}

#pragma mark NSApplicationDelegate Protocol
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	BOOL result = NO;
	NSURL *fileURL = [NSURL fileURLWithPath:filename];
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if (cdsCore == nil)
	{
		return result;
	}
	
	NSString *fileKind = [CocoaDSFile fileKindByURL:fileURL];
	if ([fileKind isEqualToString:@"ROM"])
	{
		result = [emuControl handleLoadRomByURL:fileURL];
		if ([emuControl isShowingSaveStateDialog] || [emuControl isShowingFileMigrationDialog])
		{
			// Just reply YES if a sheet is showing, even if the ROM hasn't actually been loaded yet.
			// This will prevent the error dialog from showing, which is the intended behavior in
			// this case.
			result = YES;
		}
	}
	
	return result;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
	// Register the application's defaults.
	NSDictionary *prefsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultUserPrefs" ofType:@"plist"]];
	if (prefsDict == nil)
	{
		[[NSAlert alertWithMessageText:NSSTRING_ALERT_CRITICAL_FILE_MISSING_PRI defaultButton:nil alternateButton:nil otherButton:nil informativeTextWithFormat:NSSTRING_ALERT_CRITICAL_FILE_MISSING_SEC] runModal];
		[NSApp terminate:nil];
		return;
	}
	
	[[NSUserDefaults standardUserDefaults] registerDefaults:prefsDict];
	
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	PreferencesWindowDelegate *prefWindowDelegate = (PreferencesWindowDelegate *)[prefWindow delegate];
	CheatWindowDelegate *cheatWindowDelegate = (CheatWindowDelegate *)[cheatListWindow delegate];
	Slot2WindowDelegate *slot2WindowDelegate = (Slot2WindowDelegate *)[slot2Window delegate];
	
	// Create the needed directories in Application Support if they haven't already
	// been created.
	if (![CocoaDSFile setupAllAppDirectories])
	{
		// Need to show a modal dialog here.
		return;
	}
	
	[CocoaDSFile setupAllFilePaths];
	
	// Setup the About window.
	NSString *descriptionStr = [[[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"] stringByAppendingString:@" "] stringByAppendingString:[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"]];
	descriptionStr = [[descriptionStr stringByAppendingString:@"\n"] stringByAppendingString:@STRING_DESMUME_SHORT_DESCRIPTION];
	descriptionStr = [[descriptionStr stringByAppendingString:@"\n"] stringByAppendingString:@STRING_DESMUME_WEBSITE];
	
	NSString *buildInfoStr = @"Build Info:";
	buildInfoStr = [[buildInfoStr stringByAppendingString:[CocoaDSUtil appInternalVersionString]] stringByAppendingString:[CocoaDSUtil appCompilerDetailString]];
	buildInfoStr = [[buildInfoStr stringByAppendingString:@"\nBuild Date: "] stringByAppendingString:@__DATE__];
	buildInfoStr = [[buildInfoStr stringByAppendingString:@"\nOperating System: "] stringByAppendingString:[CocoaDSUtil operatingSystemString]];
	buildInfoStr = [[buildInfoStr stringByAppendingString:@"\nModel Identifier: "] stringByAppendingString:[CocoaDSUtil modelIdentifierString]];
	
	NSFont *aboutTextFilesFont = [NSFont fontWithName:@"Monaco" size:10];
	NSMutableDictionary *aboutWindowProperties = [NSMutableDictionary dictionaryWithObjectsAndKeys:
												  [[NSBundle mainBundle] pathForResource:@FILENAME_README ofType:@""], @"readMePath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_COPYING ofType:@""], @"licensePath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_AUTHORS ofType:@""], @"authorsPath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_CHANGELOG ofType:@""], @"changeLogPath",
												  descriptionStr, @"descriptionString",
												  buildInfoStr, @"buildInfoString",
												  aboutTextFilesFont, @"aboutTextFilesFont",
												  nil];
	
	[aboutWindowController setContent:aboutWindowProperties];
	
	// Set the preferences window to the general view by default.
	[[prefWindowDelegate toolbar] setSelectedItemIdentifier:[[prefWindowDelegate toolbarItemGeneral] itemIdentifier]];
	[prefWindowDelegate changePrefView:self];
	
	// Setup the slot menu items. We set this up manually instead of through Interface
	// Builder because we're assuming an arbitrary number of slot items.
	[self setupSlotMenuItems];
	
	// Init the DS emulation core.
	CocoaDSCore *newCore = [[[CocoaDSCore alloc] init] autorelease];
	
	// Init the DS controller.
	[[newCore cdsController] setDelegate:emuControl];
	[[newCore cdsController] startHardwareMicDevice];
	
	// Init the DS speakers.
	CocoaDSSpeaker *newSpeaker = [[[CocoaDSSpeaker alloc] init] autorelease];
	[newCore addOutput:newSpeaker];
	[emuControl setCdsSpeaker:newSpeaker];
	
	// Update the SLOT-2 device list after the emulation core is initialized.
	[slot2WindowDelegate update];
	[slot2WindowDelegate setHidManager:[inputManager hidManager]];
	[slot2WindowDelegate setAutoSelectedDeviceText:[[slot2WindowDelegate deviceManager] autoSelectedDeviceName]];
	
	// Set up all the object controllers.
	[cdsCoreController setContent:newCore];
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	[prefWindowController setContent:[prefWindowDelegate bindings]];
	[cheatWindowController setContent:[cheatWindowDelegate bindings]];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	
	// Determine if the app was run for the first time.
	NSMutableDictionary *appFirstTimeRunDict = [[NSMutableDictionary alloc] initWithDictionary:[[NSUserDefaults standardUserDefaults] dictionaryForKey:@"General_AppFirstTimeRun"]];
	NSString *bundleVersionString = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
	
	BOOL isFirstTimeRun = NO;
	NSNumber *isFirstTimeRunNumber = (NSNumber *)[appFirstTimeRunDict valueForKey:bundleVersionString];
	if (isFirstTimeRunNumber == nil)
	{
		isFirstTimeRunNumber = [NSNumber numberWithBool:isFirstTimeRun];
	}
	
	isFirstTimeRun = [isFirstTimeRunNumber boolValue];
	
	if (appFirstTimeRunDict == nil)
	{
		appFirstTimeRunDict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:isFirstTimeRunNumber, bundleVersionString, nil];
	}
	
	// Setup the applications settings from the user defaults file.
	[self setupUserDefaults];
	
	// Set up the input preferences view.
	[inputPrefsView initSettingsSheets];
	[inputPrefsView populateInputProfileMenu];
	[[inputPrefsView inputPrefOutlineView] expandItem:nil expandChildren:YES];
	[[inputPrefsView inputProfileMenu] selectItemAtIndex:0];
	
	// Initialize the microphone status icon.
	[emuControl updateMicStatusIcon];
	
	// Bring the application to the front
	[NSApp activateIgnoringOtherApps:YES];
	[self restoreDisplayWindowStates];
	
	// Load a new ROM on launch per user preferences.
	if ([[NSUserDefaults standardUserDefaults] objectForKey:@"General_AutoloadROMOnLaunch"] != nil)
	{
		// Older versions of DeSmuME used General_AutoloadROMOnLaunch to determine whether to
		// load a ROM on launch or not. This has been superseded by the autoload ROM option
		// ROMAUTOLOADOPTION_LOAD_NONE. So if this object key exists in the user defaults, we
		// need to update the user defaults key/values as needed, and then remove the
		// General_AutoloadROMOnLaunch object key.
		const BOOL loadROMOnLaunch = [[NSUserDefaults standardUserDefaults] boolForKey:@"General_AutoloadROMOnLaunch"];
		if (!loadROMOnLaunch)
		{
			[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_NONE forKey:@"General_AutoloadROMOption"];
		}
		
		[[NSUserDefaults standardUserDefaults] removeObjectForKey:@"General_AutoloadROMOnLaunch"];
	}
	
	if ([emuControl currentRom] == nil)
	{
		const NSInteger autoloadRomOption = [[NSUserDefaults standardUserDefaults] integerForKey:@"General_AutoloadROMOption"];
		NSURL *autoloadRomURL = nil;
		
		switch (autoloadRomOption)
		{
			case ROMAUTOLOADOPTION_LOAD_NONE:
				autoloadRomURL = nil;
				break;
				
			case ROMAUTOLOADOPTION_LOAD_LAST:
				autoloadRomURL = [CocoaDSFile lastLoadedRomURL];
				break;
				
			case ROMAUTOLOADOPTION_LOAD_SELECTED:
			{
				NSString *autoloadRomPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"General_AutoloadROMSelectedPath"];
				if (autoloadRomPath != nil && [autoloadRomPath length] > 0)
				{
					autoloadRomURL = [NSURL fileURLWithPath:autoloadRomPath];
				}
				
				break;
			}
				
			default:
				break;
		}
		
		[emuControl handleLoadRomByURL:autoloadRomURL];
	}
	
	// Present the file migration window to the user (if they haven't disabled it).
	if (![[NSUserDefaults standardUserDefaults] boolForKey:@"General_DoNotAskMigrate"] || !isFirstTimeRun)
	{
		[migrationDelegate updateFileList];
		if ([migrationDelegate filesPresent])
		{
			[[migrationDelegate window] center];
			[[migrationDelegate window] makeKeyAndOrderFront:nil];
		}
	}
	
	// Set that the app has run for the first time.
	isFirstTimeRun = YES;
	[appFirstTimeRunDict setValue:[NSNumber numberWithBool:isFirstTimeRun] forKey:bundleVersionString];
	[[NSUserDefaults standardUserDefaults] setObject:appFirstTimeRunDict forKey:@"General_AppFirstTimeRun"];
	[appFirstTimeRunDict release];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	
	// If a file needs to be saved, give the user a chance to save it
	// before quitting.
	const BOOL didRomClose = [emuControl handleUnloadRom:REASONFORCLOSE_TERMINATE romToLoad:nil];
	if (!didRomClose)
	{
		if ([emuControl isShowingSaveStateDialog])
		{
			return NSTerminateLater;
		}
	}
	
	// Either there wasn't a file that needed to be saved, or there
	// wasn't anything loaded. Just continue program termination normally.
	return NSTerminateNow;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	// Save some settings to user defaults before app termination
	[self saveDisplayWindowStates];
	[romInfoPanel writeDefaults];
	[[NSUserDefaults standardUserDefaults] setBool:[[cdsCore cdsController] hardwareMicMute] forKey:@"Microphone_HardwareMicMute"];
	[[NSUserDefaults standardUserDefaults] setDouble:[emuControl currentVolumeValue] forKey:@"Sound_Volume"];
	[[NSUserDefaults standardUserDefaults] setDouble:[emuControl lastSetSpeedScalar] forKey:@"CoreControl_SpeedScalar"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore isSpeedLimitEnabled] forKey:@"CoreControl_EnableSpeedLimit"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore isFrameSkipEnabled] forKey:@"CoreControl_EnableAutoFrameSkip"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore isCheatingEnabled] forKey:@"CoreControl_EnableCheats"];
	
#ifdef GDB_STUB
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore enableGdbStubARM9] forKey:@"Debug_GDBStubEnableARM9"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore enableGdbStubARM7] forKey:@"Debug_GDBStubEnableARM7"];
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsCore gdbStubPortARM9] forKey:@"Debug_GDBStubPortARM9"];
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsCore gdbStubPortARM7] forKey:@"Debug_GDBStubPortARM7"];
#endif
	
	[cdsCoreController setContent:nil];
}

#pragma mark IBActions
- (IBAction) launchWebsite:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_WEBSITE]];
}

- (IBAction) launchForums:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_FORUM_SITE]];
}

- (IBAction) supportRequest:(id)sender
{
	TroubleshootingWindowDelegate *troubleshootingWindowDelegate = [troubleshootingWindow delegate];
	
	[troubleshootingWindowDelegate switchViewByID:TROUBLESHOOTING_TECH_SUPPORT_VIEW_ID];
	[troubleshootingWindow setTitle:NSSTRING_TITLE_TECH_SUPPORT_WINDOW_TITLE];
	[troubleshootingWindow makeKeyAndOrderFront:sender];
}

- (IBAction) bugReport:(id)sender
{
	TroubleshootingWindowDelegate *troubleshootingWindowDelegate = [troubleshootingWindow delegate];
	
	[troubleshootingWindowDelegate switchViewByID:TROUBLESHOOTING_BUG_REPORT_VIEW_ID];
	[troubleshootingWindow setTitle:NSSTRING_TITLE_BUG_REPORT_WINDOW_TITLE];
	[troubleshootingWindow makeKeyAndOrderFront:sender];
}

#pragma mark Class Methods
- (void) setupSlotMenuItems
{
	NSMenuItem *loadItem = nil;
	NSMenuItem *saveItem = nil;
	
	for(NSInteger i = 0; i < MAX_SAVESTATE_SLOTS; i++)
	{
		loadItem = [self addSlotMenuItem:mLoadStateSlot slotNumber:(NSUInteger)(i + 1)];
		[loadItem setKeyEquivalentModifierMask:0];
		[loadItem setTag:i];
		[loadItem setAction:@selector(loadEmuSaveStateSlot:)];
		
		saveItem = [self addSlotMenuItem:mSaveStateSlot slotNumber:(NSUInteger)(i + 1)];
		[saveItem setKeyEquivalentModifierMask:NSShiftKeyMask];
		[saveItem setTag:i];
		[saveItem setAction:@selector(saveEmuSaveStateSlot:)];
	}
}

- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	NSUInteger slotNumberKey = slotNumber;
	
	if (slotNumber == 10)
	{
		slotNumberKey = 0;
	}
	
	NSMenuItem *mItem = [menu addItemWithTitle:[NSString stringWithFormat:NSSTRING_TITLE_SLOT_NUMBER, (unsigned long)slotNumber]
										action:nil
								 keyEquivalent:[NSString stringWithFormat:@"%ld", (unsigned long)slotNumberKey]];
	
	[mItem setTarget:emuControl];
	
	return mItem;
}

- (void) setupUserDefaults
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	PreferencesWindowDelegate *prefWindowDelegate = [prefWindow delegate];
	Slot2WindowDelegate *slot2WindowDelegate = (Slot2WindowDelegate *)[slot2Window delegate];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	// Set the emulation flags.
	[cdsCore setEmuFlagAdvancedBusLevelTiming:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_AdvancedBusLevelTiming"]];
	[cdsCore setEmuFlagRigorousTiming:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_RigorousTiming"]];
	[cdsCore setEmuFlagUseExternalBios:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_UseExternalBIOSImages"]];
	[cdsCore setEmuFlagEmulateBiosInterrupts:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_BIOSEmulateSWI"]];
	[cdsCore setEmuFlagPatchDelayLoop:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_BIOSPatchDelayLoopSWI"]];
	[cdsCore setEmuFlagUseExternalFirmware:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_UseExternalFirmwareImage"]];
	[cdsCore setEmuFlagFirmwareBoot:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_FirmwareBoot"]];
	[cdsCore setEmuFlagEmulateEnsata:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_EmulateEnsata"]];
	[cdsCore setEmuFlagDebugConsole:[[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_UseDebugConsole"]];
	
	// If we're not running on Intel, force the CPU emulation engine to use the interpreter engine.
	if (!isAppRunningOnIntel)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:CPUEmulationEngineID_Interpreter forKey:@"Emulation_CPUEmulationEngine"];
	}
	
	// Set the CPU emulation engine per user preferences.
	[cdsCore setCpuEmulationEngine:[[NSUserDefaults standardUserDefaults] integerForKey:@"Emulation_CPUEmulationEngine"]];
	[cdsCore setMaxJITBlockSize:[[NSUserDefaults standardUserDefaults] integerForKey:@"Emulation_MaxJITBlockSize"]];
	
	// Set the SLOT-1 device settings per user preferences.
	NSString *slot1R4Path = (NSString *)[[NSUserDefaults standardUserDefaults] objectForKey:@"EmulationSlot1_R4StoragePath"];
	[cdsCore setSlot1DeviceType:[[NSUserDefaults standardUserDefaults] integerForKey:@"EmulationSlot1_DeviceType"]];
	[cdsCore setSlot1R4URL:(slot1R4Path != nil) ? [NSURL fileURLWithPath:slot1R4Path] : nil];
	
	// Set the miscellaneous emulations settings per user preferences.
	[emuControl changeCoreSpeedWithDouble:[[NSUserDefaults standardUserDefaults] doubleForKey:@"CoreControl_SpeedScalar"]];
	[cdsCore setIsSpeedLimitEnabled:[[NSUserDefaults standardUserDefaults] boolForKey:@"CoreControl_EnableSpeedLimit"]];
	[cdsCore setIsFrameSkipEnabled:[[NSUserDefaults standardUserDefaults] boolForKey:@"CoreControl_EnableAutoFrameSkip"]];
	[cdsCore setIsCheatingEnabled:[[NSUserDefaults standardUserDefaults] boolForKey:@"CoreControl_EnableCheats"]];
	
	// Set up the firmware per user preferences.
	NSMutableDictionary *newFWDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Nickname"], @"Nickname",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Message"], @"Message",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_FavoriteColor"], @"FavoriteColor",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Birthday"], @"Birthday",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Language"], @"Language",
									  nil];
	
	CocoaDSFirmware *newFirmware = [[[CocoaDSFirmware alloc] initWithDictionary:newFWDict] autorelease];
	[newFirmware update];
	[emuControl setCdsFirmware:newFirmware];
	
	// Setup the ARM7 BIOS, ARM9 BIOS, and firmware image paths per user preferences.
	NSString *arm7BiosImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"BIOS_ARM7ImagePath"];
	if (arm7BiosImagePath != nil)
	{
		[cdsCore setArm7ImageURL:[NSURL fileURLWithPath:arm7BiosImagePath]];
	}
	else
	{
		[cdsCore setArm7ImageURL:nil];
	}
	
	NSString *arm9BiosImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"BIOS_ARM9ImagePath"];
	if (arm9BiosImagePath != nil)
	{
		[cdsCore setArm9ImageURL:[NSURL fileURLWithPath:arm9BiosImagePath]];
	}
	else
	{
		[cdsCore setArm9ImageURL:nil];
	}
	
	NSString *firmwareImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"Emulation_FirmwareImagePath"];
	if (firmwareImagePath != nil)
	{
		[cdsCore setFirmwareImageURL:[NSURL fileURLWithPath:firmwareImagePath]];
	}
	else
	{
		[cdsCore setFirmwareImageURL:nil];
	}
	
	// Set up GDB stub settings per user preferences.
#ifdef GDB_STUB
	[cdsCore setEnableGdbStubARM9:[[NSUserDefaults standardUserDefaults] boolForKey:@"Debug_GDBStubEnableARM9"]];
	[cdsCore setEnableGdbStubARM7:[[NSUserDefaults standardUserDefaults] boolForKey:@"Debug_GDBStubEnableARM7"]];
	[cdsCore setGdbStubPortARM9:[[NSUserDefaults standardUserDefaults] integerForKey:@"Debug_GDBStubPortARM9"]];
	[cdsCore setGdbStubPortARM7:[[NSUserDefaults standardUserDefaults] integerForKey:@"Debug_GDBStubPortARM7"]];
#else
	[cdsCore setEnableGdbStubARM9:NO];
	[cdsCore setEnableGdbStubARM7:NO];
	[cdsCore setGdbStubPortARM9:0];
	[cdsCore setGdbStubPortARM7:0];
#endif
	
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
	
	// Set up the ROM Info panel.
	[romInfoPanel setupUserDefaults];
	
	// Set up the preferences window.
	[prefWindowDelegate setupUserDefaults];
	
	// Set up the default SLOT-2 device.
	[slot2WindowDelegate setupUserDefaults];
	
	// Set up the rest of the emulation-related user defaults.
	[emuControl setupUserDefaults];
}

- (void) restoreDisplayWindowStates
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	NSArray *windowPropertiesList = [[NSUserDefaults standardUserDefaults] arrayForKey:@"General_DisplayWindowRestorableStates"];
	const BOOL willRestoreWindows = [[NSUserDefaults standardUserDefaults] boolForKey:@"General_WillRestoreDisplayWindows"];
	
	if (!willRestoreWindows || windowPropertiesList == nil || [windowPropertiesList count] < 1)
	{
		// If no windows were saved for restoring (the user probably closed all windows before
		// app termination), then simply create a new display window per user defaults.
		[emuControl newDisplayWindow:self];
	}
	else
	{
		for (NSDictionary *windowProperties in windowPropertiesList)
		{
			DisplayWindowController *windowController = [[DisplayWindowController alloc] initWithWindowNibName:@"DisplayWindow" emuControlDelegate:emuControl];
			
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
			const BOOL hudShowVideoFPS				= ([windowProperties objectForKey:@"hudShowVideoFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowVideoFPS"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowVideoFPS"];
			const BOOL hudShowRender3DFPS			= ([windowProperties objectForKey:@"hudShowRender3DFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowRender3DFPS"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowRender3DFPS"];
			const BOOL hudShowFrameIndex			= ([windowProperties objectForKey:@"hudShowFrameIndex"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowFrameIndex"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowFrameIndex"];
			const BOOL hudShowLagFrameCount			= ([windowProperties objectForKey:@"hudShowLagFrameCount"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowLagFrameCount"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowLagFrameCount"];
			const BOOL hudShowCPULoadAverage		= ([windowProperties objectForKey:@"hudShowCPULoadAverage"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowCPULoadAverage"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowCPULoadAverage"];
			const BOOL hudShowRTC					= ([windowProperties objectForKey:@"hudShowRTC"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowRTC"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowRTC"];
			const BOOL hudShowInput					= ([windowProperties objectForKey:@"hudShowInput"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudShowInput"] boolValue] : [[NSUserDefaults standardUserDefaults] boolForKey:@"HUD_ShowInput"];
			
			const NSUInteger hudColorVideoFPS		= ([windowProperties objectForKey:@"hudColorVideoFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorVideoFPS"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_VideoFPS"];
			const NSUInteger hudColorRender3DFPS	= ([windowProperties objectForKey:@"hudColorRender3DFPS"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorRender3DFPS"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Render3DFPS"];
			const NSUInteger hudColorFrameIndex		= ([windowProperties objectForKey:@"hudColorFrameIndex"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorFrameIndex"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_FrameIndex"];
			const NSUInteger hudColorLagFrameCount	= ([windowProperties objectForKey:@"hudColorLagFrameCount"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorLagFrameCount"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_LagFrameCount"];
			const NSUInteger hudColorCPULoadAverage	= ([windowProperties objectForKey:@"hudColorCPULoadAverage"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorCPULoadAverage"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_CPULoadAverage"];
			const NSUInteger hudColorRTC			= ([windowProperties objectForKey:@"hudColorRTC"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorRTC"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_RTC"];
			const NSUInteger hudColorInputPendingAndApplied = ([windowProperties objectForKey:@"hudColorInputPendingAndApplied"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorInputPendingAndApplied"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Input_PendingAndApplied"];
			const NSUInteger hudColorInputAppliedOnly = ([windowProperties objectForKey:@"hudColorInputAppliedOnly"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorInputAppliedOnly"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Input_AppliedOnly"];
			const NSUInteger hudColorInputPendingOnly = ([windowProperties objectForKey:@"hudColorInputPendingOnly"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"hudColorInputPendingOnly"] unsignedIntegerValue] : [[NSUserDefaults standardUserDefaults] integerForKey:@"HUD_Color_Input_PendingOnly"];
			
			const NSInteger screenshotFileFormat	= ([windowProperties objectForKey:@"screenshotFileFormat"] != nil) ? [(NSNumber *)[windowProperties valueForKey:@"screenshotFileFormat"] integerValue] : NSTIFFFileType;
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
			
			[windowController setScreenshotFileFormat:screenshotFileFormat];
			[[windowController view] setUseVerticalSync:useVerticalSync];
			[[windowController view] setIsHUDVisible:hudEnable];
			[[windowController view] setIsHUDVideoFPSVisible:hudShowVideoFPS];
			[[windowController view] setIsHUDRender3DFPSVisible:hudShowRender3DFPS];
			[[windowController view] setIsHUDFrameIndexVisible:hudShowFrameIndex];
			[[windowController view] setIsHUDLagFrameCountVisible:hudShowLagFrameCount];
			[[windowController view] setIsHUDCPULoadAverageVisible:hudShowCPULoadAverage];
			[[windowController view] setIsHUDRealTimeClockVisible:hudShowRTC];
			[[windowController view] setIsHUDInputVisible:hudShowInput];
			
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
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	NSArray *windowList = [emuControl windowList];
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
											  [NSNumber numberWithInteger:[windowController screenshotFileFormat]], @"screenshotFileFormat",
											  [NSNumber numberWithInteger:[[windowController view] outputFilter]], @"videoOutputFilter",
											  [NSNumber numberWithBool:[[windowController view] sourceDeposterize]], @"videoSourceDeposterize",
											  [NSNumber numberWithBool:[[windowController view] useVerticalSync]], @"useVerticalSync",
											  [NSNumber numberWithBool:[[windowController view] isHUDVisible]], @"hudEnable",
											  [NSNumber numberWithBool:[[windowController view] isHUDVideoFPSVisible]], @"hudShowVideoFPS",
											  [NSNumber numberWithBool:[[windowController view] isHUDRender3DFPSVisible]], @"hudShowRender3DFPS",
											  [NSNumber numberWithBool:[[windowController view] isHUDFrameIndexVisible]], @"hudShowFrameIndex",
											  [NSNumber numberWithBool:[[windowController view] isHUDLagFrameCountVisible]], @"hudShowLagFrameCount",
											  [NSNumber numberWithBool:[[windowController view] isHUDCPULoadAverageVisible]], @"hudShowCPULoadAverage",
											  [NSNumber numberWithBool:[[windowController view] isHUDRealTimeClockVisible]], @"hudShowRTC",
											  [NSNumber numberWithBool:[[windowController view] isHUDInputVisible]], @"hudShowInput",
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
			
			// TODO: Show HUD Input.
			//[NSNumber numberWithBool:[[windowController view] isHUDInputVisible]], @"hudShowInput",
			
			[windowPropertiesList addObject:windowProperties];
		}
		
		[[NSUserDefaults standardUserDefaults] setObject:windowPropertiesList forKey:@"General_DisplayWindowRestorableStates"];
	}
	else
	{
		[[NSUserDefaults standardUserDefaults] removeObjectForKey:@"General_DisplayWindowRestorableStates"];
	}
}

@end
