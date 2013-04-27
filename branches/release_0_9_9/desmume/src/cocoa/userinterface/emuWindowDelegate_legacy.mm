/*
	Copyright (C) 2007 Jeff Bland
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

#import "emuWindowDelegate_legacy.h"
#import "cocoa_file.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"
#import "video_output_view_legacy.h"
#import "nds_control_legacy.h"
#import "input_legacy.h"


@implementation EmuWindowDelegate

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
	
	iconVolumeFull = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeFull_16x16" ofType:@"png"]];
	iconVolumeTwoThird = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeTwoThird_16x16" ofType:@"png"]];
	iconVolumeOneThird = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeOneThird_16x16" ofType:@"png"]];
	iconVolumeMute = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeMute_16x16" ofType:@"png"]];
	iconExecute = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Execute_420x420" ofType:@"png"]];
	iconPause = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Pause_420x420" ofType:@"png"]];
	iconSpeedNormal = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Speed1x_420x420" ofType:@"png"]];
	iconSpeedDouble = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Speed2x_420x420" ofType:@"png"]];
	
	isRomLoading = NO;
	currentEmuSaveStateURL = nil;
	statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
	minDisplayViewSize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2);
	isSmallestSizeNormal = YES;
	isShowingStatusBar = YES;
	screenshotFileFormat = NSTIFFFileType;
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRomLoaded"];
	[bindings setValue:[NSNumber numberWithFloat:MAX_VOLUME] forKey:@"volume"];
	[bindings setObject:iconVolumeFull forKey:@"volumeIconImage"];
	[bindings setValue:NSSTRING_STATUS_READY forKey:@"status"];
	
	return self;
}

- (void)dealloc
{
	[iconVolumeFull release];
	[iconVolumeTwoThird release];
	[iconVolumeOneThird release];
	[iconVolumeMute release];
	[iconExecute release];
	[iconPause release];
	[iconSpeedNormal release];
	[iconSpeedDouble release];
	[bindings release];
	
	[super dealloc];
}

- (NSMutableDictionary *) bindings
{
	return bindings;
}

- (CocoaDisplayView *) displayView
{
	return displayView;
}

- (void) setContentScalar:(double)s
{
	[displayView setScale:s];
	
	// Resize the window when contentScalar changes.
	NSSize drawBounds = [displayView normalSize];
	[self resizeWithTransform:drawBounds scalar:s rotation:[displayView rotation]];
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
	
	[displayView setRotation:newAngleDegrees];
	
	// Set the minimum content size for the window, since this will change based on rotation.
	NSSize drawBounds = minDisplayViewSize;
	CGSize minContentSize = GetTransformedBounds(drawBounds.width, drawBounds.height, 1.0, CLOCKWISE_DEGREES(newAngleDegrees));
	minContentSize.height += statusBarHeight;
	[window setContentMinSize:NSMakeSize(minContentSize.width, minContentSize.height)];
	
	// Resize the window.
	NSSize oldBounds = [window frame].size;
	[self resizeWithTransform:[displayView normalSize] scalar:[displayView scale] rotation:newAngleDegrees];
	NSSize newBounds = [window frame].size;
	
	// If the window size didn't change, it is possible that the old and new rotation angles
	// are 180 degrees offset from each other. In this case, we'll need to force the
	// display view to update itself.
	if (oldBounds.width == newBounds.width && oldBounds.height == newBounds.height)
	{
		[displayView setFrame:[displayView frame]];
	}
}

- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees
{
	// Convert angle to clockwise-direction degrees.
	angleDegrees = CLOCKWISE_DEGREES(angleDegrees);
	
	// Get the maximum scalar size within drawBounds. Constrain scalar to maxScalar if necessary.
	CGSize checkSize = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, angleDegrees);
	double maxScalar = [self maxContentScalar:NSMakeSize(checkSize.width, checkSize.height)];
	if (scalar > maxScalar)
	{
		scalar = maxScalar;
	}
	
	// Get the new bounds for the window's content view based on the transformed draw bounds.
	CGSize transformedBounds = GetTransformedBounds(normalBounds.width, normalBounds.height, scalar, angleDegrees);
	
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

- (NSURL *) loadedRomURL
{
	NSURL *romURL = nil;
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	if (cdsCore != nil)
	{
		romURL = [cdsCore loadedRomURL];
	}
	
	return romURL;
}

- (void) emulationError
{
	[CocoaDSUtil quickDialogUsingTitle:NSSTRING_ERROR_TITLE_LEGACY message:NSSTRING_ERROR_GENERIC_LEGACY];
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
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
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
	BOOL willClose = [CocoaDSUtil quickYesNoDialogUsingTitle:NSSTRING_MESSAGE_TITLE_LEGACY message:NSSTRING_MESSAGE_ASK_CLOSE_LEGACY];
	if (willClose)
	{
		[self handleUnloadRom:REASONFORCLOSE_NORMAL romToLoad:nil];
	}
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
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
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
	[panel setRequiredFileType:@FILE_EXT_SAVE_STATE];
	
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
		return;
	}
	
	if (i < 0 || i > MAX_SAVESTATE_SLOTS)
	{
		return;
	}
	
	NSString *fileName = [CocoaDSFile saveSlotFileName:[self loadedRomURL] slotNumber:(NSUInteger)(i + 1)];
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
	
	NSString *fileName = [CocoaDSFile saveSlotFileName:[self loadedRomURL] slotNumber:(NSUInteger)(i + 1)];
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
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_ROM_DS, @FILE_EXT_ROM_GBA, nil];
	
	// The NSOpenPanel method -(NSInt)runModalForDirectory:file:types:
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
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
	BOOL result = NO;
	NSInteger buttonClicked;
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setTitle:NSSTRING_TITLE_EXPORT_ROM_SAVE_PANEL];
	[panel setCanCreateDirectories:YES];
	[panel setRequiredFileType:@FILE_EXT_ROM_SAVE_NOGBA];
	
	//save it
	buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		[self pauseCore];
		
		result = [CocoaDSFile exportRomSaveToURL:[panel URL] romSaveURL:[self loadedRomURL] fileType:ROMSAVEFORMAT_NOGBA];
		if (result == NO)
		{
			[self setStatus:NSSTRING_STATUS_ROM_SAVE_EXPORT_FAILED];
			return;
		}
		
		[self restoreCoreState];
		[self setStatus:NSSTRING_STATUS_ROM_SAVE_EXPORTED];
	}
}

- (IBAction) copy:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[cdsCore copyToPasteboard];
}

- (IBAction) executeCoreToggle:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if ([cdsCore paused])
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
		if ([cdsCore paused])
		{
			[displayView setViewToWhite];
		}
		
		[self setStatus:NSSTRING_STATUS_EMULATOR_RESET_LEGACY];
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

- (IBAction) changeRomSaveType:(id)sender
{
	if (![self isRomLoaded])
	{
		return;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setSaveType:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeCoreSpeed:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[cdsCore setSpeedScalar:(CGFloat)[CocoaDSUtil getIBActionSenderTag:sender] / 100.0f];
}

- (IBAction) changeVolume:(id)sender
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	float vol = [self volume];
	
	[self setVolume:vol];
	[self setStatus:[NSString stringWithFormat:NSSTRING_STATUS_VOLUME, vol]];
	[cdsCore setVolume:vol];
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
	double angleDegrees = [displayView rotation] + (double)[CocoaDSUtil getIBActionSenderTag:sender];
	[self setContentRotation:angleDegrees];
}

- (IBAction) toggleGPUState:(id)sender
{
	NSInteger i = [CocoaDSUtil getIBActionSenderTag:sender];
	UInt32 flags = [displayView gpuStateFlags];
	
	flags ^= (1 << i);
	
	[displayView setGpuStateFlags:flags];
}

- (IBAction) toggleMinSize:(id)sender
{
	if (isSmallestSizeNormal)
	{
		minDisplayViewSize.width /= 4;
		minDisplayViewSize.height /= 4;
		isSmallestSizeNormal = NO;
	}
	else
	{
		minDisplayViewSize.width = GPU_DISPLAY_WIDTH;
		minDisplayViewSize.height = GPU_DISPLAY_HEIGHT * 2;
		isSmallestSizeNormal = YES;
	}
	
	// Set the minimum content size, keeping the display rotation in mind.
	CGSize transformedMinSize = GetTransformedBounds(minDisplayViewSize.width, minDisplayViewSize.height, 1.0, CLOCKWISE_DEGREES([displayView rotation]));
	transformedMinSize.height += statusBarHeight;
	[window setContentMinSize:NSMakeSize(transformedMinSize.width, transformedMinSize.height)];
	
	// Resize the window if it's smaller than the minimum content size.
	NSRect windowContentRect = [window contentRectForFrameRect:[window frame]];
	if (windowContentRect.size.width < transformedMinSize.width || windowContentRect.size.height < transformedMinSize.height)
	{
		// Prepare to resize.
		NSRect oldFrameRect = [window frame];
		windowContentRect.size = NSMakeSize(transformedMinSize.width, transformedMinSize.height);
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
	
	[window setFrame:frameRect display:YES animate:NO];
}

- (IBAction) changeScreenshotFileFormat:(id)sender
{
	screenshotFileFormat = (NSBitmapImageFileType)[CocoaDSUtil getIBActionSenderTag:sender];
}

- (IBAction) saveScreenshotAs:(id)sender
{
	[self pauseCore];
	
	NSInteger buttonClicked = NSFileHandlingPanelCancelButton;
	NSSavePanel *panel = [NSSavePanel savePanel];
	
	[panel setCanCreateDirectories:YES];
	[panel setTitle:@"Save Screenshot As"];
	[panel setAccessoryView:saveScreenshotPanelAccessoryView];
	
	buttonClicked = [panel runModal];
	if(buttonClicked == NSOKButton)
	{
		CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
		
		BOOL fileSaved = [CocoaDSFile saveScreenshot:[panel URL] bitmapData:[cdsCore bitmapImageRep] fileType:screenshotFileFormat];
		if (!fileSaved)
		{
			[CocoaDSUtil quickDialogUsingTitle:NSSTRING_ERROR_TITLE_LEGACY message:NSSTRING_ERROR_SCREENSHOT_FAILED_LEGACY];
		}
	}
	
	[self restoreCoreState];
}

- (BOOL) handleLoadRom:(NSURL *)fileURL
{
	BOOL result = NO;
	
	if (isRomLoading)
	{
		return result;
	}
	
	result = [self loadRom:fileURL];
	
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
	result = [self unloadRom];
	
	return result;
}

- (BOOL) loadRom:(NSURL *)romURL
{
	BOOL romLoaded = NO;
	
	if (romURL == nil)
	{
		return romLoaded;
	}
	
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	[self setStatus:NSSTRING_STATUS_ROM_LOADING];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isWorking"];
	[window displayIfNeeded];
	
	// Need to pause the core before loading the ROM.
	[self pauseCore];
	
	// Load the ROM.
	isRomLoading = YES;
	romLoaded = [cdsCore loadRom:romURL];
	if (!romLoaded)
	{
		// If ROM loading fails, restore the core state, but only if a ROM is already loaded.
		if([self isRomLoaded])
		{
			[self restoreCoreState];
		}
		
		[self setStatus:NSSTRING_STATUS_ROM_LOADING_FAILED_LEGACY];
		[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
		
		isRomLoading = NO;
		return romLoaded;
	}
	
	[romInfoPanelController setContent:[cdsCore romInfoBindings]];
	
	// After the ROM loading is complete, send an execute message to the Cocoa DS per
	// user preferences.
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ExecuteROMOnLoad"])
	{
		[self executeCore];
	}
	
	// Add the last loaded ROM to the Recent ROMs list.
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[cdsCore loadedRomURL]];
	
	// Update the UI to indicate that a ROM has indeed been loaded.
	[displayView setViewToWhite];
	[self setStatus:NSSTRING_STATUS_ROM_LOADED_LEGACY];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isWorking"];
	[self setIsRomLoaded:YES];
	[window displayIfNeeded];
	isRomLoading = NO;
	
	romLoaded = YES;
	
	return romLoaded;
}

- (BOOL) unloadRom
{
	BOOL result = NO;
	
	[currentEmuSaveStateURL release];
	currentEmuSaveStateURL = nil;
	[window setDocumentEdited:NO];
	
	// Update the UI to indicate that the ROM has started the process of unloading.
	[self setStatus:NSSTRING_STATUS_ROM_UNLOADING];
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isWorking"];
	[window displayIfNeeded];
	
	// Unload the ROM.
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore closeROM];
	
	// Update the UI to indicate that the ROM has finished unloading.
	[displayView setViewToBlack];
	[self setStatus:NSSTRING_STATUS_ROM_UNLOADED_LEGACY];
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
	[self setStatus:NSSTRING_STATUS_EMULATOR_EXECUTING_LEGACY];
}

- (void) pauseCore
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore setCoreState:CORESTATE_PAUSE];
	[self setStatus:NSSTRING_STATUS_EMULATOR_PAUSED_LEGACY];
}

- (void) restoreCoreState
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore restoreCoreState];
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

- (void)windowDidResize:(NSNotification *)notification
{
	if (displayView == nil)
	{
		return;
	}
	
	NSSize normalBounds = [displayView normalSize];
	double r = [displayView rotation];
	
	// Get the max scalar within the window's current content bounds.
	CGSize checkSize = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, r);
	NSSize contentBounds = [[window contentView] bounds].size;
	contentBounds.height -= statusBarHeight;
	double maxS = GetMaxScalarInBounds(checkSize.width, checkSize.height, contentBounds.width, contentBounds.height);
	
	// Set the display view's properties.
	[displayView setScale:maxS];
	
	// Resize the view.
	NSRect newContentFrame = [[window contentView] bounds];
	newContentFrame.origin.y = statusBarHeight;
	newContentFrame.size.height -= statusBarHeight;
	[displayView setFrame:newContentFrame];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
	NSSize finalSize = frameSize;	
	const NSSize normalBounds = [displayView normalSize];
	
	// Get a content Rect so that we can make our comparison.
	// This will be based on the proposed frameSize.
	const NSRect frameRect = NSMakeRect(0.0f, 0.0f, frameSize.width, frameSize.height);
	const NSRect contentRect = [sender contentRectForFrameRect:frameRect];
	
	// Find the maximum scalar we can use for the display view, bounded by the
	// content Rect.
	const CGSize checkSize = GetTransformedBounds(normalBounds.width, normalBounds.height, 1.0, [displayView rotation]);
	const NSSize contentBounds = NSMakeSize(contentRect.size.width, contentRect.size.height - statusBarHeight);
	const double maxS = GetMaxScalarInBounds(checkSize.width, checkSize.height, contentBounds.width, contentBounds.height);
	[displayView setScale:maxS];
	
	// Make a new content Rect with our max scalar, and convert it back to a frame Rect.
	NSRect finalContentRect = NSMakeRect(0.0f, 0.0f, checkSize.width * maxS, (checkSize.height * maxS) + statusBarHeight);
	NSRect finalFrameRect = [sender frameRectForContentRect:finalContentRect];
	
	// Set the final size based on our new frame Rect.
	finalSize.width = finalFrameRect.size.width;
	finalSize.height = finalFrameRect.size.height;
	
	return finalSize;
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
	[emuWindowController setContent:bindings];
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
			![cdsCore masterExecute])
		{
			enable = NO;
		}
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore paused])
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
			if ([cdsCore paused])
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
			![cdsCore masterExecute])
		{
			enable = NO;
		}
    }
	else if (theAction == @selector(resetCore:))
	{
		if (![self isRomLoaded])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(openRom:))
	{
		if (isRomLoading)
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(closeRom:))
	{
		if (![self isRomLoaded] || isRomLoading)
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(loadEmuSaveStateSlot:))
	{
		if (![self isRomLoaded])
		{
			enable = NO;
		}
		else if (![CocoaDSFile saveStateExistsForSlot:[self loadedRomURL] slotNumber:[theItem tag] + 1])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(saveEmuSaveStateSlot:))
	{
		if (![self isRomLoaded])
		{
			enable = NO;
		}
		
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([CocoaDSFile saveStateExistsForSlot:[self loadedRomURL] slotNumber:[theItem tag] + 1])
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
				if (speedScalar == (NSInteger)(SPEED_SCALAR_QUARTER * 100.0) ||
					speedScalar == (NSInteger)(SPEED_SCALAR_HALF * 100.0) ||
					speedScalar == (NSInteger)(SPEED_SCALAR_NORMAL * 100.0) ||
					speedScalar == (NSInteger)(SPEED_SCALAR_THREE_QUARTER * 100.0) ||
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
	else if (theAction == @selector(changeRomSaveType:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([cdsCore saveType] == [theItem tag])
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
		NSInteger viewScale = (NSInteger)([displayView scale] * 100.0);
		
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
		NSInteger viewRotation = (NSInteger)([displayView rotation]);
		
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
			if ([displayView displayMode] == [theItem tag])
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
		if (![self isRomLoaded])
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(revertEmuSaveState:))
	{
		if (![self isRomLoaded] || currentEmuSaveStateURL == nil)
		{
			enable = NO;
		}
	}
	else if (theAction == @selector(toggleGPUState:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([displayView gpuStateByBit:[theItem tag]])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
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
	else if (theAction == @selector(toggleMinSize:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if (isSmallestSizeNormal)
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

@end
