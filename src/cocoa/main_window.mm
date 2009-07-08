/*  Copyright (C) 2007 Jeff Bland

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//DeSmuME Cocoa includes
#import "main_window.h"
#import "screenshot.h"
#import "screen_state.h"
#import "video_output_view.h"
#import "input.h"
#import "preferences.h"
#import "dialogs/rom_info.h"
#import "dialogs/speed_limit_selection_window.h"

//How much padding to put around the video output
#define WINDOW_BORDER_PADDING 5

#define MAX_FRAME_SKIP 10

// Save types settings
NSString *save_types[MAX_SAVE_TYPE] = { 
	NSLocalizedString(@"Auto Detect", nil), // 0
	NSLocalizedString(@"EEPROM 4kbit", nil),	// 1
	NSLocalizedString(@"EEPROM 64kbit", nil),	// 2
	NSLocalizedString(@"EEPROM 512kbit", nil),	// 3
	NSLocalizedString(@"FRAM 256knit", nil),	// 4
	NSLocalizedString(@"FLASH 2mbit", nil),	// 5
	NSLocalizedString(@"FLASH 4mbit", nil),	// 6
};


#define DS_SCREEN_HEIGHT_COMBINED (192*2) /*height of the two screens*/
#define DS_SCREEN_X_RATIO (256.0 / (192.0 * 2.0))
#define DS_SCREEN_Y_RATIO ((192.0 * 2.0) / 256.0)

//
NSMenuItem *close_rom_item = nil;
NSMenuItem *execute_item = nil;
NSMenuItem *pause_item = nil;
NSMenuItem *reset_item = nil;
NSMenuItem *save_state_as_item = nil;
NSMenuItem *load_state_from_item = nil;
NSMenuItem *saveSlot_item[MAX_SLOTS] = { nil, nil, nil, nil, nil, nil, nil, nil, nil, nil }; //make sure this corresponds to the amount in max slots
NSMenuItem *loadSlot_item[MAX_SLOTS] = { nil, nil, nil, nil, nil, nil, nil, nil, nil, nil };
NSMenuItem *rom_info_item = nil;
NSMenuItem *frame_skip_auto_item = nil;
NSMenuItem *frame_skip_item[MAX_FRAME_SKIP] = { nil, nil, nil, nil, nil, nil, nil, nil, nil, nil };
NSMenuItem *speed_limit_25_item = nil;
NSMenuItem *speed_limit_50_item = nil;
NSMenuItem *speed_limit_75_item = nil;
NSMenuItem *speed_limit_100_item = nil;
NSMenuItem *speed_limit_200_item = nil;
NSMenuItem *speed_limit_none_item = nil;
NSMenuItem *speed_limit_custom_item = nil;
NSMenuItem *save_type_item[MAX_SAVE_TYPE] = { nil, nil, nil, nil, nil, nil, nil };

NSMenuItem *volume_item[10] = { nil, nil, nil, nil, nil, nil, nil, nil, nil, nil };
NSMenuItem *mute_item = nil;

//screen menu items
NSMenuItem *resize1x = nil;
NSMenuItem *resize2x = nil;
NSMenuItem *resize3x = nil;
NSMenuItem *resize4x = nil;
NSMenuItem *constrain_item = nil;
NSMenuItem *min_size_item = nil;
NSMenuItem *toggle_status_bar_item = nil;
NSMenuItem *rotation0_item = nil;
NSMenuItem *rotation90_item = nil;
NSMenuItem *rotation180_item = nil;
NSMenuItem *rotation270_item = nil;
NSMenuItem *topBG0_item = nil;
NSMenuItem *topBG1_item = nil;
NSMenuItem *topBG2_item = nil;
NSMenuItem *topBG3_item = nil;
NSMenuItem *subBG0_item = nil;
NSMenuItem *subBG1_item = nil;
NSMenuItem *subBG2_item = nil;
NSMenuItem *subBG3_item = nil;
NSMenuItem *screenshot_to_file_item = nil;

@implementation VideoOutputWindow

//Private interface ------------------------------------------------------------

- (void)setStatusText:(NSString*)value
{
	[status_bar_text release];
	status_bar_text = value;
	[status_bar_text retain];

	if(status_view)[status_view setStringValue:status_bar_text];
}

- (NSString*)statusText
{
	if(status_bar_text)return status_bar_text;
	return @"";
}

- (float)statusBarHeight
{
	if(status_view == nil)return WINDOW_BORDER_PADDING;
	NSRect frame = [status_view frame];
	return frame.origin.y + frame.size.height;
}

- (void)resetMinSize:(BOOL)resize_if_too_small
{
	//keep the min size item up to date, just in case
	if([min_size_item target] == self)
		[min_size_item setState:no_smaller_than_ds?NSOnState:NSOffState];

	NSSize min_size;

	if(!no_smaller_than_ds)
	{
		min_size.width = 0;
		min_size.height = 0;
	} else if(video_output_view == nil)
	{
		min_size.width = DS_SCREEN_WIDTH;
		min_size.height = 0;
	} else if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
	{
		min_size.width = DS_SCREEN_WIDTH;
		min_size.height = DS_SCREEN_HEIGHT_COMBINED;
	} else
	{
		min_size.width = DS_SCREEN_HEIGHT_COMBINED;
		min_size.height = DS_SCREEN_WIDTH;
	}

	//
	NSSize adjusted_min_size = min_size;
	adjusted_min_size.width += WINDOW_BORDER_PADDING * 2;
	adjusted_min_size.height += [self statusBarHeight];

	//set min content size for the window
	[window setContentMinSize:adjusted_min_size];

	if(resize_if_too_small)
	{
		NSSize temp = [[window contentView] frame].size;
		if(temp.width < adjusted_min_size.width && temp.height >= adjusted_min_size.height)
			[self resizeScreen:NSMakeSize(min_size.width, temp.height)];
		else if(temp.width >= adjusted_min_size.width && temp.height < adjusted_min_size.height)
			[self resizeScreen:NSMakeSize(temp.width, min_size.height)];
		else if(temp.width < adjusted_min_size.width && temp.height < adjusted_min_size.height)
			[self resizeScreen:NSMakeSize(min_size.width, min_size.height)];
	}
}

//Public Interface -----------------------------------------------------------

- (id)init
{
	//Create the NDS
	self = [super init];
	if(self == nil)return nil; //superclass will display it's own error messages if needed

	//Set vars
	controller = nil;
	video_output_view = nil;
	status_view = nil;
	status_bar_text = nil;
	input = nil;
	no_smaller_than_ds = true;
	keep_proportions = true;

	//Create the window rect (content rect - title bar not included in size)
	NSRect rect;
	rect.size.width = DS_SCREEN_WIDTH + WINDOW_BORDER_PADDING * 2;
	rect.size.height = DS_SCREEN_HEIGHT_COMBINED + [self statusBarHeight];
	rect.origin.x = rect.origin.y = 200;

	//allocate the window
	if((window = [[NSWindow alloc] initWithContentRect:rect styleMask:
	NSTitledWindowMask|NSResizableWindowMask|/*NSClosableWindowMask|*/NSMiniaturizableWindowMask|NSTexturedBackgroundWindowMask
	backing:NSBackingStoreBuffered defer:NO screen:nil])==nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create window");
		[super release];
		return nil;
	}

	[window setTitle:NSLocalizedString(@"DeSmuME Emulator", nil)];
	[window setShowsResizeIndicator:NO];

	//Create the image view where we will display video output
	rect.origin.x = WINDOW_BORDER_PADDING;
	rect.origin.y = [self statusBarHeight];
	rect.size.width -= WINDOW_BORDER_PADDING*2;
	rect.size.height -= [self statusBarHeight];
	video_output_view = [[VideoOutputView alloc] initWithFrame:rect]; //no nil check - will do it's own error messages
	[video_output_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable]; //view will automatically resize with the window

	[[window contentView] addSubview:video_output_view];

	//create the status bar
	[self setStatusText:NSLocalizedString(@"No ROM loaded", nil)];
	[self toggleStatusBar];

	//set min size
	[self resetMinSize:YES];

	//Add callbacks
	[self setVideoUpdateCallback:@selector(setScreenState:) withObject:video_output_view];
	[self setErrorCallback:@selector(emulationError) withObject:self];

	//Show the window
	[window setDelegate:self]; //we do this after making the ouput/statusbar incase some delegate method gets called suddenly
	[controller = [[NSWindowController alloc] initWithWindow:window] showWindow:nil];

	//Create the input manager and insert it into the cocoa responder chain
	input = [[InputHandler alloc] initWithWindow:(id)self];
	NSResponder *temp = [window nextResponder];
	[window setNextResponder:input];
	[input setNextResponder:temp];

	return self;
}

- (void)dealloc
{
	[controller release];
	[window release];
	[video_output_view release];
	[status_view release];
	[status_bar_text release];
	[input release];

	[super dealloc];
}

- (BOOL)loadROM:(NSString*)filename
{
	//Attemp to load the rom
	BOOL result = [super loadROM:filename];

	//Set status var and screen depending on whether the rom could be loaded
	if(result == NO)
	{
		[video_output_view setScreenState:[ScreenState blackScreenState]]; //black if the rom doesn't load
		[self setStatusText:NSLocalizedString(@"No ROM Loaded", nil)];
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't load ROM");
	} else
	{
		//if it worked, check the execute upon load option
		if([[NSUserDefaults standardUserDefaults] boolForKey:PREF_EXECUTE_UPON_LOAD])
			[self execute];
		else
		{
			[video_output_view setScreenState:[ScreenState whiteScreenState]]; //white if the rom loaded but is not playing
			[self setStatusText:NSLocalizedString(@"ROM Loaded", nil)];
		}

		//add the rom to the recent documents list
		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:filename]];

		//update the rom info window, if needed
		[ROMInfo changeDS:self];
	}

	//reset menu items
	if([self ROMLoaded] == YES)
	{
		int i;
		for(i = 0; i < MAX_SLOTS; i++)
			if([saveSlot_item[i] target] == self)
				if([self saveStateExists:i] == YES)
					[saveSlot_item[i] setState:NSOnState];
				else
					[saveSlot_item[i] setState:NSOffState];

	} else
	{
		int i;
		for(i = 0; i < MAX_SLOTS; i++)
			if([saveSlot_item[i] target] == self)[saveSlot_item[i] setState:NSOffState];
	}

	return result;
}

- (void)execute
{
	[super execute];

	if([pause_item target] == self)
		[pause_item setState:NSOffState];
	if([execute_item target] == self)
		[execute_item setState:NSOnState];

 	[self setStatusText:NSLocalizedString(@"Emulation Executing", nil)];
}

- (void)pause
{
	[super pause];

	if([pause_item target] == self)
		[pause_item setState:NSOnState];
 	if([execute_item target] == self)
		[execute_item setState:NSOffState];

 	[self setStatusText:NSLocalizedString(@"Emulation Paused", nil)];
}

- (void)reset
{
	[super reset];

	//if we're paused, clear the screen to show we've reset
	//as opposed to just leaving whatever image is there
	if([self paused])
	{
		[self setStatusText:NSLocalizedString(@"Emulation Reset", nil)];
		[video_output_view setScreenState:[ScreenState whiteScreenState]];
	}
}

- (void)emulationError
{
	messageDialog(NSLocalizedString(@"Error", nil), NSLocalizedString(@"An emulation error occured", nil));
}

- (void)setFrameSkip:(int)frameskip
{
	[super setFrameSkip:frameskip];
	frameskip = [super frameSkip];

	if([frame_skip_auto_item target] == self)
	if(frameskip < 0)
		[frame_skip_auto_item setState:NSOnState];
	else
		[frame_skip_auto_item setState:NSOffState];

	int i;
	for(i = 0; i < MAX_FRAME_SKIP; i++)
		if([frame_skip_item[i] target] == self)
			if(i == frameskip)
				[frame_skip_item[i] setState:NSOnState];
			else
				[frame_skip_item[i] setState:NSOffState];
}

- (void)setFrameSkipFromMenuItem:(id)sender
{
	//see if this was sent from the autoskip menu item
	if(sender == frame_skip_auto_item)
	{
		//set the frame skip to auto
		[self setFrameSkip:-1];
		return;
	}

	//search through the frame skip menu items to find out which one called this function
	int i;
	for(i = 0; i < MAX_FRAME_SKIP; i++)
		if(sender == frame_skip_item[i])
		{
			[self setFrameSkip:i];
			return;
		}
}

- (void)setSpeedLimit:(int)speedLimit
{
	[super setSpeedLimit:speedLimit];

	//Set the correct menu states
	speedLimit = [super speedLimit];
	int standard_size = 0;

	if([speed_limit_25_item target] == self)
		if(speedLimit == 25){ [speed_limit_25_item setState:NSOnState]; standard_size=1; }
		else [speed_limit_25_item setState:NSOffState];

	if([speed_limit_50_item target] == self)
		if(speedLimit == 50){ [speed_limit_50_item setState:NSOnState]; standard_size=1; }
		else [speed_limit_50_item setState:NSOffState];

	if([speed_limit_75_item target] == self)
		if(speedLimit == 75){ [speed_limit_75_item setState:NSOnState]; standard_size=1; }
		else [speed_limit_75_item setState:NSOffState];

	if([speed_limit_100_item target] == self)
		if(speedLimit == 100){ [speed_limit_100_item setState:NSOnState]; standard_size=1; }
		else [speed_limit_100_item setState:NSOffState];

	if([speed_limit_200_item target] == self)
		if(speedLimit == 200){ [speed_limit_200_item setState:NSOnState]; standard_size=1; }
		else [speed_limit_200_item setState:NSOffState];

	if([speed_limit_none_item target] == self)
		if(speedLimit == 0){ [speed_limit_none_item setState:NSOnState]; standard_size=1; }
		else [speed_limit_none_item setState:NSOffState];

	if([speed_limit_custom_item target] == self)
		if(!standard_size)[speed_limit_custom_item setState:NSOnState];
		else [speed_limit_custom_item setState:NSOffState];

}

- (void)setSpeedLimitFromMenuItem:(id)sender
{
	if(sender == speed_limit_25_item  )[self setSpeedLimit:  25];
	if(sender == speed_limit_50_item  )[self setSpeedLimit:  50];
	if(sender == speed_limit_75_item  )[self setSpeedLimit:  75];
	if(sender == speed_limit_100_item )[self setSpeedLimit: 100];
	if(sender == speed_limit_200_item )[self setSpeedLimit: 200];
	if(sender == speed_limit_none_item)[self setSpeedLimit:   0];

	if(sender == speed_limit_custom_item)
	{
		//create
		SpeedLimitSelectionWindow *s_window = [[SpeedLimitSelectionWindow alloc] initWithDS:self];

		//show & run
		NSWindowController *window_controller = [[NSWindowController alloc] initWithWindow:s_window];
		[window_controller showWindow:nil];
		[s_window runModal];

		//release
		[s_window release];
		[window_controller release];
	}
}

- (void)setSaveType:(int)savetype
{
	[super setSaveType:savetype];
	savetype = [super saveType];

	int i;
	for(i = 0; i < MAX_SAVE_TYPE; i++)
		if([save_type_item[i] target] == self)
			if(i == savetype)
				[save_type_item[i] setState:NSOnState];
			else
				[save_type_item[i] setState:NSOffState];
}

- (void)setSaveTypeFromMenuItem:(id)sender
{
	// Find the culprit
	int i;
	for(i = 0; i < MAX_SAVE_TYPE; i++)
		if(sender == save_type_item[i])
		{
			[self setSaveType:i];
			return;
		}
}


- (void)closeROM
{
	[super closeROM];

	//reset window
	[video_output_view setScreenState:[ScreenState blackScreenState]];
	[self setStatusText:NSLocalizedString(@"No ROM Loaded", nil)];

	//reset menu items
	if([window isMainWindow] == YES)
	{
		int i;
		for(i = 0; i < MAX_SLOTS; i++)
			[saveSlot_item[i] setState:NSOffState];

		[execute_item setState:NSOffState];
		[pause_item setState:NSOffState];
	}

	//close ROM info
	[ROMInfo closeROMInfo];
}

- (void)askAndCloseROM
{
	if(messageDialogYN(NSLocalizedString(@"DeSmuME Emulator", nil), NSLocalizedString(@"Are you sure you want to close the ROM?", nil)))
		[self closeROM];
}

- (BOOL)saveState:(NSString*)file
{
	return [super saveState:file];
}

- (BOOL)loadState:(NSString*)file
{
	return [super loadState:file];
}

- (BOOL)saveStateToSlot:(int)slot
{
	if([super saveStateToSlot:slot] == YES)
	{
		if([saveSlot_item[slot] target] == self);
			[saveSlot_item[slot] setState:NSOnState];

		//if([clear_all_saves_item target] == self);
		//[clear_all_saves_item setEnabled:YES];

		return YES;
	}

	return NO;
}

- (BOOL)loadStateFromSlot:(int)slot
{
	return [super loadStateFromSlot:slot];
}

- (BOOL)saveStateAs
{
    //pause emulation so it doesnt save the state after
	BOOL was_executing = [self executing];
	[self pause];

	//file select
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setTitle:NSLocalizedString(@"Save State...", nil)];
	[panel setRequiredFileType:@"DST"];

	//save it
	if([panel runModal] == NSFileHandlingPanelOKButton)
	{
		if(was_executing == YES)[self execute];
		return [self saveState:[panel filename]];
	}

	//unpause emulation if needed
	if(was_executing == YES)[self execute];

	return NO;
}

- (BOOL)loadStateFrom
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setTitle:NSLocalizedString(@"Load State From...", nil)];
	[panel setCanChooseFiles:YES];
	[panel setCanChooseDirectories:NO];
	[panel setAllowsMultipleSelection:NO];

	if([panel runModalForTypes:[NSArray arrayWithObject:@"DST"]] == NSFileHandlingPanelOKButton)
		return [self loadState:[panel filename]];

	return NO;
}

- (BOOL)saveToSlot:(id)menu_item
{
	//get the save slot by detecting which menu item sent the message
	int i;
	for(i = 0; i < MAX_SLOTS; i++)
		if(menu_item == saveSlot_item[i])break;

	//quit if the message wasn't sent by a save state menu item
	if(i >= MAX_SLOTS)return NO;

	return [self saveStateToSlot:i];
}

- (BOOL)loadFromSlot:(id)menu_item
{
	int i;
	for(i = 0; i < MAX_SLOTS; i++)
		if(menu_item == loadSlot_item[i])break;

	if(i >= MAX_SLOTS)return NO;

	return [self loadStateFromSlot:i];
}

- (void)toggleMinSize
{
	no_smaller_than_ds = !no_smaller_than_ds;

	[self resetMinSize:YES];
}

- (void)resizeScreen:(NSSize)size
{
	//add the window borders
	size.width += WINDOW_BORDER_PADDING * 2;
	size.height += [self statusBarHeight];

	//convert size to window frame rather than content frame
	size = [window frameRectForContentRect:NSMakeRect(0, 0, size.width, size.height)].size;

	//constrain
	size = [self windowWillResize:window toSize:size];

	//set the position (keeping the center of the window the same)
	NSRect current_window_rect = [window frame];
	current_window_rect.origin.x -= (size.width - current_window_rect.size.width) / 2;
	current_window_rect.origin.y -= (size.height - current_window_rect.size.height) / 2;

	//and set the size
	current_window_rect.size.width = size.width;
	current_window_rect.size.height = size.height;

	//send it off to cocoa
	[window setFrame:current_window_rect display:YES];
}

- (void)resizeScreen1x
{
	if(video_output_view == nil)return;

	if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT_COMBINED)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED, DS_SCREEN_WIDTH)];
}

- (void)resizeScreen2x
{
	if(video_output_view == nil)return;

	if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 2, DS_SCREEN_HEIGHT_COMBINED * 2)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 2, DS_SCREEN_WIDTH * 2)];
}

- (void)resizeScreen3x
{
	if(video_output_view == nil)return;

	if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 3, DS_SCREEN_HEIGHT_COMBINED * 3)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 3, DS_SCREEN_WIDTH * 3)];
}

- (void)resizeScreen4x
{
	if(video_output_view == nil)return;

	if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 4, DS_SCREEN_HEIGHT_COMBINED * 4)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 4, DS_SCREEN_WIDTH * 4)];
}

- (NSPoint)windowPointToDSCoords:(NSPoint)location
{
	if(video_output_view == nil)return NSMakePoint(-1, -1);

	NSSize temp = [[window contentView] frame].size;
	CGFloat x_size = temp.width - WINDOW_BORDER_PADDING*2;
	CGFloat y_size = temp.height - [self statusBarHeight];

	//If the click was to the left or under the opengl view, exit
	if((location.x < WINDOW_BORDER_PADDING) || (location.y < [self statusBarHeight]))
		return NSMakePoint(-1, -1);

	location.x -= WINDOW_BORDER_PADDING;
	location.y -= [self statusBarHeight];

	//If the click was top or right of the view...
	if(location.x >= x_size)return NSMakePoint(-1, -1);
	if(location.y >= y_size)return NSMakePoint(-1, -1);

	if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
	{
		if([video_output_view boundsRotation] == -180)
		{
			if(location.y < y_size / 2)return NSMakePoint(-1, -1);
			location.x = x_size - location.x;
			location.y = y_size - location.y;
		} else
			if(location.y >= y_size / 2)return NSMakePoint(-1, -1);

		//scale the coordinates
		location.x *= ((float)DS_SCREEN_WIDTH) / ((float)x_size);
		location.y *= ((float)DS_SCREEN_HEIGHT_COMBINED) / ((float)y_size);

		//invert the y
		location.y = DS_SCREEN_HEIGHT - location.y - 1;

	} else
	{

		if([video_output_view boundsRotation] == -270)
		{
			if(location.x < x_size / 2)return NSMakePoint(-1, -1);
			location.x = x_size - location.x;

		} else
		{
			if(location.x >= x_size / 2)return NSMakePoint(-1, -1);
			location.y = y_size - location.y;
		}

		location.x *= ((float)DS_SCREEN_HEIGHT_COMBINED) / ((float)x_size);
		location.y *= ((float)DS_SCREEN_WIDTH) / ((float)y_size);

		//invert the y
		location.x = DS_SCREEN_HEIGHT - location.x - 1;

		float temp = location.x;
		location.x = location.y;
		location.y = temp;
	}

	return location;
}

- (void)toggleConstrainProportions;
{
	//change the state
	keep_proportions = !keep_proportions;

	//constrain
	if(keep_proportions)
	{
        NSSize temp = [[window contentView] frame].size;
        temp.width -= WINDOW_BORDER_PADDING*2;
        temp.height -= [self statusBarHeight];
		[self resizeScreen:temp];
	}

	//set the menu checks
	if([constrain_item target] == self)
		[constrain_item setState:keep_proportions?NSOnState:NSOffState];
}

- (void)toggleStatusBar
{
	NSRect rect;
	if(status_view == nil)
	{
		//Create the status view

//fixme subtract resize handle size to avoid overlap?
		rect = NSMakeRect(WINDOW_BORDER_PADDING, 0, [window frame].size.width - WINDOW_BORDER_PADDING*2, 16);

		status_view = [[NSTextField alloc] initWithFrame:rect];

		if(status_view != nil)
		{
			//setup the status view
			[status_view setDrawsBackground:NO];
			[status_view setBordered:NO];
			[status_view setBezeled:NO];
			[status_view setSelectable:NO];
			[status_view setEditable:NO];
			[status_view setStringValue:[self statusText]];
			[status_view setAutoresizingMask:NSViewWidthSizable];
			rect = [status_view frame];

			//add status view to the window
			[[window contentView] addSubview:status_view];

			//set the menu checkmark
			if([toggle_status_bar_item target] == self)
			[toggle_status_bar_item setState:NSOnState];

			[window setShowsResizeIndicator:YES];

		} else
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create status bar");
			return;
		}

	} else
	{
		//get rid of status view
		[status_view removeFromSuperview];
		[status_view release];
		status_view = nil;

		//unset the menu checkmark
		if([toggle_status_bar_item target] == self)
			[toggle_status_bar_item setState:NSOffState];

		//hide the resize control (so it doesn't overlap our screen)
		[window setShowsResizeIndicator:NO];
	}

	//move the video output view up or down to where the status bar is (or isn't)
	rect = [video_output_view frame];
	rect.origin.y = [self statusBarHeight];
	[video_output_view setFrame:rect];

	//resize the window
	NSUInteger autoresizing_mask = [video_output_view autoresizingMask];
	[video_output_view setAutoresizingMask:NSViewNotSizable]; //dont resize the video output when we resize the window\

	CGFloat new_height = [window frameRectForContentRect:NSMakeRect(0, 0, [[window contentView] frame].size.width, [self statusBarHeight] + (video_output_view==nil?0:[video_output_view frame].size.height))].size.height;

	rect = [window frame];
	rect.origin.y += rect.size.height - new_height;
	rect.size.height = new_height;
	[window setFrame:rect display:YES];

	[video_output_view setAutoresizingMask:autoresizing_mask];

	//set new window min size
	[self resetMinSize:YES];
}

- (void)setRotation:(float)rotation
{
	float current_rotation = [self rotation];

	if(video_output_view && current_rotation != rotation)
	{
		NSSize size = [video_output_view frame].size;

		//rotate view bounds (negative because nsview does it counter-clockwise)
		[video_output_view setBoundsRotation:-rotation];
		[video_output_view setNeedsDisplay:YES];

		//if view is to be rotated (not just flipped)
		if(((current_rotation == 0 || current_rotation == 180) && (rotation == 90 || rotation == 270)) ||
		((current_rotation == 90 || current_rotation == 270) && (rotation == 0 || rotation == 180)))
		{
			//swap x-y sizes
			float temp = size.width;
			size.width = size.height;
			size.height = temp;

			//fit window
			[self resetMinSize:NO];
			[self resizeScreen:size];
		}
	}

	//set the checkmarks
	rotation = video_output_view ? [self rotation] : -1; //no checks set if video output is nil
	if([rotation0_item target] == self)
		if(rotation == 0)[rotation0_item setState:NSOnState];
		else [rotation0_item setState:NSOffState];
	if([rotation90_item target] == self)
		if(rotation == 90)[rotation90_item setState:NSOnState];
		else [rotation90_item setState:NSOffState];
	if([rotation180_item target] == self)
		if(rotation == 180)[rotation180_item setState:NSOnState];
		else [rotation180_item setState:NSOffState];
	if([rotation270_item target] == self)
		if(rotation == 270)[rotation270_item setState:NSOnState];
		else [rotation270_item setState:NSOffState];
}

- (void)setRotation0   { [self setRotation:  0]; }
- (void)setRotation90  { [self setRotation: 90]; }
- (void)setRotation180 { [self setRotation:180]; }
- (void)setRotation270 { [self setRotation:270]; }
- (float)rotation { return video_output_view ? -[video_output_view boundsRotation] : 0; }

- (void)toggleTopBackground0
{
	[super toggleTopBackground0];

	if([topBG0_item target] == self)
	if([super showingTopBackground0] == NO)
		[topBG0_item setState:NSOffState];
	else
		[topBG0_item setState:NSOnState];
}

- (void)toggleTopBackground1
{
	[super toggleTopBackground1];

	if([topBG1_item target] == self)
	if([super showingTopBackground1] == NO)
		[topBG1_item setState:NSOffState];
	else
		[topBG1_item setState:NSOnState];
}

- (void)toggleTopBackground2
{
	[super toggleTopBackground2];

	if([topBG2_item target] == self)
	if([super showingTopBackground2] == NO)
		[topBG2_item setState:NSOffState];
	else
		[topBG2_item setState:NSOnState];
}

- (void)toggleTopBackground3
{
	[super toggleTopBackground3];

	if([topBG3_item target] == self)
	if([super showingTopBackground3] == NO)
		[topBG3_item setState:NSOffState];
	else
		[topBG3_item setState:NSOnState];
}

- (void)toggleSubBackground0
{
	[super toggleSubBackground0];

	if([subBG0_item target] == self)
	if([super showingSubBackground0] == NO)
		[subBG0_item setState:NSOffState];
	else
		[subBG0_item setState:NSOnState];
}

- (void)toggleSubBackground1
{
	[super toggleSubBackground1];

	if([subBG1_item target] == self)
	if([super showingSubBackground1] == NO)
		[subBG1_item setState:NSOffState];
	else
		[subBG1_item setState:NSOnState];
}

- (void)toggleSubBackground2
{
	[super toggleSubBackground2];

	if([subBG2_item target] == self)
	if([super showingSubBackground2] == NO)
		[subBG2_item setState:NSOffState];
	else
		[subBG2_item setState:NSOnState];
}

- (void)toggleSubBackground3
{
	[super toggleSubBackground3];

	if([subBG3_item target] == self)
	if([super showingSubBackground3] == NO)
		[subBG3_item setState:NSOffState];
	else
		[subBG3_item setState:NSOnState];
}

- (void)saveScreenshot
{//todo: this should work even if video_output_view doesn't exist
	BOOL executing = [self executing];
	[self pause];

	[Screenshot saveScreenshotAs:[video_output_view screenState]];

	if(executing == YES)[self execute];
}

- (BOOL)windowShouldClose:(id)sender
{
	[sender hide];
	return FALSE;
}

- (void)resetSizeMenuChecks
{
	if(video_output_view == nil)
	{
		if([resize1x target] == self)[resize1x setState:NSOffState];
		if([resize2x target] == self)[resize2x setState:NSOffState];
		if([resize3x target] == self)[resize3x setState:NSOffState];
		if([resize4x target] == self)[resize4x setState:NSOffState];
		return;
	}

	//get the window size
	NSSize size = [video_output_view frame].size;

    //due to rounding errors in the constrain function or whatnot
	//the size can get off slightly, but we can round for the sake of having menu checkmarks appear correct
#define APPRX_EQL(a, b) ((b >= (a - 1)) && (b <= (a + 1)))

	//take rotation into account
	CGFloat ds_screen_width = (([video_output_view boundsRotation] == 0) || ([video_output_view boundsRotation] == -180)) ? DS_SCREEN_WIDTH : DS_SCREEN_HEIGHT_COMBINED;
	CGFloat ds_screen_height = (([video_output_view boundsRotation] == 0) || ([video_output_view boundsRotation] == -180)) ? DS_SCREEN_HEIGHT_COMBINED : DS_SCREEN_WIDTH;

	if(APPRX_EQL(size.width, ds_screen_width) && APPRX_EQL(size.height, ds_screen_height))
	{
		if([resize1x target] == self)[resize1x setState:NSOnState ];
		if([resize2x target] == self)[resize2x setState:NSOffState];
		if([resize3x target] == self)[resize3x setState:NSOffState];
		if([resize4x target] == self)[resize4x setState:NSOffState];
	} else if(APPRX_EQL(size.width, ds_screen_width*2) && APPRX_EQL(size.height, ds_screen_height*2))
	{
		if([resize1x target] == self)[resize1x setState:NSOffState];
		if([resize2x target] == self)[resize2x setState:NSOnState ];
		if([resize3x target] == self)[resize3x setState:NSOffState];
		if([resize4x target] == self)[resize4x setState:NSOffState];
	} else if(APPRX_EQL(size.width, ds_screen_width*3) && APPRX_EQL(size.height, ds_screen_height*3))
	{
		if([resize1x target] == self)[resize1x setState:NSOffState];
		if([resize2x target] == self)[resize2x setState:NSOffState];
		if([resize3x target] == self)[resize3x setState:NSOnState ];
		if([resize4x target] == self)[resize4x setState:NSOffState];
	} else if(APPRX_EQL(size.width, ds_screen_width*4) && APPRX_EQL(size.height, ds_screen_height*4))
	{
		if([resize1x target] == self)[resize1x setState:NSOffState];
		if([resize2x target] == self)[resize2x setState:NSOffState];
		if([resize3x target] == self)[resize3x setState:NSOffState];
		if([resize4x target] == self)[resize4x setState:NSOnState ];
	} else
	{
		if([resize1x target] == self)[resize1x setState:NSOffState];
		if([resize2x target] == self)[resize2x setState:NSOffState];
		if([resize3x target] == self)[resize3x setState:NSOffState];
		if([resize4x target] == self)[resize4x setState:NSOffState];
	}
}

- (void)windowDidResize:(NSNotification*)aNotification;
{
	if(video_output_view == nil)return;

    //set the menu items
	[self resetSizeMenuChecks];
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)proposedFrameSize
{
	if(video_output_view == nil)
		//allow x resize if theres no video output, but not y resize
		return NSMakeSize(proposedFrameSize.width, [window frameRectForContentRect:NSMakeRect(0, 0, proposedFrameSize.width, [self statusBarHeight])].size.height);

	//constrain proportions
	if(keep_proportions)
	{
		//
		NSSize content_size = [window contentRectForFrameRect:NSMakeRect(0, 0, proposedFrameSize.width, proposedFrameSize.height)].size;

		//this is a simple algorithm to constrain to the smallest axis
		NSSize constrained_size;

		if([video_output_view boundsRotation] == 0 || [video_output_view boundsRotation] == -180)
		{
			//this is a simple algorithm to constrain to the smallest axis

			constrained_size.width = (content_size.height - [self statusBarHeight]) * DS_SCREEN_X_RATIO;
			constrained_size.width += WINDOW_BORDER_PADDING*2;

			constrained_size.height = (content_size.width - WINDOW_BORDER_PADDING*2) * DS_SCREEN_Y_RATIO;
			constrained_size.height += [self statusBarHeight];

		} else
		{
			//like above but with opposite ratios

			constrained_size.width = (content_size.height - [self statusBarHeight]) * DS_SCREEN_Y_RATIO;
			constrained_size.width += WINDOW_BORDER_PADDING*2;

			constrained_size.height = (content_size.width - WINDOW_BORDER_PADDING*2) * DS_SCREEN_X_RATIO;
			constrained_size.height += [self statusBarHeight];
		}

		constrained_size = [window frameRectForContentRect:NSMakeRect(0, 0, constrained_size.width, constrained_size.height)].size;

		if(constrained_size.width < proposedFrameSize.width)proposedFrameSize.width = constrained_size.width;
		if(constrained_size.height < proposedFrameSize.height)proposedFrameSize.height = constrained_size.height;
	}

	//return the [potentially modified] frame size to cocoa to actually resize the window
	return proposedFrameSize;
}

- (void)showRomInfo
{
	[ROMInfo showROMInfo:self];
}

- (void)setVolume:(int)volume
{
	[super setVolume:volume];
	
	int i;
	for(i = 0; i < 10; i++)
	if([volume_item[i] target] == self)
		if(volume == (i+1)*10)
			[volume_item[i] setState:NSOnState];
		else
			[volume_item[i] setState:NSOffState];
}

- (void)setVolumeFromMenu:(id)sender
{
	int i;
	for(i = 0; i < 10; i++)
	if(sender == volume_item[i])
	{
		[self disableMute]; //unmute if needed
		[self setVolume:(i+1)*10];
		break;
	}
}

- (void)enableMute
{
	[super enableMute];

	if([mute_item target] == self)
		[mute_item setState:NSOnState];
}

- (void)disableMute
{
	[super disableMute];
	
	if([mute_item target] == self)
		[mute_item setState:NSOffState];
}

- (void)windowDidBecomeMain:(NSNotification*)notification
{
	int i;

	//FILE options
	[close_rom_item setTarget:self];
	[rom_info_item setTarget:self];
	[save_state_as_item setTarget:self];
	[load_state_from_item setTarget:self];
	for(i = 0; i < MAX_SLOTS; i++)
	{
		[saveSlot_item[i] setTarget:self];
		[loadSlot_item[i] setTarget:self];

		if([self saveStateExists:i] == YES)
		{
			[saveSlot_item[i] setState:NSOnState];
		} else
			[saveSlot_item[i] setState:NSOffState];
	}

	//EXECUTION menu
	[execute_item setTarget:self];
	[pause_item setTarget:self];
	[reset_item setTarget:self];

	if([self ROMLoaded] == YES)
	{
		if([self paused] == YES)
		{
			[execute_item setState:NSOffState];
			[pause_item setState:NSOnState];
		} else
		{
			[execute_item setState:NSOnState];
			[pause_item setState:NSOffState];
		}
	} else
	{
		[execute_item setState:NSOffState];
		[pause_item setState:NSOffState];
	}

	[frame_skip_auto_item setTarget:self];
	for(i = 0; i < MAX_FRAME_SKIP; i++)[frame_skip_item[i] setTarget:self];
	[self setFrameSkip:[self frameSkip]]; //set the menu checkmarks correctly
	
	// Backup media type
	for(i = 0; i < MAX_SAVE_TYPE; i++)[save_type_item[i] setTarget:self];
	[self setSaveType:[self saveType]]; // initalize the menu
		

	[speed_limit_25_item setTarget:self];
	[speed_limit_50_item setTarget:self];
	[speed_limit_75_item setTarget:self];
	[speed_limit_100_item setTarget:self];
	[speed_limit_200_item setTarget:self];
	[speed_limit_none_item setTarget:self];
	[speed_limit_custom_item setTarget:self];
	[self setSpeedLimit:[self speedLimit]]; //this will set the checks correctly

	//VIEW menu

	//view options now target this window
	[resize1x setTarget:self];
	[resize1x setTarget:self];
	[resize2x setTarget:self];
	[resize3x setTarget:self];
	[resize4x setTarget:self];
	[constrain_item setTarget:self];
	[min_size_item setTarget:self];
	[toggle_status_bar_item setTarget:self];
	[rotation0_item setTarget:self];
	[rotation90_item setTarget:self];
	[rotation180_item setTarget:self];
	[rotation270_item setTarget:self];
	[topBG0_item setTarget:self];
	[topBG1_item setTarget:self];
	[topBG2_item setTarget:self];
	[topBG3_item setTarget:self];
	[subBG0_item setTarget:self];
	[subBG1_item setTarget:self];
	[subBG2_item setTarget:self];
	[subBG3_item setTarget:self];
	[screenshot_to_file_item setTarget:self];

	//set checks for view window based on the options of this window
	[self resetSizeMenuChecks];
	[constrain_item setState:keep_proportions?NSOnState:NSOffState];
	[min_size_item setState:no_smaller_than_ds?NSOnState:NSOffState];
	[toggle_status_bar_item setState:(status_view!=nil)?NSOnState:NSOffState];
	if([self showingTopBackground0])
		[topBG0_item setState:NSOnState];
	else
		[topBG0_item setState:NSOffState];
	if([self showingTopBackground1])
		[topBG1_item setState:NSOnState];
	else
		[topBG1_item setState:NSOffState];
	if([self showingTopBackground2])
		[topBG2_item setState:NSOnState];
	else
		[topBG2_item setState:NSOffState];
	if([self showingTopBackground3])
		[topBG3_item setState:NSOnState];
	else
		[topBG3_item setState:NSOffState];
	if([self showingSubBackground0])
		[subBG0_item setState:NSOnState];
	else
		[subBG0_item setState:NSOffState];
	if([self showingSubBackground1])
		[subBG1_item setState:NSOnState];
	else
		[subBG1_item setState:NSOffState];
	if([self showingSubBackground2])
		[subBG2_item setState:NSOnState];
	else
		[subBG2_item setState:NSOffState];
	if([self showingSubBackground3])
		[subBG3_item setState:NSOnState];
	else
		[subBG3_item setState:NSOffState];

	[self setRotation:[self rotation]];

	//SOUND Menu
	int volume;
	[mute_item setTarget:self];
	if([self muted] == YES)
	{
		[mute_item setState:NSOnState];
		volume = -1;
	} else
	{
		[mute_item setState:NSOffState];
		volume = [self volume];
	}

	for(i = 0; i < 10; i++)
	{
		if((i+1)*10 == volume)
			[volume_item[i] setState:NSOnState];
		else
			[volume_item[i] setState:NSOffState];
		[volume_item[i] setTarget:self];
	}

	//Update the ROM Info window
	if([self ROMLoaded] == YES)
		[ROMInfo changeDS:self];
	else
		[ROMInfo closeROMInfo];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item
{
	//This function is called automatically by Cocoa
	//when it needs to know if a menu item should be greyed out

	int i;

	if([self ROMLoaded] == NO)
	{ //if no rom is loaded, various options are disabled
		if(item == close_rom_item)return NO;
		if(item == rom_info_item)return NO;
		if(item == save_state_as_item)return NO;
		if(item == load_state_from_item)return NO;
		for(i = 0; i < MAX_SLOTS; i++)
		{
			if(item == saveSlot_item[i])return NO;
			if(item == loadSlot_item[i])return NO;
		}

		if(item == execute_item)return NO;
		if(item == pause_item)return NO;
		if(item == reset_item)return NO;

		if(item == screenshot_to_file_item)return NO;
	}

	else
		for(i = 0; i < MAX_SLOTS; i++)
			if(item == loadSlot_item[i])
				if([self saveStateExists:i]==NO)return NO;

	if(video_output_view == nil)
	{
		if(item == resize1x)return NO;
		if(item == resize2x)return NO;
		if(item == resize3x)return NO;
		if(item == resize4x)return NO;
		if(item == constrain_item)return NO;
		if(item == rotation0_item)return NO;
		if(item == rotation90_item)return NO;
		if(item == rotation180_item)return NO;
		if(item == rotation270_item)return NO;
		if(item == screenshot_to_file_item)return NO;
	}
	
	if([self hasSound] == NO)
	{
		if(item == mute_item)return NO;
		for(i = 0; i < 10; i++)if(item == volume_item[i])return NO;
	}

	return YES;
}

@end
