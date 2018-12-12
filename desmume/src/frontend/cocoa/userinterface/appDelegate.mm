/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2018 DeSmuME Team

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
#import "MacAVCaptureTool.h"
#import "WifiSettingsPanel.h"
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
@synthesize mLoadStateSlot;
@synthesize mSaveStateSlot;
@synthesize inputPrefsView;
@synthesize aboutWindowController;
@synthesize emuControlController;
@synthesize prefWindowController;
@synthesize cdsCoreController;
@synthesize avCaptureToolDelegate;
@synthesize wifiSettingsPanelDelegate;
@synthesize migrationDelegate;

@synthesize isAppRunningOnIntel;
@synthesize isDeveloperPlusBuild;
@synthesize didApplicationFinishLaunching;
@synthesize delayedROMFileName;

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
	
	didApplicationFinishLaunching = NO;
	delayedROMFileName = nil;
	
	RGBA8888ToNSColorValueTransformer *nsColorTransformer = [[[RGBA8888ToNSColorValueTransformer alloc] init] autorelease];
	[NSValueTransformer setValueTransformer:nsColorTransformer forName:@"RGBA8888ToNSColorValueTransformer"];
	
	return self;
}

#pragma mark NSApplicationDelegate Protocol
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	BOOL result = NO;
	
	// Apparently, we can't be too sure when macOS will try to call this method if the user tries to
	// open a ROM file while launching the app (i.e. the user double-clicks a ROM file from the Finder).
	// Will this method be called before or after [NSApplicationDelgate applicationDidFinishLaunching:]
	// is finished? Who knows? And so since we really don't know when this method will be called, we will
	// keep a flag for ourselves that is set whenever [NSApplicationDelgate applicationDidFinishLaunching:]
	// actually finishes. This way, we can prevent any race conditions that may occur by ensuring that the
	// app finishes launching BEFORE trying to load a ROM file.
	//
	// If this method is called AFTER [NSApplicationDelgate applicationDidFinishLaunching:] finishes, then
	// nothing special happens and everything works as it normally does.
	//
	// If this method is called BEFORE [NSApplicationDelgate applicationDidFinishLaunching:] finishes, then
	// simply save the passed in file name, and then automatically call this method again with our
	// previously saved file name the moment before [NSApplicationDelgate applicationDidFinishLaunching:]
	// finishes.
	//
	// We'll do the delayed ROM loading only for Mavericks and later, since this delayed loading code
	// doesn't seem to work on older macOS versions.
	
	if (IsOSXVersionSupported(10, 9, 0) && ![self didApplicationFinishLaunching])
	{
		[self setDelayedROMFileName:filename];
		result = YES;
		return result;
	}
	
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
	
	// Create the needed directories in Application Support if they haven't already
	// been created.
	if (![CocoaDSFile setupAllAppDirectories])
	{
		// Need to show a modal dialog here.
		return;
	}
	
	[CocoaDSFile setupAllFilePaths];
	
	// On macOS v10.13 and later, some unwanted menu items will show up in the View menu.
	// Disable automatic window tabbing for all NSWindows in order to rid ourselves of
	// these unwanted menu items.
#if defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	if ([[NSWindow class] respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:)])
	{
		[NSWindow setAllowsAutomaticWindowTabbing:NO];
	}
#endif
	
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
	
	[avCaptureToolDelegate setCdsCore:newCore];
	[avCaptureToolDelegate setExecControl:[newCore execControl]];
	
	[wifiSettingsPanelDelegate setExecControl:[newCore execControl]];
	[wifiSettingsPanelDelegate fillLibpcapDeviceMenu];
	
	// Init the DS speakers.
	CocoaDSSpeaker *newSpeaker = [[[CocoaDSSpeaker alloc] init] autorelease];
	[newCore addOutput:newSpeaker];
	[emuControl setCdsSpeaker:newSpeaker];
	
	// Set up all the object controllers.
	[cdsCoreController setContent:newCore];
	[prefWindowController setContent:[prefWindowDelegate bindings]];
	
	[emuControl appInit];
	[prefWindowDelegate markUnsupportedOpenGLMSAAMenuItems];
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
	[emuControl restoreDisplayWindowStates];
	
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
	
	// If the user is trying to load a ROM file while launching the app, then ensure that the
	// ROM file is loaded at the end of this method and never any time before that.
	[self setDidApplicationFinishLaunching:YES];
	if (IsOSXVersionSupported(10, 9, 0) && [self delayedROMFileName] != nil)
	{
		[self application:NSApp openFile:[self delayedROMFileName]];
		[self setDelayedROMFileName:nil];
	}
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
	[emuControl saveDisplayWindowStates];
	[emuControl writeUserDefaults];
	
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
	[self setDelayedROMFileName:nil];
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
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
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
	CocoaDSFirmware *newFirmware = [[[CocoaDSFirmware alloc] init] autorelease];
	BOOL needUserDefaultSynchronize = NO;
	uint32_t defaultMACAddress_u32 = 0;
	
	[newFirmware setExecControl:[cdsCore execControl]];
	
	if ([[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_FirmwareMACAddress"] != nil)
	{
		defaultMACAddress_u32 = (uint32_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_FirmwareMACAddress"];
	}
	
	if (defaultMACAddress_u32 == 0)
	{
		defaultMACAddress_u32 = [newFirmware firmwareMACAddressPendingValue];
		[[NSUserDefaults standardUserDefaults] setInteger:defaultMACAddress_u32 forKey:@"FirmwareConfig_FirmwareMACAddress"];
		needUserDefaultSynchronize = YES;
	}
	else
	{
		[newFirmware setFirmwareMACAddressPendingValue:defaultMACAddress_u32];
	}
	
	defaultMACAddress_u32 = 0;
	if ([[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_CustomMACAddress"] != nil)
	{
		defaultMACAddress_u32 = (uint32_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_CustomMACAddress"];
	}
	
	if (defaultMACAddress_u32 == 0)
	{
		defaultMACAddress_u32 = [newFirmware customMACAddressValue];
		[[NSUserDefaults standardUserDefaults] setInteger:defaultMACAddress_u32 forKey:@"FirmwareConfig_CustomMACAddress"];
		needUserDefaultSynchronize = YES;
	}
	else
	{
		[newFirmware setCustomMACAddressValue:defaultMACAddress_u32];
	}
	
	if (needUserDefaultSynchronize)
	{
		[[NSUserDefaults standardUserDefaults] synchronize];
	}
	
	[wifiSettingsPanelDelegate updateCustomMACAddressStrings];
	
	[newFirmware setIpv4Address_AP1_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP1_1"]];
	[newFirmware setIpv4Address_AP1_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP1_2"]];
	[newFirmware setIpv4Address_AP1_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP1_3"]];
	[newFirmware setIpv4Address_AP1_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP1_4"]];
	[newFirmware setIpv4Gateway_AP1_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP1_1"]];
	[newFirmware setIpv4Gateway_AP1_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP1_2"]];
	[newFirmware setIpv4Gateway_AP1_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP1_3"]];
	[newFirmware setIpv4Gateway_AP1_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP1_4"]];
	[newFirmware setIpv4PrimaryDNS_AP1_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_1"]];
	[newFirmware setIpv4PrimaryDNS_AP1_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_2"]];
	[newFirmware setIpv4PrimaryDNS_AP1_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_3"]];
	[newFirmware setIpv4PrimaryDNS_AP1_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP1_4"]];
	[newFirmware setIpv4SecondaryDNS_AP1_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_1"]];
	[newFirmware setIpv4SecondaryDNS_AP1_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_2"]];
	[newFirmware setIpv4SecondaryDNS_AP1_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_3"]];
	[newFirmware setIpv4SecondaryDNS_AP1_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP1_4"]];
	[newFirmware setSubnetMask_AP1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_SubnetMask_AP1"]];
	
	[newFirmware setIpv4Address_AP2_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP2_1"]];
	[newFirmware setIpv4Address_AP2_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP2_2"]];
	[newFirmware setIpv4Address_AP2_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP2_3"]];
	[newFirmware setIpv4Address_AP2_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP2_4"]];
	[newFirmware setIpv4Gateway_AP2_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP2_1"]];
	[newFirmware setIpv4Gateway_AP2_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP2_2"]];
	[newFirmware setIpv4Gateway_AP2_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP2_3"]];
	[newFirmware setIpv4Gateway_AP2_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP2_4"]];
	[newFirmware setIpv4PrimaryDNS_AP2_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_1"]];
	[newFirmware setIpv4PrimaryDNS_AP2_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_2"]];
	[newFirmware setIpv4PrimaryDNS_AP2_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_3"]];
	[newFirmware setIpv4PrimaryDNS_AP2_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP2_4"]];
	[newFirmware setIpv4SecondaryDNS_AP2_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_1"]];
	[newFirmware setIpv4SecondaryDNS_AP2_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_2"]];
	[newFirmware setIpv4SecondaryDNS_AP2_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_3"]];
	[newFirmware setIpv4SecondaryDNS_AP2_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP2_4"]];
	[newFirmware setSubnetMask_AP2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_SubnetMask_AP2"]];
	
	[newFirmware setIpv4Address_AP3_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP3_1"]];
	[newFirmware setIpv4Address_AP3_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP3_2"]];
	[newFirmware setIpv4Address_AP3_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP3_3"]];
	[newFirmware setIpv4Address_AP3_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Address_AP3_4"]];
	[newFirmware setIpv4Gateway_AP3_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP3_1"]];
	[newFirmware setIpv4Gateway_AP3_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP3_2"]];
	[newFirmware setIpv4Gateway_AP3_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP3_3"]];
	[newFirmware setIpv4Gateway_AP3_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4Gateway_AP3_4"]];
	[newFirmware setIpv4PrimaryDNS_AP3_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_1"]];
	[newFirmware setIpv4PrimaryDNS_AP3_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_2"]];
	[newFirmware setIpv4PrimaryDNS_AP3_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_3"]];
	[newFirmware setIpv4PrimaryDNS_AP3_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4PrimaryDNS_AP3_4"]];
	[newFirmware setIpv4SecondaryDNS_AP3_1:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_1"]];
	[newFirmware setIpv4SecondaryDNS_AP3_2:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_2"]];
	[newFirmware setIpv4SecondaryDNS_AP3_3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_3"]];
	[newFirmware setIpv4SecondaryDNS_AP3_4:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_IPv4SecondaryDNS_AP3_4"]];
	[newFirmware setSubnetMask_AP3:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_SubnetMask_AP3"]];
	
	//[newFirmware setConsoleType:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_ConsoleType"]];
	[newFirmware setNickname:[[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Nickname"]];
	[newFirmware setMessage:[[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Message"]];
	[newFirmware setFavoriteColor:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_FavoriteColor"]];
	[newFirmware setBirthday:[[NSUserDefaults standardUserDefaults] objectForKey:@"FirmwareConfig_Birthday"]];
	[newFirmware setLanguage:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_Language"]];
	//[newFirmware setBacklightLevel:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_BacklightLevel"]];
	
	[prefWindowDelegate updateFirmwareMACAddressString:nil];
	[prefWindowDelegate updateSubnetMaskString_AP1:nil];
	[prefWindowDelegate updateSubnetMaskString_AP2:nil];
	[prefWindowDelegate updateSubnetMaskString_AP3:nil];
	
	[newFirmware applySettings];
	[cdsCore updateFirmwareMACAddressString];
	[emuControl setCdsFirmware:newFirmware];
	
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
	
	// Set up the rest of the emulation-related user defaults.
	[emuControl readUserDefaults];
	[wifiSettingsPanelDelegate readUserDefaults];
	
	// Set up the preferences window.
	[prefWindowDelegate setupUserDefaults];
}

@end
