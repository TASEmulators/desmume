/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2013 DeSmuME team

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

#import "preferencesWindowDelegate.h"
#import "EmuControllerDelegate.h"

#import "cocoa_core.h"
#import "cocoa_cheat.h"
#import "cocoa_globals.h"
#import "cocoa_input.h"
#import "cocoa_file.h"
#import "cocoa_videofilter.h"
#import "cocoa_util.h"


@implementation PreferencesWindowDelegate

@synthesize dummyObject;
@synthesize window;
@synthesize firmwareConfigSheet;
@synthesize cdsCoreController;
@synthesize emuController;
@synthesize prefWindowController;
@synthesize cheatWindowController;
@synthesize cheatDatabaseController;

@synthesize viewGeneral;
@synthesize viewInput;
@synthesize viewDisplay;
@synthesize viewSound;
@synthesize viewEmulation;

@synthesize displayRotationMenu;
@synthesize displayRotationMenuCustomItem;
@synthesize displayRotationField;
@synthesize spuSyncMethodMenu;

@synthesize previewImageView;

@synthesize bindings;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	bindings = [[NSMutableDictionary alloc] init];
	if (bindings == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	VideoFilterTypeID vfType = (VideoFilterTypeID)[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"];
	NSImage *videoFilterImage = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"VideoFilterPreview_64x64" ofType:@"png"]];
	
	videoFilter = [[CocoaVideoFilter alloc] initWithSize:[videoFilterImage size] typeID:vfType];
	NSSize vfDestSize = [videoFilter destSize];
	NSUInteger vfWidth = (NSUInteger)vfDestSize.width;
	NSUInteger vfHeight = (NSUInteger)vfDestSize.height;
	
	NSArray *imageRepArray = [videoFilterImage representations];
	const NSBitmapImageRep *imageRep = [imageRepArray objectAtIndex:0];
	RGBA8888ForceOpaqueBuffer((const uint32_t *)[imageRep bitmapData], (uint32_t *)[videoFilter srcBufferPtr], (64 * 64));
	[videoFilterImage release];
	
	BOOL useBilinear = [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseBilinearOutput"];
	
	if (vfWidth <= 64 || vfHeight <= 64)
	{
		[videoFilter changeFilter:VideoFilterTypeID_Nearest2X];
		vfDestSize = [videoFilter destSize];
		vfWidth = (NSUInteger)vfDestSize.width;
		vfHeight = (NSUInteger)vfDestSize.height;
		
		bilinearVideoFilter = [[CocoaVideoFilter alloc] initWithSize:vfDestSize typeID:(useBilinear) ? VideoFilterTypeID_Bilinear : VideoFilterTypeID_Nearest2X];
	}
	else if  (vfWidth >= 256 || vfHeight >= 256)
	{
		bilinearVideoFilter = [[CocoaVideoFilter alloc] initWithSize:vfDestSize typeID:VideoFilterTypeID_None];
	}
	else
	{
		bilinearVideoFilter = [[CocoaVideoFilter alloc] initWithSize:vfDestSize typeID:(useBilinear) ? VideoFilterTypeID_Bilinear : VideoFilterTypeID_Nearest2X];
	}
	
	RGBA8888ForceOpaqueBuffer((const uint32_t *)[videoFilter runFilter], (uint32_t *)[bilinearVideoFilter srcBufferPtr], (vfWidth * vfHeight));
	[bindings setObject:[bilinearVideoFilter image] forKey:@"VideoFilterPreviewImage"];	
		
	iconVolumeFull = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeFull_16x16" ofType:@"png"]];
	iconVolumeTwoThird = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeTwoThird_16x16" ofType:@"png"]];
	iconVolumeOneThird = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeOneThird_16x16" ofType:@"png"]];
	iconVolumeMute = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeMute_16x16" ofType:@"png"]];
	
	[bindings setObject:iconVolumeFull forKey:@"volumeIconImage"];
	
	return self;
}

- (void)dealloc
{
	[iconVolumeFull release];
	[iconVolumeTwoThird release];
	[iconVolumeOneThird release];
	[iconVolumeMute release];
	[bindings release];
	[videoFilter release];
	
	[super dealloc];
}

- (IBAction) showGeneralView:(id)sender
{
	[self switchContentView:viewGeneral];
}

- (IBAction) showInputView:(id)sender
{
	[self switchContentView:(NSView *)viewInput];
	[window makeFirstResponder:(NSView *)viewInput];
}

- (IBAction) showDisplayView:(id)sender
{
	[self switchContentView:viewDisplay];
}

- (IBAction) showSoundView:(id)sender
{
	[self switchContentView:viewSound];
}

- (IBAction) showEmulationView:(id)sender
{
	[self switchContentView:viewEmulation];
}

- (IBAction) chooseRomForAutoload:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ROM_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ROM_DS, @FILE_EXT_ROM_GBA, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseRomForAutoloadDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseRomForAutoloadDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseRomForAutoloadDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	// Temporarily set the autoload ROM option in user defaults to some neutral value first and synchronize.
	// When the user defaults are actually set later, this will force the proper state transitions to occur.
	[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_CHOOSE_ROM forKey:@"General_AutoloadROMOption"];
	[[NSUserDefaults standardUserDefaults] synchronize];
	
	if (returnCode == NSCancelButton)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_NONE forKey:@"General_AutoloadROMOption"];
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_NONE forKey:@"General_AutoloadROMOption"];
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setInteger:ROMAUTOLOADOPTION_LOAD_SELECTED forKey:@"General_AutoloadROMOption"];
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"General_AutoloadROMSelectedPath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"AutoloadRomName"];
}

- (IBAction) chooseAdvansceneDatabase:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ADVANSCENE_DB_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ADVANSCENE_DB, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseAdvansceneDatabaseDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseAdvansceneDatabaseDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseAdvansceneDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"Advanscene_DatabasePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"AdvansceneDatabaseName"];
}

- (IBAction) chooseCheatDatabase:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_R4_CHEAT_DB_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_R4_CHEAT_DB, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseCheatDatabaseDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseCheatDatabaseDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseCheatDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"R4Cheat_DatabasePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"R4CheatDatabaseName"];
	
	const BOOL isRomLoaded = [(EmuControllerDelegate *)[emuController content] currentRom] != nil;
	NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
	CocoaDSCheatManager *cdsCheats = (CocoaDSCheatManager *)[cheatWindowBindings valueForKey:@"cheatList"];
	
	if (isRomLoaded == YES && cdsCheats != nil)
	{
		NSInteger error = 0;
		NSMutableArray *dbList = [cdsCheats cheatListFromDatabase:selectedFileURL errorCode:&error];
		if (dbList != nil)
		{
			[cheatDatabaseController setContent:dbList];
			
			NSString *titleString = cdsCheats.dbTitle;
			NSString *dateString = cdsCheats.dbDate;
			
			[cheatWindowBindings setValue:titleString forKey:@"cheatDBTitle"];
			[cheatWindowBindings setValue:dateString forKey:@"cheatDBDate"];
			[cheatWindowBindings setValue:[NSString stringWithFormat:@"%ld", (unsigned long)[dbList count]] forKey:@"cheatDBItemCount"];
		}
		else
		{
			// TODO: Display an error message here.
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
}

- (IBAction) selectDisplayRotation:(id)sender
{
	const NSInteger displayRotation = [(NSMenuItem *)sender tag];
	if (displayRotation != -1)
	{
		[[NSUserDefaults standardUserDefaults] setDouble:displayRotation forKey:@"DisplayView_Rotation"];
	}
}

- (IBAction) setUseBilinear:(id)sender
{
	const BOOL useBilinear = [CocoaDSUtil getIBActionSenderButtonStateBool:sender];
	const NSUInteger previewSrcWidth = (NSUInteger)[bilinearVideoFilter srcSize].width;
	const NSUInteger previewSrcHeight = (NSUInteger)[bilinearVideoFilter srcSize].height;
	
	if (previewSrcWidth <= 128 || previewSrcHeight <= 128)
	{
		[bilinearVideoFilter changeFilter:(useBilinear) ? VideoFilterTypeID_Bilinear : VideoFilterTypeID_Nearest2X];
	}
	
	NSBitmapImageRep *newPreviewImageRep = [bilinearVideoFilter bitmapImageRep];
	
	NSImage *videoFilterPreviewImage = [bindings objectForKey:@"VideoFilterPreviewImage"];
	NSArray *imageRepArray = [videoFilterPreviewImage representations];
	NSImageRep *oldImageRep = [imageRepArray objectAtIndex:0];
	[videoFilterPreviewImage removeRepresentation:oldImageRep];
	[videoFilterPreviewImage addRepresentation:newPreviewImageRep];
	
	[previewImageView setNeedsDisplay:YES];
}

- (IBAction) selectVideoFilterType:(id)sender
{
	const VideoFilterTypeID vfType = (VideoFilterTypeID)[CocoaDSUtil getIBActionSenderTag:sender];
	const BOOL useBilinear = [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseBilinearOutput"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:vfType forKey:@"DisplayView_VideoFilter"];
	
	[videoFilter changeFilter:vfType];
	NSSize vfDestSize = [videoFilter destSize];
	NSUInteger vfWidth = (NSUInteger)[videoFilter destSize].width;
	NSUInteger vfHeight = (NSUInteger)[videoFilter destSize].height;
	
	if (vfWidth <= 64 || vfHeight <= 64)
	{
		[videoFilter changeFilter:VideoFilterTypeID_Nearest2X];
		vfDestSize = [videoFilter destSize];
		vfWidth = (NSUInteger)vfDestSize.width;
		vfHeight = (NSUInteger)vfDestSize.height;
		
		[bilinearVideoFilter setSourceSize:vfDestSize];
		[bilinearVideoFilter changeFilter:(useBilinear) ? VideoFilterTypeID_Bilinear : VideoFilterTypeID_Nearest2X];
	}
	else if (vfWidth >= 256 || vfHeight >= 256)
	{
		[bilinearVideoFilter setSourceSize:vfDestSize];
		[bilinearVideoFilter changeFilter:VideoFilterTypeID_None];
	}
	else
	{
		[bilinearVideoFilter setSourceSize:vfDestSize];
		[bilinearVideoFilter changeFilter:(useBilinear) ? VideoFilterTypeID_Bilinear : VideoFilterTypeID_Nearest2X];
	}
	
	RGBA8888ForceOpaqueBuffer((const uint32_t *)[videoFilter runFilter], (uint32_t *)[bilinearVideoFilter srcBufferPtr], (vfWidth * vfHeight));
	NSBitmapImageRep *newPreviewImageRep = [bilinearVideoFilter bitmapImageRep];
	
	NSImage *videoFilterPreviewImage = [bindings objectForKey:@"VideoFilterPreviewImage"];
	NSArray *imageRepArray = [videoFilterPreviewImage representations];
	NSImageRep *oldImageRep = [imageRepArray objectAtIndex:0];
	[videoFilterPreviewImage removeRepresentation:oldImageRep];
	[videoFilterPreviewImage addRepresentation:newPreviewImageRep];
	
	[previewImageView setNeedsDisplay:YES];
}

- (IBAction) updateVolumeIcon:(id)sender
{
	NSImage *iconImage = (NSImage *)[bindings objectForKey:@"volumeIconImage"];
	const float vol = [[NSUserDefaults standardUserDefaults] floatForKey:@"Sound_Volume"];
	
	if (vol <= 0.0f)
	{
		if (iconImage == iconVolumeMute)
		{
			return;
		}
		
		iconImage = iconVolumeMute;
	}
	else if (vol > 0.0f && vol <= VOLUME_THRESHOLD_LOW)
	{
		if (iconImage == iconVolumeOneThird)
		{
			return;
		}
		
		iconImage = iconVolumeOneThird;
	}
	else if (vol > VOLUME_THRESHOLD_LOW && vol <= VOLUME_THRESHOLD_HIGH)
	{
		if (iconImage == iconVolumeTwoThird)
		{
			return;
		}
		
		iconImage = iconVolumeTwoThird;
	}
	else
	{
		if (iconImage == iconVolumeFull)
		{
			return;
		}
		
		iconImage = iconVolumeFull;
	}
	
	[bindings setObject:iconImage forKey:@"volumeIconImage"];
}

- (IBAction) selectSPUSyncMode:(id)sender
{
	const NSInteger spuSyncMode = [[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"];
	[spuSyncMethodMenu setEnabled:(spuSyncMode == SPU_SYNC_MODE_DUAL_SYNC_ASYNC) ? NO : YES];
}

- (IBAction) selectSPUSyncMethod:(id)sender
{
	const NSInteger spuSyncMethod = [(NSMenuItem *)sender tag];
	[[NSUserDefaults standardUserDefaults] setInteger:spuSyncMethod forKey:@"SPU_SyncMethod"];
}

- (void) updateDisplayRotationMenu:(double)displayRotation
{
	if (displayRotation == 0.0f ||
		displayRotation == 90.0f ||
		displayRotation == 180.0f ||
		displayRotation == 270.0f)
	{
		[displayRotationMenu selectItemWithTag:(NSInteger)displayRotation];
	}
	else if (displayRotation < 0.0f || displayRotation >= 360.0f)
	{
		displayRotation = 0.0f;
		[displayRotationField setFloatValue:displayRotation];
		[displayRotationMenu selectItemWithTag:(NSInteger)displayRotation];
	}
	else
	{
		[displayRotationMenu selectItem:displayRotationMenuCustomItem];
	}
}

- (IBAction) chooseARM9BiosImage:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ARM9_IMAGE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_HW_IMAGE_FILE, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseArm9BiosImageDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseArm9BiosImageDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (IBAction) chooseARM7BiosImage:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_ARM7_IMAGE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_HW_IMAGE_FILE, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseArm7BiosImageDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseArm7BiosImageDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (IBAction) chooseFirmwareImage:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_SELECT_FIRMWARE_IMAGE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_HW_IMAGE_FILE, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	[panel beginSheetModalForWindow:window
				  completionHandler:^(NSInteger result) {
					  [self chooseFirmwareImageDidEnd:panel returnCode:result contextInfo:nil];
				  } ];
#else
	[panel beginSheetForDirectory:nil
							 file:nil
							types:fileTypes
				   modalForWindow:window
					modalDelegate:self
				   didEndSelector:@selector(chooseFirmwareImageDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
#endif
}

- (void) chooseArm9BiosImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"BIOS_ARM9ImagePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"Arm9BiosImageName"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setArm9ImageURL:selectedFileURL];
}

- (void) chooseArm7BiosImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"BIOS_ARM7ImagePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"Arm7BiosImageName"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setArm7ImageURL:selectedFileURL];
}

- (void) chooseFirmwareImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == NSCancelButton)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	if(selectedFileURL == nil)
	{
		return;
	}
	
	NSString *selectedFile = [selectedFileURL path];
	
	[[NSUserDefaults standardUserDefaults] setObject:selectedFile forKey:@"Emulation_FirmwareImagePath"];
	[bindings setValue:[selectedFile lastPathComponent] forKey:@"FirmwareImageName"];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setFirmwareImageURL:selectedFileURL];
}

- (IBAction) configureInternalFirmware:(id)sender
{
	[NSApp beginSheet:firmwareConfigSheet
	   modalForWindow:window
		modalDelegate:self
	   didEndSelector:@selector(didEndFirmwareConfigSheet:returnCode:contextInfo:)
		  contextInfo:nil];
}

- (IBAction) closeFirmwareConfigSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	const NSInteger code = [CocoaDSUtil getIBActionSenderTag:sender];
	
	// Force end of editing of any text fields.
	[sheet makeFirstResponder:nil];
	
    [NSApp endSheet:sheet returnCode:code];
}

- (void) didEndFirmwareConfigSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
	[self updateDisplayRotationMenu:[displayRotationField floatValue]];
}

- (void) switchContentView:(NSView *)theView
{
	if ([window contentView] == theView)
	{
		return;
	}
	
	NSRect newFrame = [window frameRectForContentRect:[theView frame]];
	newFrame.origin.x = [window frame].origin.x;
	newFrame.origin.y = [window frame].origin.y + [[window contentView] frame].size.height - [theView frame].size.height;
	
	NSView *tempView = [[NSView alloc] initWithFrame:[[window contentView] frame]];
	[window setContentView:tempView];
	
	[window setFrame:newFrame display:YES animate:YES];
	[window setContentView:theView];
	
	[tempView release];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	[prefWindowController setContent:bindings];
	
	if ([viewInput configInputTargetID] != nil)
	{
		[[viewInput inputManager] setHidInputTarget:viewInput];
	}
}

- (void)windowWillClose:(NSNotification *)notification
{
	[viewInput setConfigInputTargetID:nil];
}

@end
