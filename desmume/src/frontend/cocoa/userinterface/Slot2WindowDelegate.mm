/*
	Copyright (C) 2014 DeSmuME Team

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

#import "Slot2WindowDelegate.h"
#import "InputManager.h"
#import "preferencesWindowDelegate.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"


@implementation Slot2WindowDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize deviceListController;
@synthesize deviceListTable;
@synthesize deviceSettingsBox;
@synthesize mpcfFileSearchMenu;
@synthesize prefWindowDelegate;

@synthesize viewUnsupported;
@synthesize viewNoSelection;
@synthesize viewNone;
@synthesize viewAuto;
@synthesize viewCompactFlash;
@synthesize viewRumblePak;
@synthesize viewGBACartridge;
@synthesize viewGuitarGrip;
@synthesize viewMemoryExpansionPack;
@synthesize viewPiano;
@synthesize viewPaddleController;
@synthesize viewPassME;

@synthesize selectedDevice;
@synthesize deviceManager;
@synthesize hidManager;

@synthesize autoSelectedDeviceText;
@dynamic mpcfFolderURL;
@dynamic mpcfDiskImageURL;
@dynamic mpcfFolderName;
@dynamic mpcfFolderPath;
@dynamic mpcfDiskImageName;
@dynamic mpcfDiskImagePath;
@dynamic gbaCartridgeURL;
@dynamic gbaCartridgeName;
@dynamic gbaCartridgePath;
@synthesize gbaSRamURL;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	selectedDevice = nil;
	deviceManager = [[[[CocoaDSSlot2Manager alloc] init] retain] autorelease];
	hidManager = nil;
	currentDeviceView = viewNoSelection;
	
	autoSelectedDeviceText = @"";
	mpcfFolderURL = nil;
	mpcfDiskImageURL = nil;
	gbaCartridgeURL = nil;
	gbaSRamURL = nil;
	
	// This needs to respond to force feedback notifications.
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(sendForceFeedback:)
												 name:@"org.desmume.DeSmuME.sendForceFeedback"
											   object:nil];
	
	return self;
}

- (void)dealloc
{
	[self setSelectedDevice:nil];
	[self setDeviceManager:nil];
	[self setHidManager:nil];
	[self setAutoSelectedDeviceText:nil];
	[self setMpcfFolderURL:nil];
	[self setMpcfDiskImageURL:nil];
	[self setGbaCartridgeURL:nil];
	[self setGbaSRamURL:nil];
	
	[super dealloc];
}

- (IBAction) applySettings:(id)sender
{
	CocoaDSSlot2Device *theDevice = [self selectedDevice];
	const NDS_SLOT2_TYPE selectedDeviceType = (theDevice != nil) ? [theDevice type] : NDS_SLOT2_NONE;
	
	if (selectedDeviceType == NDS_SLOT2_CFLASH)
	{
		NSURL *theURL = nil;
		NSInteger mpcfPathOption = [[NSUserDefaults standardUserDefaults] integerForKey:@"Slot2_MPCF_PathOption"];
		
		switch (mpcfPathOption)
		{
			case MPCF_OPTION_LOAD_WITH_ROM:
				theURL = nil;
				break;
				
			case MPCF_OPTION_LOAD_DIRECTORY:
				theURL = [self mpcfFolderURL];
				break;
				
			case MPCF_OPTION_LOAD_DISK_IMAGE:
				theURL = [self mpcfDiskImageURL];
				break;
				
			default:
				break;
		}
		
		[deviceManager setMpcfFileSearchURL:theURL];
	}
	else if (selectedDeviceType == NDS_SLOT2_GBACART)
	{
		[deviceManager setGbaCartridgeURL:[self gbaCartridgeURL]];
		[deviceManager setGbaSRamURL:[self gbaSRamURL]];
	}
	
	[[self deviceManager] setCurrentDevice:theDevice];
	[[NSUserDefaults standardUserDefaults] setInteger:[theDevice type] forKey:@"Slot2_LoadedDevice"];
}

- (IBAction) showInputPreferences:(id)sender
{
	[[prefWindowDelegate toolbar] setSelectedItemIdentifier:[[prefWindowDelegate toolbarItemInput] itemIdentifier]];
	[prefWindowDelegate changePrefView:sender];
	[[prefWindowDelegate window] makeKeyAndOrderFront:sender];
}

- (void) update
{
	[deviceManager updateDeviceList];
	[deviceListController setContent:[deviceManager deviceList]];
	[deviceListController setSelectedObjects:[NSArray arrayWithObject:[deviceManager currentDevice]]];
}

- (void) selectDeviceByType:(NSInteger)theType
{
	CocoaDSSlot2Device *theDevice = [[self deviceManager] findDeviceByType:(NDS_SLOT2_TYPE)theType];
	if (theDevice != nil)
	{
		[deviceListController setSelectedObjects:[NSArray arrayWithObject:theDevice]];
	}
}

- (void) setDeviceViewByDevice:(CocoaDSSlot2Device *)theDevice
{
	NSView *newView = viewNoSelection;
	const BOOL isDeviceEnabled = [theDevice enabled];
	const NDS_SLOT2_TYPE deviceType = [theDevice type];
	
	if (currentDeviceView == nil)
	{
		currentDeviceView = viewNoSelection;
	}
	
	if (isDeviceEnabled)
	{
		switch (deviceType)
		{
			case NDS_SLOT2_NONE:
				newView = viewNone;
				break;
				
			case NDS_SLOT2_AUTO:
				newView = viewAuto;
				break;
				
			case NDS_SLOT2_CFLASH:
				newView = viewCompactFlash;
				break;
				
			case NDS_SLOT2_RUMBLEPAK:
				newView = viewRumblePak;
				break;
				
			case NDS_SLOT2_GBACART:
				newView = viewGBACartridge;
				break;
				
			case NDS_SLOT2_GUITARGRIP:
				newView = viewGuitarGrip;
				break;
				
			case NDS_SLOT2_EXPMEMORY:
				newView = viewMemoryExpansionPack;
				break;
				
			case NDS_SLOT2_EASYPIANO:
				newView = viewPiano;
				break;
				
			case NDS_SLOT2_PADDLE:
				newView = viewPaddleController;
				break;
				
			case NDS_SLOT2_PASSME:
				newView = viewPassME;
				break;
				
			default:
				break;
		}
	}
	else
	{
		newView = viewUnsupported;
	}
	
	if (newView != nil)
	{
		NSRect frameRect = [currentDeviceView frame];
		[currentDeviceView retain];
		[deviceSettingsBox replaceSubview:currentDeviceView with:newView];
		currentDeviceView = newView;
		[currentDeviceView setFrame:frameRect];
	}
}

- (void) setupUserDefaults
{
	[self setMpcfFolderURL:[NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"Slot2_MPCF_DirectoryPath"]]];
	[self setMpcfDiskImageURL:[NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"Slot2_MPCF_DiskImagePath"]]];
	[self setGbaCartridgeURL:[NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"Slot2_GBA_CartridgePath"]]];
	[self setGbaSRamURL:[NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"Slot2_GBA_SRAMPath"]]];
	[self selectDeviceByType:[[NSUserDefaults standardUserDefaults] integerForKey:@"Slot2_LoadedDevice"]];
	[self applySettings:nil];
}

#pragma mark -
#pragma mark Auto



#pragma mark -
#pragma mark Compact Flash

- (void) setMpcfFolderURL:(NSURL *)theURL
{
	[mpcfFolderURL release];
	mpcfFolderURL = [theURL retain];
	
	NSString *thePath = [mpcfFolderURL path];
	[self setMpcfFolderPath:thePath];
	[self setMpcfFolderName:[thePath lastPathComponent]];
}

- (NSURL *) mpcfFolderURL
{
	return mpcfFolderURL;
}

- (void) setMpcfFolderName:(NSString *)theName
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) mpcfFolderName
{
	NSString *folderPath = [self mpcfFolderPath];
	return (folderPath != nil) ? [folderPath lastPathComponent] : NSSTRING_STATUS_NO_FOLDER_CHOSEN;
}

- (void) setMpcfFolderPath:(NSString *)thePath
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) mpcfFolderPath
{
	return (mpcfFolderURL != nil) ? [mpcfFolderURL path] : nil;
}

- (void) setMpcfDiskImageURL:(NSURL *)theURL
{
	[mpcfDiskImageURL release];
	mpcfDiskImageURL = [theURL retain];
	
	NSString *thePath = [mpcfDiskImageURL path];
	[self setMpcfDiskImagePath:thePath];
	[self setMpcfDiskImageName:[thePath lastPathComponent]];
}

- (NSURL *) mpcfDiskImageURL
{
	return mpcfDiskImageURL;
}

- (void) setMpcfDiskImageName:(NSString *)theName
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) mpcfDiskImageName
{
	NSString *diskImagePath = [self mpcfDiskImagePath];
	return (diskImagePath != nil) ? [diskImagePath lastPathComponent] : NSSTRING_STATUS_NO_DISK_IMAGE_CHOSEN;
}

- (void) setMpcfDiskImagePath:(NSString *)thePath
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) mpcfDiskImagePath
{
	return (mpcfDiskImageURL != nil) ? [mpcfDiskImageURL path] : nil;
}

- (IBAction) chooseMPCFPath:(id)sender
{
	const NSInteger mpcfOptionTag = [CocoaDSUtil getIBActionSenderTag:sender];
	
	if (mpcfOptionTag == MPCF_OPTION_LOAD_WITH_ROM)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:mpcfOptionTag forKey:@"Slot2_MPCF_PathOption"];
		return;
	}
	
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	NSArray *fileTypes = nil;
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	
	if (mpcfOptionTag == MPCF_ACTION_CHOOSE_DIRECTORY)
	{
		[panel setTitle:NSSTRING_TITLE_SELECT_MPCF_FOLDER_PANEL];
		[panel setCanChooseDirectories:YES];
		[panel setCanChooseFiles:NO];
	}
	else if (mpcfOptionTag == MPCF_ACTION_CHOOSE_DISK_IMAGE)
	{
		[panel setTitle:NSSTRING_TITLE_SELECT_MPCF_DISK_IMAGE_PANEL];
		[panel setCanChooseDirectories:NO];
		[panel setCanChooseFiles:YES];
		fileTypes = [NSArray arrayWithObjects:@"dmg", @"img", nil];
	}
	else
	{
		return;
	}
	
	NSNumber *mpcfOptionNumber = [[NSNumber numberWithInteger:mpcfOptionTag] retain]; // Released in chooseMPCFPathDidEnd:returnCode:contextInfo:
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseMPCFPathDidEnd:panel returnCode:result contextInfo:mpcfOptionNumber];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseMPCFPathDidEnd:returnCode:contextInfo:)
					  contextInfo:mpcfOptionNumber];
#endif
}

- (void) chooseMPCFPathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	const NSInteger prevMpcfOption = [[NSUserDefaults standardUserDefaults] integerForKey:@"Slot2_MPCF_PathOption"];
	const NSInteger mpcfOptionTag = [(NSNumber *)contextInfo integerValue];
	[(NSNumber *)contextInfo release]; // Retained in chooseMPCFPath:
	
	[sheet orderOut:self];
	
	// Temporarily set the MPCF path option in user defaults to some neutral value first and synchronize.
	// When the user defaults are actually set later, this will force the proper state transitions to occur.
	[[NSUserDefaults standardUserDefaults] setInteger:mpcfOptionTag forKey:@"Slot2_MPCF_PathOption"];
	[[NSUserDefaults standardUserDefaults] synchronize];
	
	if (returnCode == NSCancelButton)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:prevMpcfOption forKey:@"Slot2_MPCF_PathOption"];
		return;
	}
	
	NSURL *selectedURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedURL == nil)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:prevMpcfOption forKey:@"Slot2_MPCF_PathOption"];
		return;
	}
	
	if (mpcfOptionTag == MPCF_ACTION_CHOOSE_DIRECTORY)
	{
		[self setMpcfFolderURL:selectedURL];
		[[NSUserDefaults standardUserDefaults] setObject:[selectedURL path] forKey:@"Slot2_MPCF_DirectoryPath"];
		[[NSUserDefaults standardUserDefaults] setInteger:MPCF_OPTION_LOAD_DIRECTORY forKey:@"Slot2_MPCF_PathOption"];
	}
	else if (mpcfOptionTag == MPCF_ACTION_CHOOSE_DISK_IMAGE)
	{
		[self setMpcfDiskImageURL:selectedURL];
		[[NSUserDefaults standardUserDefaults] setObject:[selectedURL path] forKey:@"Slot2_MPCF_DiskImagePath"];
		[[NSUserDefaults standardUserDefaults] setInteger:MPCF_OPTION_LOAD_DISK_IMAGE forKey:@"Slot2_MPCF_PathOption"];
	}
}

#pragma mark -
#pragma mark GBA Cartridge

- (void) setGbaCartridgeURL:(NSURL *)theURL
{
	[gbaCartridgeURL release];
	gbaCartridgeURL = [theURL retain];
	
	NSString *thePath = [gbaCartridgeURL path];
	[self setGbaCartridgePath:thePath];
	[self setGbaCartridgeName:[thePath lastPathComponent]];
}

- (NSURL *) gbaCartridgeURL
{
	return gbaCartridgeURL;
}

- (void) setGbaCartridgeName:(NSString *)theName
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) gbaCartridgeName
{
	NSString *gbaCartPath = [self gbaCartridgePath];
	return (gbaCartPath != nil) ? [gbaCartPath lastPathComponent] : NSSTRING_STATUS_NO_GBA_CART_CHOSEN;
}

- (void) setGbaCartridgePath:(NSString *)thePath
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) gbaCartridgePath
{
	return (gbaCartridgeURL != nil) ? [gbaCartridgeURL path] : nil;
}

- (void) setGbaSRamURL:(NSURL *)theURL
{
	[gbaSRamURL release];
	gbaSRamURL = [theURL retain];
	
	NSString *thePath = [gbaSRamURL path];
	[self setGbaSRamPath:thePath];
	[self setGbaSRamName:[thePath lastPathComponent]];
}

- (NSURL *) gbaSRamURL
{
	return gbaSRamURL;
}

- (void) setGbaSRamName:(NSString *)theName
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) gbaSRamName
{
	NSString *sramPath = [self gbaSRamPath];
	return (sramPath != nil) ? [sramPath lastPathComponent] : NSSTRING_STATUS_NO_GBA_SRAM_CHOSEN;
}

- (void) setGbaSRamPath:(NSString *)thePath
{
	// Do nothing. This is for KVO-compliance only.
}

- (NSString *) gbaSRamPath
{
	return (gbaSRamURL != nil) ? [gbaSRamURL path] : nil;
}

- (IBAction) chooseGbaCartridgePath:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_CHOOSE_GBA_CARTRIDGE_PANEL];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_GBA_ROM, nil];
		
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseGbaCartridgePathDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseGbaCartridgePathDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseGbaCartridgePathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectURL == nil)
	{
		return;
	}
	
	[self setGbaCartridgeURL:selectURL];
	[[NSUserDefaults standardUserDefaults] setObject:[selectURL path] forKey:@"Slot2_GBA_CartridgePath"];
	
	if ([self isGbaSRamWithCartridge])
	{
		NSString *sramPath = [NSString stringWithFormat:@"%@.%s", [[selectURL path] stringByDeletingPathExtension], FILE_EXT_GBA_SRAM];
		[self setGbaSRamURL:[NSURL fileURLWithPath:sramPath]];
		[[NSUserDefaults standardUserDefaults] setObject:sramPath forKey:@"Slot2_GBA_SRAMPath"];
	}
	else
	{
		[self clearSRamPath:self];
	}
}

- (IBAction) chooseGbaSRamPath:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_CHOOSE_GBA_SRAM_PANEL];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_GBA_SRAM, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseGbaSRamPathDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseGbaSRamPathDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseGbaSRamPathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectURL == nil)
	{
		return;
	}
	
	[self setGbaSRamURL:selectURL];
	[[NSUserDefaults standardUserDefaults] setObject:[selectURL path] forKey:@"Slot2_GBA_SRAMPath"];
}

- (IBAction) clearSRamPath:(id)sender
{
	[self setGbaSRamURL:nil];
	[[NSUserDefaults standardUserDefaults] setObject:nil forKey:@"Slot2_GBA_SRAMPath"];
}

- (BOOL) isGbaSRamWithCartridge
{
	BOOL result = NO;
	NSString *gbaCartPath = [self gbaCartridgePath];
	
	if (gbaCartPath == nil)
	{
		return result;
	}
	
	NSString *sramPath = [NSString stringWithFormat:@"%@.%s", [gbaCartPath stringByDeletingPathExtension], FILE_EXT_GBA_SRAM];
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	result = [fileManager isReadableFileAtPath:sramPath];
	[fileManager release];
	
	return result;
}

#pragma mark -
#pragma mark Rumble Pak

- (IBAction) testRumble:(id)sender
{
	NSDictionary *ffProperties = [NSDictionary dictionaryWithObjectsAndKeys:
								  [NSNumber numberWithBool:YES], @"ffState",
								  [NSNumber numberWithInteger:RUMBLE_ITERATIONS_TEST], @"iterations",
								  nil];
	
	[[NSNotificationCenter defaultCenter] postNotificationName:@"org.desmume.DeSmuME.sendForceFeedback"
														object:nil
													  userInfo:ffProperties];
}

- (void) sendForceFeedback:(NSNotification *)aNotification
{
	if ([self hidManager] == nil)
	{
		return;
	}
	
	NSDictionary *ffProperties = (NSDictionary *)[aNotification userInfo];
	const BOOL ffState = [(NSNumber *)[ffProperties valueForKey:@"ffState"] boolValue];
	const UInt32 iterations = [(NSNumber *)[ffProperties valueForKey:@"iterations"] unsignedIntValue];
	
	NSMutableArray *inputDeviceList = [[[self hidManager] deviceListController] arrangedObjects];
	
	for (InputHIDDevice *inputDevice in inputDeviceList)
	{
		if ([inputDevice isForceFeedbackEnabled])
		{
			if (ffState)
			{
				[inputDevice startForceFeedbackAndIterate:iterations flags:0];
			}
			else
			{
				[inputDevice stopForceFeedback];
			}
		}
	}
}

#pragma mark -
#pragma mark Paddle


#pragma mark -
#pragma mark NSTableViewDelegate Protocol

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
	NSTableView *table = (NSTableView *)[aNotification object];
	NSInteger rowIndex = [table selectedRow];
	
	if (rowIndex >= 0)
	{
		CocoaDSSlot2Device *theDevice = nil;
		NSArray *selectedObjects = [deviceListController selectedObjects];
		
		if ([selectedObjects count] > 0)
		{
			theDevice = (CocoaDSSlot2Device *)[selectedObjects objectAtIndex:0];
			[self setSelectedDevice:theDevice];
		}
		
		[self setDeviceViewByDevice:theDevice];
	}
	else
	{
		NSRect frameRect = [currentDeviceView frame];
		[currentDeviceView retain];
		[deviceSettingsBox replaceSubview:currentDeviceView with:viewNoSelection];
		currentDeviceView = viewNoSelection;
		[currentDeviceView setFrame:frameRect];
	}
}

@end
