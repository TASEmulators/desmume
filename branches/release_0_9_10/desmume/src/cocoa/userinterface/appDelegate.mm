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

#import "appDelegate.h"
#import "DisplayWindowController.h"
#import "EmuControllerDelegate.h"
#import "FileMigrationDelegate.h"
#import "preferencesWindowDelegate.h"
#import "troubleshootingWindowDelegate.h"
#import "cheatWindowDelegate.h"
#import "inputPrefsView.h"

#import "cocoa_core.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_globals.h"
#import "cocoa_input.h"
#import "cocoa_rom.h"
#import "cocoa_util.h"


@implementation AppDelegate

@dynamic dummyObject;
@synthesize prefWindow;
@synthesize troubleshootingWindow;
@synthesize cheatListWindow;
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
@synthesize cheatWindowController;
@synthesize migrationDelegate;
@synthesize inputManager;

@synthesize boxGeneralInfo;
@synthesize boxTitles;
@synthesize boxARMBinaries;
@synthesize boxFileSystem;
@synthesize boxMisc;

@synthesize isAppRunningOnIntel;


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
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	PreferencesWindowDelegate *prefWindowDelegate = (PreferencesWindowDelegate *)[prefWindow delegate];
	CheatWindowDelegate *cheatWindowDelegate = (CheatWindowDelegate *)[cheatListWindow delegate];
	
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
	
	// Register the application's defaults.
	NSDictionary *prefsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultUserPrefs" ofType:@"plist"]];
	if (prefsDict == nil)
	{
		[[NSAlert alertWithMessageText:NSSTRING_ALERT_CRITICAL_FILE_MISSING_PRI defaultButton:nil alternateButton:nil otherButton:nil informativeTextWithFormat:NSSTRING_ALERT_CRITICAL_FILE_MISSING_SEC] runModal];
		[NSApp terminate:nil];
		return;
	}
	
	[[NSUserDefaults standardUserDefaults] registerDefaults:prefsDict];
	
	// Change the title colors of the NSBox objects in the ROM Info panel. We change the
	// colors manually here because you can't change them in Interface Builder. Boo!!!
	[self setRomInfoPanelBoxTitleColors];
	
	// Set the preferences window to the general view by default.
	[prefWindowDelegate switchContentView:prefGeneralView];
	
	// Setup the slot menu items. We set this up manually instead of through Interface
	// Builder because we're assuming an arbitrary number of slot items.
	[self setupSlotMenuItems];
	
	// Init the DS emulation core.
	CocoaDSCore *newCore = [[[CocoaDSCore alloc] init] autorelease];
	
	// Init the DS controller.
	CocoaDSController *newController = [[[CocoaDSController alloc] init] autorelease];
	[newCore setCdsController:newController];
	
	// Init the DS speakers.
	CocoaDSSpeaker *newSpeaker = [[[CocoaDSSpeaker alloc] init] autorelease];
	[newCore addOutput:newSpeaker];
	[emuControl setCdsSpeaker:newSpeaker];
	
	// Start the core thread.
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:newCore withObject:nil];
	
	// Wait until the emulation core has finished starting up.
	while (!([CocoaDSCore isCoreStarted] && [newCore thread] != nil))
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
	
	// Start up the threads for our outputs.
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:newSpeaker withObject:nil];
	
	// Wait until the SPU is finished starting up.
	while ([newSpeaker thread] == nil)
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
	
	// Set up all the object controllers.
	[cdsCoreController setContent:newCore];
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	[prefWindowController setContent:[prefWindowDelegate bindings]];
	[cheatWindowController setContent:[cheatWindowDelegate bindings]];
	
	// Setup the applications settings from the user defaults file.
	[self setupUserDefaults];
	
	[inputPrefsView initSettingsSheets];
	[inputPrefsView populateInputProfileMenu];
	[[inputPrefsView inputProfileMenu] selectItemAtIndex:0];
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
	
	//Bring the application to the front
	[NSApp activateIgnoringOtherApps:TRUE];
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
	[[NSUserDefaults standardUserDefaults] setDouble:[emuControl lastSetSpeedScalar] forKey:@"CoreControl_SpeedScalar"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore isSpeedLimitEnabled] forKey:@"CoreControl_EnableSpeedLimit"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore isFrameSkipEnabled] forKey:@"CoreControl_EnableAutoFrameSkip"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore isCheatingEnabled] forKey:@"CoreControl_EnableCheats"];
	
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
	NSMutableDictionary *prefBindings = [prefWindowDelegate bindings];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	// Set the emulation flags.
	NSUInteger emuFlags = 0;
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_AdvancedBusLevelTiming"])
	{
		emuFlags |= EMULATION_ADVANCED_BUS_LEVEL_TIMING_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_RigorousTiming"])
	{
		emuFlags |= EMULATION_RIGOROUS_TIMING_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_UseExternalBIOSImages"])
	{
		emuFlags |= EMULATION_USE_EXTERNAL_BIOS_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_BIOSEmulateSWI"])
	{
		emuFlags |= EMULATION_BIOS_SWI_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_BIOSPatchDelayLoopSWI"])
	{
		emuFlags |= EMULATION_PATCH_DELAY_LOOP_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_UseExternalFirmwareImage"])
	{
		emuFlags |= EMULATION_USE_EXTERNAL_FIRMWARE_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_FirmwareBoot"])
	{
		emuFlags |= EMULATION_BOOT_FROM_FIRMWARE_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_EmulateEnsata"])
	{
		emuFlags |= EMULATION_ENSATA_MASK;
	}
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_UseDebugConsole"])
	{
		emuFlags |= EMULATION_DEBUG_CONSOLE_MASK;
	}
	
	[cdsCore setEmulationFlags:emuFlags];
	
	// If we're not running on Intel, force the CPU emulation engine to use the interpreter engine.
	if (!isAppRunningOnIntel)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:CPU_EMULATION_ENGINE_INTERPRETER forKey:@"Emulation_CPUEmulationEngine"];
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
		[prefBindings setValue:[arm7BiosImagePath lastPathComponent] forKey:@"Arm7BiosImageName"];
	}
	else
	{
		[cdsCore setArm7ImageURL:nil];
		[prefBindings setValue:nil forKey:@"Arm7BiosImageName"];
	}
	
	NSString *arm9BiosImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"BIOS_ARM9ImagePath"];
	if (arm9BiosImagePath != nil)
	{
		[cdsCore setArm9ImageURL:[NSURL fileURLWithPath:arm9BiosImagePath]];
		[prefBindings setValue:[arm9BiosImagePath lastPathComponent] forKey:@"Arm9BiosImageName"];
	}
	else
	{
		[cdsCore setArm9ImageURL:nil];
		[prefBindings setValue:nil forKey:@"Arm9BiosImageName"];
	}
	
	NSString *firmwareImagePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"Emulation_FirmwareImagePath"];
	if (firmwareImagePath != nil)
	{
		[cdsCore setFirmwareImageURL:[NSURL fileURLWithPath:firmwareImagePath]];
		[prefBindings setValue:[firmwareImagePath lastPathComponent] forKey:@"FirmwareImageName"];
	}
	else
	{
		[cdsCore setFirmwareImageURL:nil];
		[prefBindings setValue:nil forKey:@"FirmwareImageName"];
	}
	
	NSString *advansceneDatabasePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"Advanscene_DatabasePath"];
	if (advansceneDatabasePath != nil)
	{
		[prefBindings setValue:[advansceneDatabasePath lastPathComponent] forKey:@"AdvansceneDatabaseName"];
	}
	
	NSString *cheatDatabasePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"R4Cheat_DatabasePath"];
	if (cheatDatabasePath != nil)
	{
		[prefBindings setValue:[cheatDatabasePath lastPathComponent] forKey:@"R4CheatDatabaseName"];
	}
	
	// Update the SPU Sync controls in the Preferences window.
	if ([[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"] == SPU_SYNC_MODE_DUAL_SYNC_ASYNC)
	{
		[[prefWindowDelegate spuSyncMethodMenu] setEnabled:NO];
	}
	else
	{
		[[prefWindowDelegate spuSyncMethodMenu] setEnabled:YES];
	}
	
	// Set the name of the autoloaded ROM.
	NSString *autoloadRomPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"General_AutoloadROMSelectedPath"];
	if (autoloadRomPath != nil)
	{
		[prefBindings setValue:[autoloadRomPath lastPathComponent] forKey:@"AutoloadRomName"];
	}
	else
	{
		[prefBindings setValue:NSSTRING_STATUS_AUTOLOAD_ROM_NAME_NONE forKey:@"AutoloadRomName"];
	}
	
	// Set the menu for the display rotation.
	const double displayRotation = (double)[[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Rotation"];
	[prefWindowDelegate updateDisplayRotationMenu:displayRotation];
	
	// Set the default sound volume per user preferences.
	[prefWindowDelegate updateVolumeIcon:nil];
	
	// Set up the user's default input settings.
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	if (userMappings == nil)
	{
		NSDictionary *defaultKeyMappingsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
		NSArray *internalDefaultProfilesList = (NSArray *)[defaultKeyMappingsDict valueForKey:@"DefaultInputProfiles"];
		userMappings = [(NSDictionary *)[internalDefaultProfilesList objectAtIndex:0] valueForKey:@"Mappings"];
	}
	
	[inputManager setMappingsWithMappings:userMappings];
	
	// Set up the rest of the emulation-related user defaults.
	[emuControl setupUserDefaults];
}

- (void) setRomInfoPanelBoxTitleColors
{
	NSColor *boxTitleColor = [NSColor whiteColor];
	
	[[boxGeneralInfo titleCell] setTextColor:boxTitleColor];
	[[boxTitles titleCell] setTextColor:boxTitleColor];
	[[boxARMBinaries titleCell] setTextColor:boxTitleColor];
	[[boxFileSystem titleCell] setTextColor:boxTitleColor];
	[[boxMisc titleCell] setTextColor:boxTitleColor];
	
	[boxGeneralInfo setNeedsDisplay:YES];
	[boxTitles setNeedsDisplay:YES];
	[boxARMBinaries setNeedsDisplay:YES];
	[boxFileSystem setNeedsDisplay:YES];
	[boxMisc setNeedsDisplay:YES];
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
			
			const NSInteger displayMode				= [(NSNumber *)[windowProperties valueForKey:@"displayMode"] integerValue];
			const double displayScale				= [(NSNumber *)[windowProperties valueForKey:@"displayScale"] doubleValue];
			const double displayRotation			= [(NSNumber *)[windowProperties valueForKey:@"displayRotation"] doubleValue];
			const NSInteger displayOrientation		= [(NSNumber *)[windowProperties valueForKey:@"displayOrientation"] integerValue];
			const NSInteger displayOrder			= [(NSNumber *)[windowProperties valueForKey:@"displayOrder"] integerValue];
			const double displayGap					= [(NSNumber *)[windowProperties valueForKey:@"displayGap"] doubleValue];
			const NSInteger videoFilterType			= [(NSNumber *)[windowProperties valueForKey:@"videoFilterType"] integerValue];
			const NSInteger screenshotFileFormat	= [(NSNumber *)[windowProperties valueForKey:@"screenshotFileFormat"] integerValue];
			const BOOL useBilinearOutput			= [(NSNumber *)[windowProperties valueForKey:@"useBilinearOutput"] boolValue];
			const BOOL useVerticalSync				= [(NSNumber *)[windowProperties valueForKey:@"useVerticalSync"] boolValue];
			const BOOL isMinSizeNormal				= [(NSNumber *)[windowProperties valueForKey:@"isMinSizeNormal"] boolValue];
			const BOOL isShowingStatusBar			= [(NSNumber *)[windowProperties valueForKey:@"isShowingStatusBar"] boolValue];
			const BOOL isInFullScreenMode			= [(NSNumber *)[windowProperties valueForKey:@"isInFullScreenMode"] boolValue];
			const NSUInteger screenIndex			= [(NSNumber *)[windowProperties valueForKey:@"screenIndex"] unsignedIntegerValue];
			NSString *windowFrameStr				= (NSString *)[windowProperties valueForKey:@"windowFrame"];
			
			int frameX = 0;
			int frameY = 0;
			int frameWidth = GPU_DISPLAY_WIDTH;
			int frameHeight = GPU_DISPLAY_HEIGHT;
			const char *frameCStr = [windowFrameStr cStringUsingEncoding:NSUTF8StringEncoding];
			sscanf(frameCStr, "%i %i %i %i", &frameX, &frameY, &frameWidth, &frameHeight);
			
			[windowController setIsMinSizeNormal:isMinSizeNormal];
			[windowController setIsShowingStatusBar:isShowingStatusBar];
			[windowController setVideoFilterType:videoFilterType];
			[windowController setDisplayMode:displayMode];
			[windowController setDisplayRotation:displayRotation];
			[windowController setDisplayOrientation:displayOrientation];
			[windowController setDisplayOrder:displayOrder];
			[windowController setDisplayGap:displayGap];
			[windowController setScreenshotFileFormat:screenshotFileFormat];
			[windowController setUseBilinearOutput:useBilinearOutput];
			[windowController setUseVerticalSync:useVerticalSync];
			[windowController setDisplayScale:displayScale];
			
			[[windowController masterWindow] setFrameOrigin:NSMakePoint(frameX, frameY)];
			
			// If this is the last window in the list, make this window key and main.
			// Otherwise, just order the window to the front so that the windows will
			// stack in a deterministic order.
			if (windowProperties == [windowPropertiesList lastObject])
			{
				[[windowController window] makeKeyAndOrderFront:self];
				[[windowController window] makeMainWindow];
			}
			else
			{
				[[windowController window] orderFront:self];
			}
			
			// Draw the display view now so that we guarantee that its drawn at least once.
			if ([emuControl currentRom] == nil)
			{
				[[windowController view] clearToBlack];
			}
			else
			{
				[[windowController view] setNeedsDisplay:YES];
			}
			
			// If this window is set to full screen mode, its associated screen index must
			// exist. If not, this window will not enter full screen mode. This is necessary,
			// since the user's screen configuration could change in between app launches,
			// and since we don't want a window to go full screen on the wrong screen.
			if (isInFullScreenMode &&
				([[NSScreen screens] indexOfObject:[[windowController window] screen]] == screenIndex))
			{
				[windowController enterFullScreen];
				[[windowController window] makeKeyAndOrderFront:self];
				[[windowController window] makeMainWindow];
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
			const BOOL isInFullScreenMode = ([windowController assignedScreen] != nil);
			const NSUInteger screenIndex = [[NSScreen screens] indexOfObject:[[windowController masterWindow] screen]];
			
			const NSRect windowFrame = [[windowController masterWindow] frame];
			NSString *windowFrameStr = [NSString stringWithFormat:@"%i %i %i %i",
										(int)windowFrame.origin.x, (int)windowFrame.origin.y, (int)windowFrame.size.width, (int)windowFrame.size.height];
			
			NSDictionary *windowProperties = [NSDictionary dictionaryWithObjectsAndKeys:
											  [NSNumber numberWithInteger:[windowController displayMode]], @"displayMode",
											  [NSNumber numberWithDouble:[windowController displayScale]], @"displayScale",
											  [NSNumber numberWithDouble:[windowController displayRotation]], @"displayRotation",
											  [NSNumber numberWithInteger:[windowController displayOrientation]], @"displayOrientation",
											  [NSNumber numberWithInteger:[windowController displayOrder]], @"displayOrder",
											  [NSNumber numberWithDouble:[windowController displayGap]], @"displayGap",
											  [NSNumber numberWithInteger:[windowController videoFilterType]], @"videoFilterType",
											  [NSNumber numberWithInteger:[windowController screenshotFileFormat]], @"screenshotFileFormat",
											  [NSNumber numberWithBool:[windowController useBilinearOutput]], @"useBilinearOutput",
											  [NSNumber numberWithBool:[windowController useVerticalSync]], @"useVerticalSync",
											  [NSNumber numberWithBool:[windowController isMinSizeNormal]], @"isMinSizeNormal",
											  [NSNumber numberWithBool:[windowController isShowingStatusBar]], @"isShowingStatusBar",
											  [NSNumber numberWithBool:isInFullScreenMode], @"isInFullScreenMode",
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

@end
