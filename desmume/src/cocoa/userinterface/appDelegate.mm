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

#import "appDelegate.h"
#import "emuWindowDelegate.h"
#import "preferencesWindowDelegate.h"
#import "cheatWindowDelegate.h"
#import "displayView.h"
#import "inputPrefsView.h"

#import "cocoa_core.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_globals.h"
#import "cocoa_hid.h"
#import "cocoa_input.h"
#import "cocoa_mic.h"


@implementation AppDelegate

@dynamic dummyObject;
@synthesize mainWindow;
@synthesize prefWindow;
@synthesize cheatListWindow;
@synthesize migrationWindow;
@synthesize prefGeneralView;
@synthesize mLoadStateSlot;
@synthesize mSaveStateSlot;
@synthesize inputPrefsView;
@synthesize fileMigrationList;
@synthesize aboutWindowController;
@synthesize emuWindowController;
@synthesize prefWindowController;
@synthesize cdsCoreController;
@synthesize cheatWindowController;

@synthesize boxGeneralInfo;
@synthesize boxTitles;
@synthesize boxARMBinaries;
@synthesize boxFileSystem;
@synthesize boxMisc;

@synthesize hidManager;
@synthesize migrationFilesPresent;
@synthesize isAppRunningOnIntel;


- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	BOOL result = NO;
	NSURL *fileURL = [NSURL fileURLWithPath:filename];
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if (cdsCore == nil)
	{
		return result;
	}
	
	NSString *fileKind = [CocoaDSFile fileKindByURL:fileURL];
	if ([fileKind isEqualToString:@"ROM"])
	{
		result = [mainWindowDelegate handleLoadRom:fileURL];
		if ([mainWindowDelegate isShowingSaveStateSheet] || [mainWindowDelegate isShowingFileMigrationSheet])
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
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	PreferencesWindowDelegate *prefWindowDelegate = [prefWindow delegate];
	CheatWindowDelegate *cheatWindowDelegate = [cheatListWindow delegate];
	
	// Determine if we're running on Intel or PPC.
#if defined(__i386__) || defined(__x86_64__)
	isAppRunningOnIntel = YES;
#else
	isAppRunningOnIntel = NO;
#endif
	
	// Create the needed directories in Application Support if they haven't already
	// been created.
	if (![CocoaDSFile setupAllAppDirectories])
	{
		// Need to show a modal dialog here.
		return;
	}
	
	[CocoaDSFile setupAllFilePaths];
	
	// Setup the About window.
	NSString *buildVersionStr = @"Build Version: ";
	buildVersionStr = [buildVersionStr stringByAppendingString:[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"]];
	NSString *buildDateStr = @"Build Date: ";
	buildDateStr = [buildDateStr stringByAppendingString:@__DATE__];
	
	NSMutableDictionary *aboutWindowProperties = [NSMutableDictionary dictionaryWithObjectsAndKeys:
												  [[NSBundle mainBundle] pathForResource:@FILENAME_README ofType:@""], @"readMePath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_COPYING ofType:@""], @"licensePath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_AUTHORS ofType:@""], @"authorsPath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_CHANGELOG ofType:@""], @"changeLogPath",
												  buildVersionStr, @"versionString",
												  buildDateStr, @"dateString",
												  nil];
	
	[aboutWindowController setContent:aboutWindowProperties];
	
	// Register the application's defaults.
	NSDictionary *prefsDict = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultUserPrefs" ofType:@"plist"]];
	if (prefsDict == nil)
	{
		[[NSAlert alertWithMessageText:NSSTRING_ALERT_CRITICAL_FILE_MISSING_PRI defaultButton:nil alternateButton:nil otherButton:nil informativeTextWithFormat:NSSTRING_ALERT_CRITICAL_FILE_MISSING_SEC] runModal];
		[NSApp terminate: nil];
		return;
	}
	
	[[NSUserDefaults standardUserDefaults] registerDefaults:prefsDict];
	
	// Change the title colors of the NSBox objects in the ROM Info panel. We change the
	// colors manually here because you can't change them in Interface Builder. Boo!!!
	[self setRomInfoPanelBoxTitleColors];
	
	// Set up all the object controllers according to our default windows.
	[emuWindowController setContent:[mainWindowDelegate bindings]];
	[prefWindowController setContent:[prefWindowDelegate bindings]];
	[cheatWindowController setContent:[cheatWindowDelegate bindings]];
	
	// Set the preferences window to the general view by default.
	[prefWindowDelegate switchContentView:prefGeneralView];
	
	// Setup the slot menu items. We set this up manually instead of through Interface
	// Builder because we're assuming an arbitrary number of slot items.
	[self setupSlotMenuItems];
	
	// Setup the HID Input manager.
	[self setHidManager:[[[CocoaHIDManager alloc] init] autorelease]];
	
	// Set the display view delegate.
	DisplayViewDelegate *newDispViewDelegate = [[[DisplayViewDelegate alloc] init] autorelease];
	[newDispViewDelegate setView:[mainWindowDelegate displayView]];
	[mainWindowDelegate setDispViewDelegate:newDispViewDelegate];
	[[mainWindowDelegate displayView] setDispViewDelegate:newDispViewDelegate];
	
	// Init the DS emulation core.
	CocoaDSCore *newCore = [[[CocoaDSCore alloc] init] autorelease];
	[cdsCoreController setContent:newCore];
	
	// Init the DS controller and microphone.
	CocoaDSMic *newMic = [[[CocoaDSMic alloc] init] autorelease];
	CocoaDSController *newController = [[[CocoaDSController alloc] initWithMic:newMic] autorelease];
	[newCore setCdsController:newController];
	[inputPrefsView setCdsController:newController];
	[newDispViewDelegate setCdsController:newController];
	
	// Init the DS displays.
	CocoaDSDisplay *newComboDisplay = [[[CocoaDSDisplay alloc] init] autorelease];
	[newComboDisplay setDelegate:newDispViewDelegate];
	[newCore addOutput:newComboDisplay];
	NSPort *guiPort = [NSPort port];
	[[NSRunLoop currentRunLoop] addPort:guiPort forMode:NSDefaultRunLoopMode];
	
	// Init the DS speakers.
	CocoaDSSpeaker *newSpeaker = [[[CocoaDSSpeaker alloc] init] autorelease];
	[newCore addOutput:newSpeaker];
	[mainWindowDelegate setCdsSpeaker:newSpeaker];
	
	// Start the core thread.
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:newCore withObject:nil];
	
	// Wait until the emulation core has finished starting up.
	while (!([CocoaDSCore isCoreStarted] && [newCore thread] != nil))
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
	
	// Start up the threads for our outputs.
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:newComboDisplay withObject:nil];
	[NSThread detachNewThreadSelector:@selector(runThread:) toTarget:newSpeaker withObject:nil];
	
	// Wait until the GPU and SPU are finished starting up.
	while ([newComboDisplay thread] == nil || [newSpeaker thread] == nil)
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
	
	// Setup the applications settings from the user defaults file.
	[self setupUserDefaults];
	
	// Setup the window display view.
	[[mainWindowDelegate displayView] setNextResponder:mainWindow];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	
	// Load a new ROM on launch per user preferences.
	BOOL loadROMOnLaunch = [[NSUserDefaults standardUserDefaults] boolForKey:@"General_AutoloadROMOnLaunch"];
	if (loadROMOnLaunch && ![mainWindowDelegate isRomLoaded])
	{
		NSInteger autoloadRomOption = [[NSUserDefaults standardUserDefaults] integerForKey:@"General_AutoloadROMOption"];
		NSURL *autoloadRomURL = nil;
		
		if (autoloadRomOption == ROMAUTOLOADOPTION_LOAD_LAST)
		{
			autoloadRomURL = [CocoaDSFile lastLoadedRomURL];
		}
		else if(autoloadRomOption == ROMAUTOLOADOPTION_LOAD_SELECTED)
		{
			NSString *autoloadRomPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"General_AutoloadROMSelectedPath"];
			if (autoloadRomPath != nil && [autoloadRomPath length] > 0)
			{
				autoloadRomURL = [NSURL fileURLWithPath:autoloadRomPath];
			}
		}
		
		if (autoloadRomURL != nil)
		{
			[mainWindowDelegate handleLoadRom:autoloadRomURL];
		}
	}
	
	// Make the display view black.
	[[mainWindowDelegate dispViewDelegate] setViewToBlack];
	
	//Bring the application to the front
	[NSApp activateIgnoringOtherApps:TRUE];
	[mainWindow makeKeyAndOrderFront:nil];
	[mainWindow makeMainWindow];
	
	// Present the file migration window to the user (if they haven't disabled it).
	[self setMigrationFilesPresent:NO];
	if (![[NSUserDefaults standardUserDefaults] boolForKey:@"General_DoNotAskMigrate"])
	{
		[self showMigrationWindow:nil];
	}
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	
	// If a file needs to be saved, give the user a chance to save it
	// before quitting.
	BOOL didRomClose = [mainWindowDelegate handleUnloadRom:REASONFORCLOSE_TERMINATE romToLoad:nil];
	if (!didRomClose)
	{
		if ([mainWindowDelegate isShowingSaveStateSheet])
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
	[cdsCoreController setContent:nil];
}

- (IBAction) showSupportFolderInFinder:(id)sender
{
	NSURL *folderURL = [CocoaDSFile userAppSupportBaseURL];
	
	[[NSWorkspace sharedWorkspace] openFile:[folderURL path] withApplication:@"Finder"];
}

- (IBAction) launchWebsite:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_WEBSITE]];
}

- (IBAction) launchForums:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_FORUM_SITE]];
}

- (IBAction) bugReport:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_BUG_SITE]];
}

- (void) setupSlotMenuItems
{
	NSInteger i;
	NSMenuItem *loadItem = nil;
	NSMenuItem *saveItem = nil;
	
	for(i = 0; i < MAX_SAVESTATE_SLOTS; i++)
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
	NSUInteger slotNumberKey = slotNumber;
	
	if (slotNumber == 10)
	{
		slotNumberKey = 0;
	}
	
	NSMenuItem *mItem = [menu addItemWithTitle:[NSString stringWithFormat:NSSTRING_TITLE_SLOT_NUMBER, slotNumber]
										action:nil
								 keyEquivalent:[NSString stringWithFormat:@"%d", slotNumberKey]];
	
	[mItem setTarget:[mainWindow delegate]];
	
	return mItem;
}

- (void) setupUserDefaults
{
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	PreferencesWindowDelegate *prefWindowDelegate = [prefWindow delegate];
	NSMutableDictionary *prefBindings = [prefWindowDelegate bindings];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	// Set the emulation flags.
	NSUInteger emuFlags = 0;
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"Emulation_AdvancedBusLevelTiming"])
	{
		emuFlags |= EMULATION_ADVANCED_BUS_LEVEL_TIMING_MASK;
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
	
	// Set up the firmware per user preferences.
	NSMutableDictionary *newFWDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Nickname"], @"Nickname",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Message"], @"Message",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_FavoriteColor"], @"FavoriteColor",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Birthday"], @"Birthday",
									  [[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Language"], @"Language",
									  nil];
	
	CocoaDSFirmware *newFirmware = [[[CocoaDSFirmware alloc] initWithDictionary:newFWDict] autorelease];
	[cdsCore setCdsFirmware:newFirmware];
	[newFirmware update];
	
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
	
	// Set the sound input mode per user preferences.
	[[cdsCore cdsController] setSoundInputMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"Input_AudioInputMode"]];
	
	// Update the SPU Sync controls in the Preferences window.
	if ([[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"] == SPU_SYNC_MODE_DUAL_SYNC_ASYNC)
	{
		[[prefWindowDelegate spuSyncMethodMenu] setEnabled:NO];
	}
	else
	{
		[[prefWindowDelegate spuSyncMethodMenu] setEnabled:YES];
	}
	
	// Set the text field for the autoloaded ROM.
	NSString *autoloadRomPath = [[NSUserDefaults standardUserDefaults] stringForKey:@"General_AutoloadROMSelectedPath"];
	if (autoloadRomPath != nil)
	{
		[prefBindings setValue:[autoloadRomPath lastPathComponent] forKey:@"AutoloadRomName"];
	}
	else
	{
		[prefBindings setValue:nil forKey:@"AutoloadRomName"];
	}
	
	// Update all of the input display fields.
	[self updateInputDisplayFields];
	
	// Set the menu for the display rotation.
	double displayRotation = (double)[[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Rotation"];
	[prefWindowDelegate updateDisplayRotationMenu:displayRotation];
	
	// Set the default sound volume per user preferences.
	[prefWindowDelegate updateVolumeIcon:nil];
	
	[mainWindowDelegate setupUserDefaults];
}

- (void) updateInputDisplayFields
{
	PreferencesWindowDelegate *prefWindowDelegate = [prefWindow delegate];
	NSMutableDictionary *prefBindings = [prefWindowDelegate bindings];
	
	if ([[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"] == nil)
	{
		return;
	}
	
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Up"] forKey:@"Input_Up"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Down"] forKey:@"Input_Down"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Left"] forKey:@"Input_Left"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Right"] forKey:@"Input_Right"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"A"] forKey:@"Input_A"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"B"] forKey:@"Input_B"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"X"] forKey:@"Input_X"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Y"] forKey:@"Input_Y"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"L"] forKey:@"Input_L"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"R"] forKey:@"Input_R"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Start"] forKey:@"Input_Start"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Select"] forKey:@"Input_Select"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Microphone"] forKey:@"Input_Microphone"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Lid"] forKey:@"Input_Lid"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Debug"] forKey:@"Input_Debug"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Speed Half"] forKey:@"Input_SpeedHalf"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Speed Double"] forKey:@"Input_SpeedDouble"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"HUD"] forKey:@"Input_HUD"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Execute"] forKey:@"Input_Execute"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Pause"] forKey:@"Input_Pause"];
	[prefBindings setValue:[inputPrefsView parseMappingDisplayString:@"Reset"] forKey:@"Input_Reset"];
}

- (IBAction) showMigrationWindow:(id)sender
{
	NSMutableArray *fileList = [CocoaDSFile completeFileList];
	if (fileList != nil)
	{
		if ([fileList count] == 0)
		{
			[self setMigrationFilesPresent:NO];
		}
		else
		{
			[self setMigrationFilesPresent:YES];
		}
		
		if (sender == nil && ![self migrationFilesPresent])
		{
			return;
		}
	}
	
	[fileMigrationList setContent:fileList];
	[migrationWindow center];
	[migrationWindow makeKeyAndOrderFront:nil];
}

- (IBAction) handleMigrationWindow:(id)sender
{
	NSInteger option = [(NSControl *)sender tag];
	NSMutableArray *fileList = [fileMigrationList content];
	
	switch (option)
	{
		case COCOA_DIALOG_DEFAULT:
			[CocoaDSFile copyFileListToCurrent:fileList];
			break;
		
		case COCOA_DIALOG_OPTION:
			[CocoaDSFile moveFileListToCurrent:fileList];
			break;
		
		default:
			break;
	}
	
	[[(NSControl *)sender window] close];
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

@end
