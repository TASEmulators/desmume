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

#import "emuWindowDelegate.h"
#import "EmuControllerDelegate.h"
#import "displayView.h"

#import "cocoa_globals.h"
#import "cocoa_file.h"
#import "cocoa_util.h"

#undef BOOL


@implementation EmuWindowDelegate

@dynamic dummyObject;
@synthesize window;
@synthesize displayView;
@synthesize saveScreenshotPanelAccessoryView;

@synthesize dispViewDelegate;

@synthesize cdsDisplayController;
@synthesize emuControlController;
@synthesize emuWindowController;

@dynamic isMinSizeNormal;

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
	
	dispViewDelegate = nil;
	isShowingStatusBar = YES;
	statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
	minDisplayViewSize = NSMakeSize(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT * 2);
	isMinSizeNormal = YES;
	screenshotFileFormat = NSTIFFFileType;
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isShowingStatusBar"];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(saveScreenshotAsFinish:)
												 name:@"org.desmume.DeSmuME.requestScreenshotDidFinish"
											   object:nil];
	
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[self setDispViewDelegate:nil];
	[bindings release];
	
	[super dealloc];
}

- (void) setContentScalar:(double)s
{
	// Resize the window when contentScalar changes.
	// No need to set the view's scale here since window resizing will implicitly change it.
	[self resizeWithTransform:[dispViewDelegate normalSize] scalar:s rotation:[dispViewDelegate rotation]];
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
	NSSize minContentSize = GetTransformedBounds(minDisplayViewSize, 1.0, CLOCKWISE_DEGREES(newAngleDegrees));
	minContentSize.height += statusBarHeight;
	[window setContentMinSize:minContentSize];
	
	// Resize the window.
	const NSSize oldBounds = [window frame].size;
	[self resizeWithTransform:[dispViewDelegate normalSize] scalar:[dispViewDelegate scale] rotation:newAngleDegrees];
	const NSSize newBounds = [window frame].size;
	
	// If the window size didn't change, it is possible that the old and new rotation angles
	// are 180 degrees offset from each other. In this case, we'll need to force the
	// display view to update itself.
	if (oldBounds.width == newBounds.width && oldBounds.height == newBounds.height)
	{
		[displayView setNeedsDisplay:YES];
	}
}

- (double) resizeWithTransform:(NSSize)normalBounds scalar:(double)scalar rotation:(double)angleDegrees
{
	// Convert angle to clockwise-direction degrees.
	angleDegrees = CLOCKWISE_DEGREES(angleDegrees);
	
	// Get the maximum scalar size within drawBounds. Constrain scalar to maxScalar if necessary.
	const NSSize checkSize = GetTransformedBounds(normalBounds, 1.0, angleDegrees);
	const double maxScalar = [self maxContentScalar:checkSize];
	if (scalar > maxScalar)
	{
		scalar = maxScalar;
	}
	
	// Get the new bounds for the window's content view based on the transformed draw bounds.
	const NSSize transformedBounds = GetTransformedBounds(normalBounds, scalar, angleDegrees);
	
	// Get the center of the content view in screen coordinates.
	const NSRect windowContentRect = [[window contentView] bounds];
	const double translationX = (windowContentRect.size.width - transformedBounds.width) / 2.0;
	const double translationY = ((windowContentRect.size.height - statusBarHeight) - transformedBounds.height) / 2.0;
	
	// Resize the window.
	const NSRect windowFrame = [window frame];
	const NSRect newFrame = [window frameRectForContentRect:NSMakeRect(windowFrame.origin.x + translationX, windowFrame.origin.y + translationY, transformedBounds.width, transformedBounds.height + statusBarHeight)];
	[window setFrame:newFrame display:YES animate:NO];
	
	// Return the actual scale used for the view (may be constrained).
	return scalar;
}

- (double) maxContentScalar:(NSSize)contentBounds
{
	// Determine the maximum scale based on the visible screen size (which
	// doesn't include the menu bar or dock).
	const NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
	const NSRect windowFrame = [window frameRectForContentRect:NSMakeRect(0.0, 0.0, contentBounds.width, contentBounds.height + statusBarHeight)];
	const NSSize visibleScreenBounds = { (screenFrame.size.width - (windowFrame.size.width - contentBounds.width)), (screenFrame.size.height - (windowFrame.size.height - contentBounds.height)) };
	
	return GetMaxScalarInBounds(contentBounds.width, contentBounds.height, visibleScreenBounds.width, visibleScreenBounds.height);
}

- (void) setIsMinSizeNormal:(BOOL)theState
{
	isMinSizeNormal = theState;
	
	if ([dispViewDelegate displayMode] == DS_DISPLAY_TYPE_COMBO)
	{
		if ([dispViewDelegate displayOrientation] == DS_DISPLAY_ORIENTATION_HORIZONTAL)
		{
			minDisplayViewSize.width = GPU_DISPLAY_WIDTH * 2;
			minDisplayViewSize.height = GPU_DISPLAY_HEIGHT;
		}
		else
		{
			minDisplayViewSize.width = GPU_DISPLAY_WIDTH;
			minDisplayViewSize.height = GPU_DISPLAY_HEIGHT * 2;
		}
	}
	else
	{
		minDisplayViewSize.width = GPU_DISPLAY_WIDTH;
		minDisplayViewSize.height = GPU_DISPLAY_HEIGHT;
	}
	
	if (!isMinSizeNormal)
	{
		minDisplayViewSize.width /= 4;
		minDisplayViewSize.height /= 4;
	}
	
	// Set the minimum content size, keeping the display rotation in mind.
	NSSize transformedMinSize = GetTransformedBounds(minDisplayViewSize, 1.0, CLOCKWISE_DEGREES([dispViewDelegate rotation]));
	transformedMinSize.height += statusBarHeight;
	[window setContentMinSize:transformedMinSize];
}

- (BOOL) isMinSizeNormal
{
	return isMinSizeNormal;
}

- (IBAction)copy:(id)sender
{
	[dispViewDelegate copyToPasteboard];
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
	NSInteger newDisplayModeID = [CocoaDSUtil getIBActionSenderTag:sender];
	
	if (newDisplayModeID == [dispViewDelegate displayMode])
	{
		return;
	}
	
	[dispViewDelegate setDisplayMode:newDisplayModeID];
	[self resizeWithTransform:[dispViewDelegate normalSize] scalar:[dispViewDelegate scale] rotation:[dispViewDelegate rotation]];
}

- (IBAction) changeDisplayOrientation:(id)sender
{
	NSInteger newDisplayOrientation = [CocoaDSUtil getIBActionSenderTag:sender];
	
	if (newDisplayOrientation == [dispViewDelegate displayOrientation])
	{
		return;
	}
	
	[dispViewDelegate setDisplayOrientation:newDisplayOrientation];
	[self setIsMinSizeNormal:[self isMinSizeNormal]];
	[self resizeWithTransform:[dispViewDelegate normalSize] scalar:[dispViewDelegate scale] rotation:[dispViewDelegate rotation]];
}

- (IBAction) changeDisplayOrder:(id)sender
{
	[dispViewDelegate setDisplayOrder:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) changeVideoFilter:(id)sender
{
	[dispViewDelegate setVideoFilterType:[CocoaDSUtil getIBActionSenderTag:sender]];
}

- (IBAction) hudDisable:(id)sender
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	
	if ([dispViewDelegate isHudEnabled])
	{
		[dispViewDelegate setIsHudEnabled:NO];
		[emuControl setStatusText:NSSTRING_STATUS_HUD_DISABLED];
	}
	else
	{
		[dispViewDelegate setIsHudEnabled:YES];
		[emuControl setStatusText:NSSTRING_STATUS_HUD_ENABLED];
	}
}

- (IBAction) toggleKeepMinDisplaySizeAtNormal:(id)sender
{
	if ([self isMinSizeNormal])
	{
		[self setIsMinSizeNormal:NO];
	}
	else
	{
		[self setIsMinSizeNormal:YES];
		
		// Set the minimum content size, keeping the display rotation in mind.
		NSSize transformedMinSize = GetTransformedBounds(minDisplayViewSize, 1.0, CLOCKWISE_DEGREES([dispViewDelegate rotation]));
		transformedMinSize.height += statusBarHeight;
		
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
}

- (IBAction) toggleStatusBar:(id)sender
{
	if (isShowingStatusBar)
	{
		[self setShowStatusBar:NO];
	}
	else
	{
		[self setShowStatusBar:YES];
	}
}

- (void) setShowStatusBar:(BOOL)showStatusBar
{
	NSRect frameRect = [window frame];
	
	if (showStatusBar)
	{
		statusBarHeight = WINDOW_STATUS_BAR_HEIGHT;
		frameRect.size.height += WINDOW_STATUS_BAR_HEIGHT;
		
		NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
		if (frameRect.size.height > screenFrame.size.height)
		{
			NSRect windowContentRect = [[window contentView] bounds];
			double widthToHeightRatio = windowContentRect.size.width / windowContentRect.size.height;
			windowContentRect.size.height -= frameRect.size.height - screenFrame.size.height;
			windowContentRect.size.width = windowContentRect.size.height * widthToHeightRatio;
			
			frameRect.size.height = screenFrame.size.height;
			frameRect.size.width = windowContentRect.size.width;			
		}
		else
		{
			frameRect.origin.y -= WINDOW_STATUS_BAR_HEIGHT;
		}
	}
	else
	{
		statusBarHeight = 0;
		frameRect.origin.y += WINDOW_STATUS_BAR_HEIGHT;
		frameRect.size.height -= WINDOW_STATUS_BAR_HEIGHT;
	}
	
	isShowingStatusBar = showStatusBar;
	[[self bindings] setValue:[NSNumber numberWithBool:showStatusBar] forKey:@"isShowingStatusBar"];
	[[NSUserDefaults standardUserDefaults] setBool:showStatusBar forKey:@"DisplayView_ShowStatusBar"];
	[window setFrame:frameRect display:YES animate:NO];
}

- (IBAction) selectScreenshotFileFormat:(id)sender
{
	screenshotFileFormat = (NSBitmapImageFileType)[CocoaDSUtil getIBActionSenderTag:sender];
}

- (IBAction) saveScreenshotAs:(id)sender
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	
	[emuControl pauseCore];
	
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
		[emuControl restoreCoreState];
	}
}

- (void) saveScreenshotAsFinish:(NSNotification *)aNotification
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	NSURL *fileURL = (NSURL *)[[aNotification userInfo] valueForKey:@"fileURL"];
	NSBitmapImageFileType fileType = (NSBitmapImageFileType)[(NSNumber *)[[aNotification userInfo] valueForKey:@"fileType"] integerValue];
	NSImage *screenshotImage = (NSImage *)[[aNotification userInfo] valueForKey:@"screenshotImage"];
	
	BOOL fileSaved = [CocoaDSFile saveScreenshot:fileURL bitmapData:[[screenshotImage representations] objectAtIndex:0] fileType:fileType];
	if (!fileSaved)
	{
		[CocoaDSUtil quickDialogUsingTitle:NSSTRING_ERROR_TITLE_LEGACY message:NSSTRING_ERROR_SCREENSHOT_FAILED_LEGACY];
	}
	
	[emuControl restoreCoreState];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)theItem
{
	BOOL enable = YES;
    SEL theAction = [theItem action];
	
	if (theAction == @selector(changeScale:))
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
			if ([dispViewDelegate displayMode] == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeDisplayOrientation:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([dispViewDelegate displayOrientation] == [theItem tag])
			{
				[(NSMenuItem*)theItem setState:NSOnState];
			}
			else
			{
				[(NSMenuItem*)theItem setState:NSOffState];
			}
		}
	}
	else if (theAction == @selector(changeDisplayOrder:))
	{
		if ([(id)theItem isMemberOfClass:[NSMenuItem class]])
		{
			if ([dispViewDelegate displayOrder] == [theItem tag])
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
	[emuWindowController setContent:[self bindings]];
	[cdsDisplayController setContent:[dispViewDelegate bindings]];
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
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)[emuControlController content];
	
	// If no ROM is loaded, terminate the application.
	if ([emuControl currentRom] == nil)
	{
		[NSApp terminate:sender];
	}
	// If a ROM is loaded, just close the ROM, but don't terminate.
	else
	{
		shouldClose = NO;
		[emuControl closeRom:nil];
	}
	
	return shouldClose;
}

- (void) setupUserDefaults
{
	// Set the display window per user preferences.
	[self setShowStatusBar:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_ShowStatusBar"]];
	
	// Set the display settings per user preferences.
	double displayScalar = (double)([[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Size"] / 100.0);
	double displayRotation = (double)[[NSUserDefaults standardUserDefaults] floatForKey:@"DisplayView_Rotation"];
	[dispViewDelegate setDisplayMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_Mode"]];
	[dispViewDelegate setDisplayOrientation:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Orientation"]];
	[dispViewDelegate setDisplayOrder:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayViewCombo_Order"]];
	[self setContentScalar:displayScalar];
	[self setContentRotation:displayRotation];
	
	// Setup the window display view per user preferences.
	[[self dispViewDelegate] setVideoFilterType:[[NSUserDefaults standardUserDefaults] integerForKey:@"DisplayView_VideoFilter"]];
	[[self dispViewDelegate] setUseBilinearOutput:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseBilinearOutput"]];
	[[self dispViewDelegate] setUseVerticalSync:[[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayView_UseVerticalSync"]];
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

@end
