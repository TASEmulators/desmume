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

#import "emuWindowDelegate.h"
#import "cheatWindowDelegate.h"
#import "displayView.h"

#import "cocoa_globals.h"
#import "cocoa_core.h"
#import "cocoa_cheat.h"
#import "cocoa_file.h"
#import "cocoa_firmware.h"
#import "cocoa_output.h"
#import "cocoa_rom.h"
#import "cocoa_util.h"

#undef BOOL


@implementation EmuWindowDelegate

@dynamic dummyObject;
@synthesize window;
@synthesize saveFileMigrationSheet;
@synthesize saveStatePrecloseSheet;
@synthesize displayView;
@synthesize exportRomSavePanelAccessoryView;
@synthesize saveScreenshotPanelAccessoryView;
@synthesize currentRom;
@synthesize cdsSpeaker;
@synthesize cdsCheats;

@synthesize dispViewDelegate;
@synthesize cheatWindowDelegate;
@synthesize romInfoPanelController;
@synthesize firmwarePanelController;
@synthesize cdsCoreController;
@synthesize cdsDisplayController;
@synthesize cdsSoundController;
@synthesize emuWindowController;
@synthesize cheatWindowController;
@synthesize cheatListController;
@synthesize cheatDatabaseController;

@synthesize isSheetControllingExecution;
@synthesize isShowingSaveStateSheet;
@synthesize isShowingFileMigrationSheet;
@synthesize selectedRomSaveTypeID;

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
	
	iconExecute = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Execute_420x420" ofType:@"png"]];
	iconPause = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Pause_420x420" ofType:@"png"]];
	iconSpeedNormal = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Speed1x_420x420" ofType:@"png"]];
	iconSpeedDouble = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Speed2x_420x420" ofType:@"png"]];
	iconVolumeFull = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeFull_16x16" ofType:@"png"]];
	iconVolumeTwoThird = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeTwoThird_16x16" ofType:@"png"]];
	iconVolumeOneThird = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeOneThird_16x16" ofType:@"png"]];
	iconVolumeMute = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeMute_16x16" ofType:@"png"]];
	
	dispViewDelegate = nil;
	currentRom = nil;
	cdsSpeaker = nil;
	dummyCheatList = nil;
	isRomLoading = NO;
	isSheetControllingExecution = NO;
	isShowingSaveStateSheet = NO;
	isShowingFileMigrationSheet = NO;
	isShowingStatusBar = YES;
	statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
	minDisplayViewSize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2);
	isMinSizeNormal = YES;
	screenshotFileFormat = NSTIFFFileType;
	currentEmuSaveStateURL = nil;
	selectedExportRomSaveID = 0;
	selectedRomSaveTypeID = ROMSAVETYPE_AUTOMATIC;
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRomLoaded"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isShowingStatusBar"];
	[bindings setValue:[NSNumber numberWithFloat:MAX_VOLUME] forKey:@"volume"];
	[bindings setObject:iconVolumeFull forKey:@"volumeIconImage"];
	[bindings setValue:NSSTRING_STATUS_READY forKey:@"status"];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(loadRomDidFinish:)
												 name:@"org.desmume.DeSmuME.loadRomDidFinish"
											   object:nil];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(saveScreenshotAsFinish:)
												 name:@"org.desmume.DeSmuME.requestScreenshotDidFinish"
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
	[iconVolumeFull release];
	[iconVolumeTwoThird release];
	[iconVolumeOneThird release];
	[iconVolumeMute release];
	[bindings release];
	
	[self setDispViewDelegate:nil];
	[self setCdsCheats:nil];
	[self setCdsSpeaker:nil];
	[self setCurrentRom:nil];
	
	[super dealloc];
}

- (void) setContentScalar:(double)s
{
	[dispViewDelegate setScale:s];
	
	// Resize the window when contentScalar changes.
	NSSize drawBounds = [dispViewDelegate normalSize];
	[self resizeWithTransform:drawBounds scalar:s rotation:[dispViewDelegate rotation]];
}

- (void) setContentRotation:(double)angleDegrees
{
	double newAngleDegrees = fmod(angleDegrees, 360.0);
	if (newAngleDegrees < 0.0)
	{
		newAngleDegrees = 360.0 + newAngleDegrees;
	}
	
	if (newAngleDegrees == 360.0)
	{
		newAngleDegrees = 0.0;
	}
	
	[dispViewDelegate setRotation:newAngleDegrees];
	
	// Set the minimum content size for the window, since this will change based on rotation.
	NSSize drawBounds = minDisplayViewSize;
	NSSize minContentSize = GetTransformedBounds(drawBounds, 1.0, CLOCKWISE_DEGREES(newAngleDegrees));
	minContentSize.height += statusBarHeight;
	[window setContentMinSize:minContentSize];
	
	// Resize the window.
	NSSize oldBounds = [window frame].size;
	[self resizeWithTransform:[dispViewDelegate normalSize] scalar:[dispViewDelegate scale] rotation:newAngleDegrees];
	NSSize newBounds = [window frame].size;
	
	// If the window size didn't change, it is possible that the old and new rotation angles
	// are 180 degrees offset from each other. In this case, we'll need to force the
	// display view to update itself.
	if (oldBounds.width == newBounds.width && oldBounds.height == newBounds.height)
	{
		NSRect newContentFrame = [[window contentView] bounds];
		newContentFrame.origin.y = statusBarHeight;
		newContentFrame.size.height -= statusBarHeight;
		[displayView setFrame:newContentFrame];
	}
}

- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees
{
	// Convert angle to clockwise-direction degrees.
	angleDegrees = CLOCKWISE_DEGREES(angleDegrees);
	
	// Get the maximum scalar size within drawBounds. Constrain scalar to maxScalar if necessary.
	NSSize checkSize = GetTransformedBounds(normalBounds, 1.0, angleDegrees);
	double maxScalar = [self maxContentScalar:checkSize];
	if (scalar > maxScalar)
	{
		scalar = maxScalar;
	}
	
	// Get the new bounds for the window's content view based on the transformed draw bounds.
	NSSize transformedBounds = GetTransformedBounds(normalBounds, scalar, angleDegrees);
	
	// Get the center of the content view in screen coordinates.
	NSRect windowContentRect = [[window contentView] bounds];
	double translationX = (windowContentRect.size.width - transformedBounds.width) / 2.0;
	double translationY = ((windowContentRect.size.height - statusBarHeight) - transformedBounds.height) / 2.0;
	
	// Resize the window.
	NSRect windowFrame = [window frame];
	NSRect newFrame = [window frameRectForContentRect:NSMakeRect(windowFrame.origin.x + translationX, windowFrame.origin.y + translationY, transformedBounds.width, transformedBounds.height + statusBarHeight)];
	[window setFrame:newFrame display:YES animate:NO];
	
	return scalar;
}

- (double) maxContentScalar:(NSSize)contentBounds
{
	// Determine the maximum scale based on the visible screen size (which
	// doesn't include the menu bar or dock).
	NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
	NSRect windowFrame = [window frameRectForContentRect:NSMakeRect(0.0, 0.0, contentBounds.width, contentBounds.height + statusBarHeight)];
	
	NSSize visibleScreenBounds = { (screenFrame.size.width - (windowFrame.size.width - contentBounds.width)), (screenFrame.size.height - (windowFrame.size.height - contentBounds.height)) };
	double maxS = GetMaxScalarInBounds(contentBounds.width, contentBounds.height, visibleScreenBounds.width, visibleScreenBounds.height);
	
	return maxS;
}

- (void) setVolume:(float)vol
{
	[bindings setValue:[NSNumber numberWithFloat:vol] forKey:@"volume"];
	
	// Update the icon.
	NSImage *currentImage = [bindings valueForKey:@"volumeIconImage"];
	NSImage *newImage = nil;
	if (vol <= 0.0f)
	{
		newImage = iconVolumeMute;
	}
	else if (vol > 0.0f && vol <= VOLUME_THRESHOLD_LOW)
	{
		newImage = iconVolumeOneThird;
	}
	else if (vol > VOLUME_THRESHOLD_LOW && vol <= VOLUME_THRESHOLD_HIGH)
	{
		newImage = iconVolumeTwoThird;
	}
	else
	{
		newImage = iconVolumeFull;
	}
	
	if (newImage != currentImage)
	{
		[bindings setObject:newImage forKey:@"volumeIconImage"];
	}
}

- (float) volume
{
	return [(NSNumber *)[bindings valueForKey:@"volume"] floatValue];
}

- (void) setIsRomLoaded:(BOOL)theState
{
	[bindings setValue:[NSNumber numberWithBool:theState] forKey:@"isRomLoaded"];
}

- (BOOL) isRomLoaded
{
	return [(NSNumber *)[bindings valueForKey:@"isRomLoaded"] boolValue];
}

- (void) setStatus:(NSString *)theString
{
	[bindings setValue:theString forKey:@"status"];
}

- (NSString *) status
{
	return (NSString *)[bindings valueForKey:@"status"];
}

- (IBAction) openRom:(id)sender
{
	if (isRomLoading)
	{
		return;
	}
	
	NSURL *selectedFile = nil;
	NSInteger buttonClicked = NSFileHandlingPanelCancelButton;
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
	buttonClicked = [panel runModal];
#else
	buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
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
	
	[self handleLoadRom:selectedFile];
}

- (IBAction) closeRom:(id)sender;
{
	[self handleUnloadRom:REASONFORCLOSE_NORMAL romToLoad:nil];
}

- (IBAction) openEmuSaveState:(id)sender
{
	BOOL result = NO;
	NSURL *selectedFile = nil;
	NSInteger buttonClicked = NSFileHandlingPanelCancelButton;
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
	buttonClicked = [panel runModal];
#else
	buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
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
	
	result = [CocoaDSFile loadState:selectedFile];
	if (result == NO)
	{
		[self setStatus:NSSTRING_STATUS_SAVESTATE_LOADING_FAILED];
		[self restoreCoreState];
		return;
	}
	
	currentEmuSaveStateURL = selectedFile;
	[currentEmuSaveStateURL retain];
	[window setDocumentEdited:YES];
	
	[self restoreCoreState];
	[self setStatus:NSSTRING_STATUS_SAVESTATE_LOADED];
}

- (IBAction) saveEmuSaveState:(id)sender
{
	BOOL result = NO;
	
	if ([window isDocumentEdited] && currentEmuSaveStateURL != nil)
	{
		[self pauseCore];
		
		result = [CocoaDSFile saveState:currentEmuSaveStateURL];
		if (result == NO)
		{
			[self setStatus:NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
			return;
		}
		
		[window setDocumentEdited:YES];
		
		[self restoreCoreState];
		[self setStatus:NSSTRING_STATUS_SAVESTATE_SAVED];
	}
	else
	{
		[self saveEmuSaveStateAs:sender];
	}
}

- (IBAction) saveEmuSaveStateAs:(id)sender
{
	BOOL result = NO;
	NSInteger buttonClicked = NSFileHandlingPanelCancelButton;
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
	
	buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		[self pauseCore];
		
		NSURL *saveFileURL = [panel URL];
		
		result = [CocoaDSFile saveState:saveFileURL];
		if (result == NO)
		{
			[self setStatus:NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
			return;
		}
		
		currentEmuSaveStateURL = saveFileURL;
		[currentEmuSaveStateURL retain];
		[window setDocumentEdited:YES];
		
		[self restoreCoreState];
		[self setStatus:NSSTRING_STATUS_SAVESTATE_SAVED];
	}
}

- (IBAction) revertEmuSaveState:(id)sender
{
	BOOL result = NO;
	
	if ([window isDocumentEdited] && currentEmuSaveStateURL != nil)
	{
		[self pauseCore];
		
		result = [CocoaDSFile loadState:currentEmuSaveStateURL];
		if (result == NO)
		{
			[self setStatus:NSSTRING_STATUS_SAVESTATE_REVERTING_FAILED];
			return;
		}
		
		[window setDocumentEdited:YES];
		
		[self restoreCoreState];
		[self setStatus:NSSTRING_STATUS_SAVESTATE_REVERTED];
	}
}

- (IBAction) loadEmuSaveStateSlot:(id)sender
{
	BOOL result = NO;
	NSInteger i = [CocoaDSUtil getIBActionSenderTag:sender];
	
	NSString *saveStatePath = [[CocoaDSFile saveStateURL] path];
	if (saveStatePath == nil)
	{
		// Should throw an error message here...
		return;
	}
	
	if (i < 0 || i > MAX_SAVESTATE_SLOTS)
	{
		return;
	}
	
	NSURL *currentRomURL = [[self currentRom] fileURL];
	NSString *fileName = [CocoaDSFile saveSlotFileName:currentRomURL slotNumber:(NSUInteger)(i + 1)];
	if (fileName == nil)
	{
		return;
	}
	
	[self pauseCore];
	
	result = [CocoaDSFile loadState:[NSURL fileURLWithPath:[saveStatePath stringByAppendingPathComponent:fileName]]];
	if (result == NO)
	{
		[self setStatus:NSSTRING_STATUS_SAVESTATE_LOADING_FAILED];
	}
	
	[self restoreCoreState];
	[self setStatus:NSSTRING_STATUS_SAVESTATE_LOADED];
}

- (IBAction) saveEmuSaveStateSlot:(id)sender
{
	BOOL result = NO;
	NSInteger i = [CocoaDSUtil getIBActionSenderTag:sender];
	
	NSString *saveStatePath = [[CocoaDSFile saveStateURL] path];
	if (saveStatePath == nil)
	{
		[self setStatus:NSSTRING_STATUS_CANNOT_GENERATE_SAVE_PATH];
		return;
	}
	
	result = [CocoaDSFile createUserAppSupportDirectory:@"States"];
	if (result == NO)
	{
		[self setStatus:NSSTRING_STATUS_CANNOT_CREATE_SAVE_DIRECTORY];
		return;
	}
	
	if (i < 0 || i > MAX_SAVESTATE_SLOTS)
	{
		return;
	}
	
	NSURL *currentRomURL = [[self currentRom] fileURL];
	NSString *fileName = [CocoaDSFile saveSlotFileName:currentRomURL slotNumber:(NSUInteger)(i + 1)];
	if (fileName == nil)
	{
		return;
	}
	
	[self pauseCore];
	
	result = [CocoaDSFile saveState:[NSURL fileURLWithPath:[saveStatePath stringByAppendingPathComponent:fileName]]];
	if (result == NO)
	{
		[self setStatus:NSSTRING_STATUS_SAVESTATE_SAVING_FAILED];
		return;
	}
	
	[self restoreCoreState];
	[self setStatus:NSSTRING_STATUS_SAVESTATE_SAVED];
}

- (IBAction) importRomSave:(id)sender
{
	NSURL *selectedFile = nil;
	NSInteger buttonClicked = NSFileHandlingPanelCancelButton;
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_IMPORT_ROM_SAVE_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ROM_SAVE_RAW, @FILE_EXT_ACTION_REPLAY_SAVE, nil];
	
	// The NSOpenPanel method -(NSInt)runModalForDirectory:file:types:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	[panel setAllowedFileTypes:fileTypes];
	buttonClicked = [panel runModal];
#else
	buttonClicked = [panel runModalForDirectory:nil file:nil types:fileTypes];
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
	
	BOOL result = [CocoaDSFile importRomSave:selectedFile];
	if (!result)
	{
		[self setStatus:NSSTRING_STATUS_ROM_SAVE_IMPORT_FAILED];
		return;
	}
	
	[self setStatus:NSSTRING_STATUS_ROM_SAVE_IMPORTED];
}

- (IBAction) exportRomSave:(id)sender
{
	[self pauseCore];
	
	BOOL result = NO;
	NSInteger buttonClicked;
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setTitle:NSSTRING_TITLE_EXPORT_ROM_SAVE_PANEL];
	[panel setCanCreateDirectories:YES];
	[panel setAccessoryView:exportRomSavePanelAccessoryView];
	
	//save it
	buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		NSURL *romSaveURL = [CocoaDSFile fileURLFromRomURL:[[self currentRom] fileURL] toKind:@"ROM Save"];
		if (romSaveURL != nil)
		{
			result = [CocoaDSFile exportRomSaveToURL:[panel URL] romSaveURL:romSaveURL fileType:selectedExportRomSaveID];
			if (result == NO)
			{
				[self setStatus:NSSTRING_STATUS_ROM_SAVE_EXPORT_FAILED];
				return;
			}
			
			[self setStatus:NSSTRING_STATUS_ROM_SAVE_EXPORTED];
		}
	}
	
	[self restoreCoreState];
}

- (IBAction) selectExportRomSaveFormat:(id)sender
{
	selectedExportRomSaveID = [CocoaDSUtil getIBActionSenderTag:sender];
}

- (IBAction)copy:(id)sender
{
	[dispViewDelegate copyToPasteboard];
}

// Emulation Menu
- (IBAction) executeCoreToggle:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore coreState] == CORESTATE_PAUSE)
	{
		if ([self isRomLoaded])
		{
			[self executeCore];
		}
	}
	else
	{
		[self pauseCore];
	}
}

- (IBAction) resetCore:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([self isRomLoaded])
	{
		[self setStatus:NSSTRING_STATUS_EMULATOR_RESETTING];
		[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isWorking"];
		[window displayIfNeeded];
		
		[cdsCore reset];
		if ([cdsCore coreState] == CORESTATE_PAUSE)
		{
			[[self dispViewDelegate] setViewToWhite];
		}
		
		[self setStatus:NSSTRING_STATUS_EMULATOR_RESET];
		[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
		[window displayIfNeeded];
	}
}

- (IBAction) speedLimitDisable:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore isSpeedLimitEnabled])
	{
		[cdsCore setIsSpeedLimitEnabled:NO];
		[self setStatus:NSSTRING_STATUS_SPEED_LIMIT_DISABLED];
	}
	else
	{
		[cdsCore setIsSpeedLimitEnabled:YES];
		[self setStatus:NSSTRING_STATUS_SPEED_LIMIT_ENABLED];
	}
}

- (IBAction) toggleAutoFrameSkip:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore isFrameSkipEnabled])
	{
		[cdsCore setIsFrameSkipEnabled:NO];
		[self setStatus:NSSTRING_STATUS_AUTO_FRAME_SKIP_DISABLED];
	}
	else
	{
		[cdsCore setIsFrameSkipEnabled:YES];
		[self setStatus:NSSTRING_STATUS_AUTO_FRAME_SKIP_ENABLED];
	}
}

- (IBAction) changeRomSaveType:(id)sender
{
	NSInteger saveTypeID = [CocoaDSUtil getIBActionSenderTag:sender];
	[self setSelectedRomSaveTypeID:saveTypeID];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore changeRomSaveType:saveTypeID];
}

- (IBAction) cheatsDisable:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore isCheatingEnabled])
	{
		[cdsCore setIsCheatingEnabled:NO];
		[self setStatus:NSSTRING_STATUS_CHEATS_DISABLED];
	}
	else
	{
		[cdsCore setIsCheatingEnabled:YES];
		[self setStatus:NSSTRING_STATUS_CHEATS_ENABLED];
	}
}

- (IBAction) ejectCard:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[cdsCore toggleEjectCard];
}

// View Menu

- (IBAction) changeCoreSpeed:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[cdsCore setSpeedScalar:(CGFloat)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0f];
}

- (IBAction) changeCoreEmuFlags:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	NSUInteger flags = [cdsCore emulationFlags];
	
	NSInteger flagBit = [CocoaDSUtil getIBActionSenderTag:sender];
	if (flagBit < 0)
	{
		return;
	}
	
	BOOL flagState = [CocoaDSUtil getIBActionSenderButtonStateBool:sender];
	if (flagState)
	{
		flags |= (1 << flagBit);
	}
	else
	{
		flags &= ~(1 << flagBit);
	}
	
	[cdsCore setEmulationFlags:flags];
}

- (IBAction) changeFirmwareSettings:(id)sender
{
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[[cdsCore cdsFirmware] update];
}

- (IBAction) changeVolume:(id)sender
{
	float vol = [self volume];
	[self setVolume:vol];
	[self setStatus:[NSString stringWithFormat:NSSTRING_STATUS_VOLUME, vol]];
	[CocoaDSUtil messageSendOneWayWithFloat:[cdsSpeaker receivePort] msgID:MESSAGE_SET_VOLUME floatValue:vol];
}

- (IBAction) changeAudioEngine:(id)sender
{
	[CocoaDSUtil messageSendOneWayWithInteger:[cdsSpeaker receivePort] msgID:MESSAGE_SET_AUDIO_PROCESS_METHOD integerValue:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeSpuAdvancedLogic:(id)sender
{
	[CocoaDSUtil messageSendOneWayWithBool:[cdsSpeaker receivePort] msgID:MESSAGE_SET_SPU_ADVANCED_LOGIC boolValue:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) changeSpuInterpolationMode:(id)sender
{
	[CocoaDSUtil messageSendOneWayWithInteger:[cdsSpeaker receivePort] msgID:MESSAGE_SET_SPU_INTERPOLATION_MODE integerValue:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeSpuSyncMode:(id)sender
{
	[CocoaDSUtil messageSendOneWayWithInteger:[cdsSpeaker receivePort] msgID:MESSAGE_SET_SPU_SYNC_MODE integerValue:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeSpuSyncMethod:(id)sender
{
	[CocoaDSUtil messageSendOneWayWithInteger:[cdsSpeaker receivePort] msgID:MESSAGE_SET_SPU_SYNC_METHOD integerValue:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeScale:(id)sender
{
	[self setContentScalar:(double)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0];
}

- (IBAction) changeRotation:(id)sender
{
	// Get the rotation value from the sender.
	if ([sender isMemberOfClass:[NSSlider class]])
	{
		[self setContentRotation:[(NSSlider *)sender doubleValue]];
	}
	else
	{
		[self setContentRotation:(double)[CocoaDSUtil getIBActionSenderTag:sender]];
	}
}

- (IBAction) changeRotationRelative:(id)sender
{
	double angleDegrees = [dispViewDelegate rotation] + (double)[CocoaDSUtil getIBActionSenderTag:sender];
	[self setContentRotation:angleDegrees];
}

- (IBAction) changeBilinearOutput:(id)sender
{
	[dispViewDelegate setUseBilinearOutput:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) changeVerticalSync:(id)sender
{
	[dispViewDelegate setUseVerticalSync:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) changeDisplayMode:(id)sender
{
	[dispViewDelegate setDisplayType:[CocoaDSUtil getIBActionSenderTag:sender]];
	[self resizeWithTransform:[dispViewDelegate normalSize] scalar:[dispViewDelegate scale] rotation:[dispViewDelegate rotation]];
}

- (IBAction) changeVideoFilter:(id)sender
{
	[dispViewDelegate setVideoFilterType:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) change3DRenderMethod:(id)sender
{
	[dispViewDelegate setRender3DRenderingEngine:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) change3DRenderHighPrecisionColorInterpolation:(id)sender
{
	[dispViewDelegate setRender3DHighPrecisionColorInterpolation:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) change3DRenderEdgeMarking:(id)sender
{
	[dispViewDelegate setRender3DEdgeMarking:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) change3DRenderFog:(id)sender
{
	[dispViewDelegate setRender3DFog:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) change3DRenderTextures:(id)sender
{
	[dispViewDelegate setRender3DTextures:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) change3DRenderDepthComparisonThreshold:(id)sender
{
	NSInteger threshold = 0;
	
	if ([sender respondsToSelector:@selector(integerValue)])
	{
		threshold = [sender integerValue];
		if (threshold < 0)
		{
			return;
		}
	}
	
	[dispViewDelegate setRender3DDepthComparisonThreshold:(NSUInteger)threshold];
}

- (IBAction) change3DRenderThreads:(id)sender
{
	NSInteger numberThreads = [CocoaDSUtil getIBActionSenderTag:sender];
	if (numberThreads < 0)
	{
		return;
	}
	
	[dispViewDelegate setRender3DThreads:(NSUInteger)numberThreads];
}

- (IBAction) change3DRenderLineHack:(id)sender
{
	[dispViewDelegate setRender3DLineHack:[CocoaDSUtil getIBActionSenderButtonStateBool:sender]];
}

- (IBAction) hudDisable:(id)sender
{
	if ([dispViewDelegate isHudEnabled])
	{
		[dispViewDelegate setIsHudEnabled:NO];
		[self setStatus:NSSTRING_STATUS_HUD_DISABLED];
	}
	else
	{
		[dispViewDelegate setIsHudEnabled:YES];
		[self setStatus:NSSTRING_STATUS_HUD_ENABLED];
	}
}

- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender
{
	if (isMinSizeNormal)
	{
		minDisplayViewSize.width /= 4;
		minDisplayViewSize.height /= 4;
		isMinSizeNormal = NO;
	}
	else
	{
		minDisplayViewSize.width = GPU_DISPLAY_WIDTH;
		minDisplayViewSize.height = GPU_DISPLAY_HEIGHT * 2;
		isMinSizeNormal = YES;
	}
	
	// Set the minimum content size, keeping the display rotation in mind.
	NSSize transformedMinSize = GetTransformedBounds(minDisplayViewSize, 1.0, CLOCKWISE_DEGREES([dispViewDelegate rotation]));
	transformedMinSize.height += statusBarHeight;
	[window setContentMinSize:transformedMinSize];
	
	// Resize the window if it's smaller than the minimum content size.
	NSRect windowContentRect = [window contentRectForFrameRect:[window frame]];
	if (windowContentRect.size.width < transformedMinSize.width || windowContentRect.size.height < transformedMinSize.height)
	{
		// Prepare to resize.
		NSRect oldFrameRect = [window frame];
		windowContentRect.size = transformedMinSize;
		NSRect newFrameRect = [window frameRectForContentRect:windowContentRect];
		
		// Keep the window centered when expanding the size.
		newFrameRect.origin.x = oldFrameRect.origin.x - ((newFrameRect.size.width - oldFrameRect.size.width) / 2);
		newFrameRect.origin.y = oldFrameRect.origin.y - ((newFrameRect.size.height - oldFrameRect.size.height) / 2);
		
		// Set the window size.
		[window setFrame:newFrameRect display:YES animate:NO];
	}
}

- (IBAction) toggleStatusBar:(id)sender
{
	NSRect frameRect = [window frame];
	
	if (isShowingStatusBar)
	{
		isShowingStatusBar = NO;
		statusBarHeight = 0;
		frameRect.origin.y += WINDOW_STATUS_BAR_HEIGHT;
		frameRect.size.height -= WINDOW_STATUS_BAR_HEIGHT;
	}
	else
	{
		isShowingStatusBar = YES;
		statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
		frameRect.origin.y -= WINDOW_STATUS_BAR_HEIGHT;
		frameRect.size.height += WINDOW_STATUS_BAR_HEIGHT;
	}
	
	[[self bindings] setValue:[NSNumber numberWithBool:isShowingStatusBar] forKey:@"isShowingStatusBar"];
	[window setFrame:frameRect display:YES animate:NO];
}

- (IBAction) selectScreenshotFileFormat:(id)sender
{
	screenshotFileFormat = (NSBitmapImageFileType)[CocoaDSUtil getIBActionSenderTag:sender];
}

- (IBAction) saveScreenshotAs:(id)sender
{
	[self pauseCore];
	
	NSInteger buttonClicked = NSFileHandlingPanelCancelButton;
	NSSavePanel *panel = [NSSavePanel savePanel];
	
	[panel setCanCreateDirectories:YES];
	[panel setTitle:NSSTRING_TITLE_SAVE_SCREENSHOT_PANEL];
	[panel setAccessoryView:saveScreenshotPanelAccessoryView];
	
	buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		[dispViewDelegate requestScreenshot:[panel URL] fileType:screenshotFileFormat];
	}
	else
	{
		[self restoreCoreState];
	}
}

- (void) saveScreenshotAsFinish:(NSNotification *)aNotification
{
	NSURL *fileURL = (NSURL *)[[aNotification userInfo] valueForKey:@"fileURL"];
	NSBitmapImageFileType fileType = (NSBitmapImageFileType)[(NSNumber *)[[aNotification userInfo] valueForKey:@"fileType"] integerValue];
	NSImage *screenshotImage = (NSImage *)[[aNotification userInfo] valueForKey:@"screenshotImage"];
	
	BOOL fileSaved = [CocoaDSFile saveScreenshot:fileURL bitmapData:[[screenshotImage representations] objectAtIndex:0] fileType:fileType];
	if (!fileSaved)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSSTRING_ERROR_TITLE_LEGACY message:NSSTRING_ERROR_SCREENSHOT_FAILED_LEGACY];
	}
	
	[self restoreCoreState];
}

- (IBAction) toggleGPUState:(id)sender
{
	NSInteger i = [CocoaDSUtil getIBActionSenderTag:sender];
	UInt32 flags = [dispViewDelegate gpuStateFlags];
	
	flags ^= (1 << i);
	
	[dispViewDelegate setGpuStateFlags:flags];
}

- (BOOL) handleLoadRom:(NSURL *)fileURL
{
	BOOL result = NO;
	
	if (isRomLoading)
	{
		return result;
	}
	
	if ([self isRomLoaded])
	{
		BOOL closeResult = [self handleUnloadRom:REASONFORCLOSE_OPEN romToLoad:fileURL];
		if ([self isShowingSaveStateSheet])
		{
			return result;
		}
		
		if (![self isShowingSaveStateSheet] && closeResult == NO)
		{
			return result;
		}
	}
	
	// Check for the v0.9.7 ROM Save File
	if ([CocoaDSFile romSaveExistsWithRom:fileURL] && ![CocoaDSFile romSaveExists:fileURL])
	{
		SEL endSheetSelector = @selector(didEndFileMigrationSheet:returnCode:contextInfo:);
		
		[fileURL retain];
		[self setIsSheetControllingExecution:YES];
		[self setIsShowingSaveStateSheet:YES];
		
		[NSApp beginSheet:saveFileMigrationSheet
		   modalForWindow:window
            modalDelegate:self
		   didEndSelector:endSheetSelector
			  contextInfo:fileURL];
	}
	else
	{
		result = [self loadRom:fileURL];
	}
	
	return result;
}

- (BOOL) handleUnloadRom:(NSInteger)reasonID romToLoad:(NSURL *)romURL
{
	BOOL result = NO;
	
	if (isRomLoading || ![self isRomLoaded])
	{
		return result;
	}
	
	[self pauseCore];
	
	if ([window isDocumentEdited] && currentEmuSaveStateURL != nil)
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
		
		[currentEmuSaveStateURL retain];
		[self setIsSheetControllingExecution:YES];
		[self setIsShowingSaveStateSheet:YES];
		
		[NSApp beginSheet:saveStatePrecloseSheet
		   modalForWindow:window
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

- (BOOL) loadRom:(NSURL *)romURL
{
	BOOL result = NO;
	
	if (romURL == nil)
	{
		return result;
	}
	
	[self setStatus:NSSTRING_STATUS_ROM_LOADING];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isWorking"];
	[window displayIfNeeded];
	
	// Need to pause the core before loading the ROM.
	[self pauseCore];
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setDynaRec];
	
	CocoaDSRom *newRom = [[[CocoaDSRom alloc] init] autorelease];
	if (newRom != nil)
	{
		isRomLoading = YES;
		[romURL retain];
		[newRom setSaveType:selectedRomSaveTypeID];
		[NSThread detachNewThreadSelector:@selector(loadDataOnThread:) toTarget:newRom withObject:romURL];
		[romURL release];
	}
	
	result = YES;
	
	return result;
}

- (void) loadRomDidFinish:(NSNotification *)aNotification
{
	CocoaDSRom *theRom = [aNotification object];
	NSDictionary *userInfo = [aNotification userInfo];
	BOOL didLoad = [(NSNumber *)[userInfo valueForKey:@"DidLoad"] boolValue];
	
	if (theRom == nil || ![theRom isDataLoaded] || !didLoad)
	{
		// If ROM loading fails, restore the core state, but only if a ROM is already loaded.
		if([self isRomLoaded])
		{
			[self restoreCoreState];
		}
		
		[self setStatus:NSSTRING_STATUS_ROM_LOADING_FAILED];
		[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
		
		isRomLoading = NO;
		return;
	}
	
	// Set the core's ROM to our newly allocated ROM object.
	[self setCurrentRom:theRom];
	[romInfoPanelController setContent:[theRom bindings]];
	
	// If the ROM has an associated cheat file, load it now.
	NSString *cheatsPath = [[CocoaDSFile fileURLFromRomURL:[theRom fileURL] toKind:@"Cheat"] path];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	CocoaDSCheatManager *newCheatList = [[[CocoaDSCheatManager alloc] initWithCore:cdsCore fileURL:[NSURL fileURLWithPath:cheatsPath]] autorelease];
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
				[cheatWindowBindings setValue:[NSString stringWithFormat:@"%d", [dbList count]] forKey:@"cheatDBItemCount"];
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
		[[cheatWindowDelegate cdsCheatSearch] setCdsCore:cdsCore];
		[cheatWindowDelegate setCheatSearchViewByStyle:CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE];
	}
	
	// Add the last loaded ROM to the Recent ROMs list.
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[theRom fileURL]];
	
	// Update the UI to indicate that a ROM has indeed been loaded.
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
	[window setRepresentedFilename:[[theRom fileURL] path]];
#else
	[window setRepresentedURL:[theRom fileURL]];
#endif
	[[window standardWindowButton:NSWindowDocumentIconButton] setImage:[theRom icon]];
	
	NSString *newWindowTitle = [theRom internalName];
	[window setTitle:newWindowTitle];
	
	[dispViewDelegate setViewToWhite];
	[self setStatus:NSSTRING_STATUS_ROM_LOADED];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
	[self setIsRomLoaded:YES];
	[window displayIfNeeded];
	isRomLoading = NO;
	
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
	
	[currentEmuSaveStateURL release];
	currentEmuSaveStateURL = nil;
	[window setDocumentEdited:NO];
	
	// Save the ROM's cheat list before unloading.
	[[self cdsCheats] save];
	
	// Update the UI to indicate that the ROM has started the process of unloading.
	[self setStatus:NSSTRING_STATUS_ROM_UNLOADING];
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
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isWorking"];
	[window displayIfNeeded];
	
	// Unload the ROM.
	[self setCurrentRom:nil];
	
	// Release the current cheat list and assign the empty list.
	[self setCdsCheats:nil];
	if (dummyCheatList == nil)
	{
		dummyCheatList = [[CocoaDSCheatManager alloc] init];
	}
	[CocoaDSCheatManager setMasterCheatList:dummyCheatList];
	
	// Update the UI to indicate that the ROM has finished unloading.
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
	[window setRepresentedFilename:nil];
#else
	[window setRepresentedURL:nil];
#endif
	
	NSString *newWindowTitle = (NSString *)[[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
	[window setTitle:newWindowTitle];
	
	[dispViewDelegate setViewToBlack];
	[self setStatus:NSSTRING_STATUS_ROM_UNLOADED];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
	[self setIsRomLoaded:NO];
	[window displayIfNeeded];
	
	result = YES;
	
	return result;
}

- (void) executeCore
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCoreState:CORESTATE_EXECUTE];
}

- (void) pauseCore
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCoreState:CORESTATE_PAUSE];
}

- (void) restoreCoreState
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore restoreCoreState];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)theItem
{
	BOOL enable = YES;
    SEL theAction = [theItem action];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if (theAction == @selector(importRomSave:) ||
		theAction == @selector(exportRomSave:))
    {
		if (![self isRomLoaded])
		{
			enable = NO;
		}
    }
    else if (theAction == @selector(executeCoreToggle:))
    {
		if (![self isRomLoaded] ||
			![cdsCore masterExecute] ||
			[self isSheetControllingExecution])
		{
			enable = NO;
		}
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore coreState] == CORESTATE_PAUSE)
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_EXECUTE_CONTROL];
			}
			else
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_PAUSE_CONTROL];
			}
		}
		else if ([(id)theItem isMemberOfClass:[NSToolbarItem class]])
		{
			if ([cdsCore coreState] == CORESTATE_PAUSE)
			{
				[(NSToolbarItem*)theItem setLabel:NSSTRING_TITLE_EXECUTE_CONTROL];
				[(NSToolbarItem*)theItem setImage:iconExecute];
			}
			else
			{
				[(NSToolbarItem*)theItem setLabel:NSSTRING_TITLE_PAUSE_CONTROL];
				[(NSToolbarItem*)theItem setImage:iconPause];
			}
		}
    }
	else if (theAction == @selector(executeCore) ||
			 theAction == @selector(pauseCore))
    {
		if (![self isRomLoaded] ||
			![cdsCore masterExecute] ||
			[self isSheetControllingExecution])
		{
			enable = NO;
		}
    }
	else if (theAction == @selector(resetCore:))
	{
		if (![self isRomLoaded] || [self isSheetControllingExecution])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(ejectCard:))
	{
		if (![self isRomLoaded])
		{
			enable = NO;
		}
		
		if ([cdsCore ejectCardFlag])
		{
			[(NSMenuItem*)theItem setState:NSOnState];
		}
		else
		{
			[(NSMenuItem*)theItem setState:NSOffState];
		}
		
	}
	else if (theAction == @selector(_openRecentDocument:))
	{
		if ([self isShowingSaveStateSheet])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(openRom:))
	{
		if (isRomLoading || [self isShowingSaveStateSheet])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(closeRom:))
	{
		if (![self isRomLoaded] || isRomLoading || [self isShowingSaveStateSheet])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(loadEmuSaveStateSlot:))
	{
		if (![self isRomLoaded] || [self isShowingSaveStateSheet])
		{
			enable = NO;
		}
		else if (![CocoaDSFile saveStateExistsForSlot:[[self currentRom] fileURL] slotNumber:[theItem tag] + 1])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(saveEmuSaveStateSlot:))
	{
		if (![self isRomLoaded] || [self isShowingSaveStateSheet])
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
			else if (speedScalar == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
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
	else if (theAction == @selector(speedLimitDisable:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore isSpeedLimitEnabled])
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_DISABLE_SPEED_LIMIT];
			}
			else
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_ENABLE_SPEED_LIMIT];
			}
		}
	}
	else if (theAction == @selector(toggleAutoFrameSkip:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore isFrameSkipEnabled])
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_DISABLE_AUTO_FRAME_SKIP];
			}
			else
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_ENABLE_AUTO_FRAME_SKIP];
			}
		}
	}
	else if (theAction == @selector(cheatsDisable:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore isCheatingEnabled])
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_DISABLE_CHEATS];
			}
			else
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_ENABLE_CHEATS];
			}
		}
	}
	else if (theAction == @selector(changeRomSaveType:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([self selectedRomSaveTypeID] == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeScale:))
	{
		NSInteger viewScale = (NSInteger)([dispViewDelegate scale] * 100.0);
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if (viewScale == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeRotation:))
	{
		NSInteger viewRotation = (NSInteger)([dispViewDelegate rotation]);
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([theItem tag] == -1)
			{
				if (viewRotation == 0 ||
					viewRotation == 90 ||
					viewRotation == 180 ||
					viewRotation == 270)
				{
					[(NSMenuItem*)theItem setState:NSOffState];
				}
				else
				{
					[(NSMenuItem*)theItem setState:NSOnState];
				}
			}
			else if (viewRotation == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeDisplayMode:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([dispViewDelegate displayType] == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(openEmuSaveState:) ||
			 theAction == @selector(saveEmuSaveState:) ||
			 theAction == @selector(saveEmuSaveStateAs:))
	{
		if (![self isRomLoaded] || [self isShowingSaveStateSheet])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(revertEmuSaveState:))
	{
		if (![self isRomLoaded] ||
			[self isShowingSaveStateSheet] ||
			currentEmuSaveStateURL == nil)
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleGPUState:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([dispViewDelegate gpuStateByBit:[theItem tag]])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(hudDisable:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([dispViewDelegate isHudEnabled])
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_DISABLE_HUD];
			}
			else
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_ENABLE_HUD];
			}
		}
	}
	else if (theAction == @selector(toggleStatusBar:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if (isShowingStatusBar)
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_HIDE_STATUS_BAR];
			}
			else
			{
				[(NSMenuItem*)theItem setTitle:NSSTRING_TITLE_SHOW_STATUS_BAR];
			}
		}
	}
	else if (theAction == @selector(toggleKeepMinDisplaySizeAtNormal:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if (isMinSizeNormal)
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	
	return enable;
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([self isRomLoaded])
	{
		[romInfoPanelController setContent:[[self currentRom] bindings]];
	}
	else
	{
		[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	}

	[firmwarePanelController setContent:[cdsCore cdsFirmware]];
	[emuWindowController setContent:[self bindings]];
	[cdsDisplayController setContent:[dispViewDelegate bindings]];
	[cdsSoundController setContent:[cdsSpeaker property]];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
	NSSize finalSize = frameSize;	
	const NSSize normalBounds = [dispViewDelegate normalSize];
	
	// Get a content Rect so that we can make our comparison.
	// This will be based on the proposed frameSize.
	const NSRect frameRect = NSMakeRect(0.0f, 0.0f, frameSize.width, frameSize.height);
	const NSRect contentRect = [sender contentRectForFrameRect:frameRect];
	
	// Find the maximum scalar we can use for the display view, bounded by the
	// content Rect.
	const NSSize checkSize = GetTransformedBounds(normalBounds, 1.0, [dispViewDelegate rotation]);
	const NSSize contentBounds = NSMakeSize(contentRect.size.width, contentRect.size.height - statusBarHeight);
	const double maxS = GetMaxScalarInBounds(checkSize.width, checkSize.height, contentBounds.width, contentBounds.height);
	[dispViewDelegate setScale:maxS];
	
	// Make a new content Rect with our max scalar, and convert it back to a frame Rect.
	const NSRect finalContentRect = NSMakeRect(0.0f, 0.0f, checkSize.width * maxS, (checkSize.height * maxS) + statusBarHeight);
	NSRect finalFrameRect = [sender frameRectForContentRect:finalContentRect];
	
	// Set the final size based on our new frame Rect.
	finalSize.width = finalFrameRect.size.width;
	finalSize.height = finalFrameRect.size.height;
	
	return finalSize;
}

- (void)windowDidResize:(NSNotification *)notification
{
	if (dispViewDelegate == nil)
	{
		return;
	}
		
	const NSSize normalBounds = [dispViewDelegate normalSize];
	const double r = [dispViewDelegate rotation];
	
	// Get the max scalar within the window's current content bounds.
	const NSSize checkSize = GetTransformedBounds(normalBounds, 1.0, r);
	NSSize contentBounds = [[window contentView] bounds].size;
	contentBounds.height -= statusBarHeight;
	const double maxS = GetMaxScalarInBounds(checkSize.width, checkSize.height, contentBounds.width, contentBounds.height);
	
	// Set the display view's properties.
	[dispViewDelegate setScale:maxS];
	
	// Resize the view.
	NSRect newContentFrame = [[window contentView] bounds];
	newContentFrame.origin.y = statusBarHeight;
	newContentFrame.size.height -= statusBarHeight;
	[displayView setFrame:newContentFrame];
}

- (BOOL)windowShouldClose:(id)sender
{
	BOOL shouldClose = YES;
	
	// If no ROM is loaded, terminate the application.
	if (![self isRomLoaded])
	{
		[NSApp terminate:sender];
	}
	// If a ROM is loaded, just close the ROM, but don't terminate.
	else
	{
		shouldClose = NO;
		[self closeRom:nil];
	}
	
	return shouldClose;
}

- (IBAction) closeMigrationSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	NSInteger code = [(NSControl *)sender tag];
	
    [NSApp endSheet:sheet returnCode:code];
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
	
	[self setIsSheetControllingExecution:NO];
	[self setIsShowingFileMigrationSheet:NO];
	
	[self loadRom:romURL];
	
	// We retained this when we initially put up the sheet, so we need to release it now.
	[romURL release];
}

- (void) didEndSaveStateSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	BOOL result = NO;
	
	[sheet orderOut:self];
	
	switch (returnCode)
	{
		case NSCancelButton: // Cancel
			[self restoreCoreState];
			[self setIsSheetControllingExecution:NO];
			[self setIsShowingSaveStateSheet:NO];
			return;
			break;
		
		case COCOA_DIALOG_DEFAULT: // Save
			result = [CocoaDSFile saveState:currentEmuSaveStateURL];
			if (result == NO)
			{
				// Throw an error here...
				return;
			}
			break;
		
		case COCOA_DIALOG_OPTION: // Don't Save
			break;
		
		default:
			break;
	}
	
	[self unloadRom];
	
	[self setIsSheetControllingExecution:NO];
	[self setIsShowingSaveStateSheet:NO];
	
	// We retained this when we initially put up the sheet, so we need to release it now.
	[currentEmuSaveStateURL release];
	currentEmuSaveStateURL = nil;
}

- (void) didEndSaveStateSheetOpen:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[self didEndSaveStateSheet:sheet returnCode:returnCode contextInfo:contextInfo];
	
	NSURL *romURL = (NSURL *)contextInfo;
	[self handleLoadRom:romURL];
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
		if (currentEmuSaveStateURL == nil)
		{
			[NSApp replyToApplicationShouldTerminate:YES];
		}
	}
}

- (void) setupUserDefaults
{
	// Set the display mode, sizing, and rotation.
	double displayScalar = (double)([[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Size"] / 100.0);
	double displayRotation = (double)[[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Rotation"];
	[dispViewDelegate setDisplayType:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_Mode"]];
	[self setContentScalar:displayScalar];
	[self setContentRotation:displayRotation];
	
	// Set the SPU settings per user preferences.
	[self setVolume:[[NSUserDefaults standardUserDefaults] floatForKey:@"Sound_Volume"]];
	[[self cdsSpeaker] setVolume:[[NSUserDefaults standardUserDefaults] floatForKey:@"Sound_Volume"]];
	[[self cdsSpeaker] setAudioOutputEngine:[[NSUserDefaults standardUserDefaults] integerForKey:@"Sound_AudioOutputEngine"]];
	[[self cdsSpeaker] setSpuAdvancedLogic:[[NSUserDefaults standardUserDefaults] boolForKey:@"SPU_AdvancedLogic"]];
	[[self cdsSpeaker] setSpuInterpolationMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_InterpolationMode"]];
	[[self cdsSpeaker] setSpuSyncMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMode"]];
	[[self cdsSpeaker] setSpuSyncMethod:[[NSUserDefaults standardUserDefaults] integerForKey:@"SPU_SyncMethod"]];
	
	// Setup the window display view per user preferences.
	[[self dispViewDelegate] setVideoFilterType:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"]];
	[[self dispViewDelegate] setUseBilinearOutput:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseBilinearOutput"]];
	[[self dispViewDelegate] setUseVerticalSync:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseVerticalSync"]];
	
	// Set the 3D rendering options per user preferences.
	[[self dispViewDelegate] setRender3DThreads:(NSUInteger)[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_Threads"]];
	[[self dispViewDelegate] setRender3DRenderingEngine:[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_RenderingEngine"]];
	[[self dispViewDelegate] setRender3DHighPrecisionColorInterpolation:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_HighPrecisionColorInterpolation"]];
	[[self dispViewDelegate] setRender3DEdgeMarking:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_EdgeMarking"]];
	[[self dispViewDelegate] setRender3DFog:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_Fog"]];
	[[self dispViewDelegate] setRender3DTextures:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_Textures"]];
	[[self dispViewDelegate] setRender3DDepthComparisonThreshold:(NSUInteger)[[NSUserDefaults standardUserDefaults] integerForKey:@"Render3D_DepthComparisonThreshold"]];
	[[self dispViewDelegate] setRender3DLineHack:[[NSUserDefaults standardUserDefaults] boolForKey:@"Render3D_LineHack"]];
}

- (IBAction) writeDefaultsDisplayRotation:(id)sender
{
	NSMutableDictionary *dispViewBindings = (NSMutableDictionary *)[cdsDisplayController content];
	
	[[NSUserDefaults standardUserDefaults] setDouble:[[dispViewBindings valueForKey:@"rotation"] doubleValue] forKey:@"DisplayView_Rotation"];
}

- (IBAction) writeDefaultsHUDSettings:(id)sender
{
	// TODO: Not implemented.
}

- (IBAction) writeDefaultsVideoOutputSettings:(id)sender
{
	NSMutableDictionary *dispViewBindings = (NSMutableDictionary *)[cdsDisplayController content];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[[dispViewBindings valueForKey:@"videoFilterType"] integerValue] forKey:@"DisplayView_VideoFilter"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"useBilinearOutput"] boolValue] forKey:@"DisplayView_UseBilinearOutput"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"useVerticalSync"] boolValue] forKey:@"DisplayView_UseVerticalSync"];
}

- (IBAction) writeDefaults3DRenderingSettings:(id)sender
{
	NSMutableDictionary *dispViewBindings = (NSMutableDictionary *)[cdsDisplayController content];
	
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[[dispViewBindings valueForKey:@"render3DRenderingEngine"] integerValue] forKey:@"Render3D_RenderingEngine"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"render3DHighPrecisionColorInterpolation"] boolValue] forKey:@"Render3D_HighPrecisionColorInterpolation"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"render3DEdgeMarking"] boolValue] forKey:@"Render3D_EdgeMarking"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"render3DFog"] boolValue] forKey:@"Render3D_Fog"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"render3DTextures"] boolValue] forKey:@"Render3D_Textures"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[dispViewBindings valueForKey:@"render3DDepthComparisonThreshold"] integerValue] forKey:@"Render3D_DepthComparisonThreshold"];
	[[NSUserDefaults standardUserDefaults] setInteger:[[dispViewBindings valueForKey:@"render3DThreads"] integerValue] forKey:@"Render3D_Threads"];
	[[NSUserDefaults standardUserDefaults] setBool:[[dispViewBindings valueForKey:@"render3DLineHack"] boolValue] forKey:@"Render3D_LineHack"];
}

- (IBAction) writeDefaultsEmulationSettings:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	NSDictionary *firmwareDict = [(CocoaDSFirmware *)[firmwarePanelController content] dataDictionary];
	
	// Force end of editing of any text fields.
	[[(NSControl *)sender window] makeFirstResponder:nil];
	
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagAdvancedBusLevelTiming] forKey:@"Emulation_AdvancedBusLevelTiming"];
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsCore cpuEmulationEngine] forKey:@"Emulation_CPUEmulationEngine"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagUseExternalBios] forKey:@"Emulation_UseExternalBIOSImages"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagEmulateBiosInterrupts] forKey:@"Emulation_BIOSEmulateSWI"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagPatchDelayLoop] forKey:@"Emulation_BIOSPatchDelayLoopSWI"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagUseExternalFirmware] forKey:@"Emulation_UseExternalFirmwareImage"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagFirmwareBoot] forKey:@"Emulation_FirmwareBoot"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagEmulateEnsata] forKey:@"Emulation_EmulateEnsata"];
	[[NSUserDefaults standardUserDefaults] setBool:[cdsCore emuFlagDebugConsole] forKey:@"Emulation_UseDebugConsole"];
	
	[[NSUserDefaults standardUserDefaults] setObject:[firmwareDict valueForKey:@"Nickname"] forKey:@"FirmwareConfig_Nickname"];
	[[NSUserDefaults standardUserDefaults] setObject:[firmwareDict valueForKey:@"Message"] forKey:@"FirmwareConfig_Message"];
	[[NSUserDefaults standardUserDefaults] setObject:[firmwareDict valueForKey:@"FavoriteColor"] forKey:@"FirmwareConfig_FavoriteColor"];
	[[NSUserDefaults standardUserDefaults] setObject:[firmwareDict valueForKey:@"Birthday"] forKey:@"FirmwareConfig_Birthday"];
	[[NSUserDefaults standardUserDefaults] setObject:[firmwareDict valueForKey:@"Language"] forKey:@"FirmwareConfig_Language"];
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
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
	[self change3DRenderDepthComparisonThreshold:[aNotification object]];
}

@end
