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

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

//DeSmuME Cocoa includes
#import "main_window.h"
#import "screenshot.h"
#import "video_output_view.h"
#import "input.h"
#import "rom_info.h"
#import "preferences.h"

//How much padding to put around the video output
#define WINDOW_BORDER_PADDING 5

#define MAX_FRAME_SKIP 10

#define ROTATION_0   0
#define ROTATION_90  1
#define ROTATION_180 2
#define ROTATION_270 3

#define DS_SCREEN_HEIGHT_COMBINED (192*2) /*height of the two screens*/
#define DS_SCREEN_X_RATIO (256.0 / (192.0 * 2.0))
#define DS_SCREEN_Y_RATIO ((192.0 * 2.0) / 256.0)

//
NSMenuItem *close_rom_item;
NSMenuItem *execute_item;
NSMenuItem *pause_item;
NSMenuItem *reset_item;
NSMenuItem *save_state_as_item;
NSMenuItem *load_state_from_item;
NSMenuItem *saveSlot_item[MAX_SLOTS];
NSMenuItem *loadSlot_item[MAX_SLOTS];
//NSMenuItem *clear_all_saves_item;
NSMenuItem *rom_info_item;
NSMenuItem *frame_skip_auto_item;
NSMenuItem *frame_skip_item[MAX_FRAME_SKIP];

NSMenuItem *volume_item[10];
NSMenuItem *mute_item;

//screen menu items
NSMenuItem *resize1x;
NSMenuItem *resize2x;
NSMenuItem *resize3x;
NSMenuItem *resize4x;
NSMenuItem *constrain_item;
NSMenuItem *min_size_item;
NSMenuItem *toggle_status_bar_item;
NSMenuItem *rotation0_item;
NSMenuItem *rotation90_item;
NSMenuItem *rotation180_item;
NSMenuItem *rotation270_item;
NSMenuItem *topBG0_item;
NSMenuItem *topBG1_item;
NSMenuItem *topBG2_item;
NSMenuItem *topBG3_item;
NSMenuItem *subBG0_item;
NSMenuItem *subBG1_item;
NSMenuItem *subBG2_item;
NSMenuItem *subBG3_item;
NSMenuItem *screenshot_to_file_item;

@implementation VideoOutputWindow

- (void)setStatusText:(NSString*)value
{
	static NSString *retained_value = nil; //stores the string value incease the status_view is deleted and recreated

	if(status_view == nil)
	{
		retained_value = value;
		[retained_value retain];
	} else
	{
		if(value == nil)
		{
			if(retained_value != nil)
			{
				[status_view setStringValue:retained_value];
			} else
				[status_view setStringValue:@""];
		} else
		{
			[status_view setStringValue:value];

			[retained_value release];
			retained_value = value;
			[retained_value retain];
		}
	}
}

- (id)init
{
	//Create the NDS
	self = [super init];

	if(self == nil)return nil;

	//
	NSRect rect;

	//Set vars
	no_smaller_than_ds = true;
	keep_proportions = true;

	//Create the window rect (content rect - title bar not included in size)
	rect.size.width = DS_SCREEN_WIDTH + WINDOW_BORDER_PADDING * 2;
	rect.size.height = DS_SCREEN_HEIGHT_COMBINED + status_bar_height;
	rect.origin.x = 200;
	rect.origin.y = 200;

	//allocate the window
	if((window = [[NSWindow alloc] initWithContentRect:rect styleMask:
	NSTitledWindowMask|NSResizableWindowMask|/*NSClosableWindowMask|*/NSMiniaturizableWindowMask|NSTexturedBackgroundWindowMask
	backing:NSBackingStoreBuffered defer:NO screen:nil])==nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create window");
		return nil;
	}
	[window setTitle:NSLocalizedString(@"DeSmuME Emulator", nil)];

	//Create the image view where we will display video output
	rect.origin.x = WINDOW_BORDER_PADDING;
	rect.origin.y = status_bar_height;
	rect.size.width = DS_SCREEN_WIDTH;
	rect.size.height = DS_SCREEN_HEIGHT_COMBINED;

	if((video_output_view = [[VideoOutputView alloc] initWithFrame:rect withDS:self]) == nil)
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create video output view");

	[[window contentView] addSubview:video_output_view];

	//Add the video update callback
	[self setVideoUpdateCallback:@selector(updateScreen:) withObject:video_output_view];

	//Add the error callback
	[self setErrorCallback:@selector(emulationError) withObject:self];

	//Show the window
	[window setDelegate:self]; //we do this after making the ouput/statusbar incase some delegate method gets called suddenly
	[controller = [[NSWindowController alloc] initWithWindow:window] showWindow:nil];

	//Create the input manager
	input = [[InputHandler alloc] initWithWindow:(id)self];
	NSResponder *temp = [window nextResponder];
	[window setNextResponder:input];
	[input setNextResponder:temp];

	//Start the status bar by default
	//also sets the min window size
	status_view = nil;
	[self setStatusText:NSLocalizedString(@"No ROM loaded", nil)];
	[self toggleStatusBar];

	return self;
}

- (void)dealloc
{
	[video_output_view release];
	[status_view release];

	[self retain]; //see the comment in init after we initialize the window controller
	[controller release];

	[super dealloc];
}

- (BOOL)loadROM:(NSString*)filename
{
	//Attemp to load the rom
	BOOL result = [super loadROM:filename];

	//Set status var and screen depending on whether the rom could be loaded
	if(result == NO)
	{
		[video_output_view clearScreenBlack]; //black if the rom doesn't load
		[self setStatusText:NSLocalizedString(@"No ROM Loaded", nil)];
		messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't load ROM");
	} else
	{
		//if it worked, check the execute upon load option
		if([[NSUserDefaults standardUserDefaults] boolForKey:PREF_EXECUTE_UPON_LOAD])
			[self execute];
		else
		{
			[video_output_view clearScreenWhite]; //white if the rom loaded but is not playing
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
			if([self saveStateExists:i] == YES)
				[saveSlot_item[i] setState:NSOnState];
			else
				[saveSlot_item[i] setState:NSOffState];
	} else
	{
		int i;
		for(i = 0; i < MAX_SLOTS; i++)
			[saveSlot_item[i] setState:NSOffState];
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
		[video_output_view clearScreenWhite];
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

- (void)closeROM
{
	[super closeROM];

	//reset window
	[video_output_view clearScreenBlack];
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
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setTitle:NSLocalizedString(@"Save State...", nil)];
	[panel setRequiredFileType:@"DST"];

	if([panel runModal] == NSFileHandlingPanelOKButton)
		return [self saveState:[panel filename]];

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
		return [self saveState:[panel filename]];

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

- (void)resetMinSize
{
	//keep the min size item up to date, just in case
	if([min_size_item target] == self)
		[min_size_item setState:no_smaller_than_ds?NSOnState:NSOffState];

	if(!no_smaller_than_ds)
	{
		[window setMinSize:NSMakeSize(100, 100)];
		[window setContentMinSize:NSMakeSize(100, 100)];
		return;
	}

	NSSize min_size;

	if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
	{
		min_size.width = DS_SCREEN_WIDTH + WINDOW_BORDER_PADDING * 2;
		min_size.height = DS_SCREEN_HEIGHT_COMBINED + status_bar_height;
		[window setContentMinSize:min_size];

		//resize if too small
		NSSize temp = [video_output_view frame].size;
		if(temp.width < DS_SCREEN_WIDTH || temp.height < DS_SCREEN_HEIGHT_COMBINED)
			[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT_COMBINED)]; //this could be per axis?

	} else
	{
		min_size.width = DS_SCREEN_HEIGHT_COMBINED + WINDOW_BORDER_PADDING * 2;
		min_size.height = DS_SCREEN_WIDTH + status_bar_height;
		[window setContentMinSize:min_size];

		//resize if too small
		NSSize temp = [video_output_view frame].size;
		if(temp.width < DS_SCREEN_HEIGHT_COMBINED || temp.height < DS_SCREEN_WIDTH)
			[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED, DS_SCREEN_WIDTH)];
	}
}

- (void)toggleMinSize
{
	no_smaller_than_ds = !no_smaller_than_ds;

	[self resetMinSize];
}

- (void)resizeScreen:(NSSize)size
{
	//add the window borders
	size.width += WINDOW_BORDER_PADDING * 2;
	size.height += status_bar_height;

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

	if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH, DS_SCREEN_HEIGHT_COMBINED)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED, DS_SCREEN_WIDTH)];
}

- (void)resizeScreen2x
{

	if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 2, DS_SCREEN_HEIGHT_COMBINED * 2)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 2, DS_SCREEN_WIDTH * 2)];
}

- (void)resizeScreen3x
{
	if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 3, DS_SCREEN_HEIGHT_COMBINED * 3)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 3, DS_SCREEN_WIDTH * 3)];
}

- (void)resizeScreen4x
{
	if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
		[self resizeScreen:NSMakeSize(DS_SCREEN_WIDTH * 4, DS_SCREEN_HEIGHT_COMBINED * 4)];
	else
		[self resizeScreen:NSMakeSize(DS_SCREEN_HEIGHT_COMBINED * 4, DS_SCREEN_WIDTH * 4)];
}

- (NSPoint)windowPointToDSCoords:(NSPoint)location
{
	NSSize temp = [[window contentView] frame].size;
	CGFloat x_size = temp.width - WINDOW_BORDER_PADDING*2;
	CGFloat y_size = temp.height - status_bar_height;

	//If the click was to the left or under the opengl view, exit
	if((location.x < WINDOW_BORDER_PADDING) || (location.y < status_bar_height))
		return NSMakePoint(-1, -1);
		
	location.x -= WINDOW_BORDER_PADDING;
	location.y -= status_bar_height;

	//If the click was top or right of the view...
	if(location.x >= x_size)return NSMakePoint(-1, -1);
	if(location.y >= y_size)return NSMakePoint(-1, -1);

	if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
	{
		if([video_output_view rotation] == ROTATION_180)
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

		if([video_output_view rotation] == ROTATION_270)
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
        temp.height -= status_bar_height;
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
		rect.origin.x = WINDOW_BORDER_PADDING;
		rect.origin.y = 0;
		rect.size.width = [video_output_view frame].size.width;
		rect.size.height = 1;
		status_view = [[NSTextField alloc] initWithFrame:rect];

		if(status_view != nil)
		{
			//setup the status view
			[status_view setDrawsBackground:NO];
			[status_view setBordered:NO];
			[status_view setBezeled:NO];
			[status_view setDrawsBackground:NO];
			[status_view setSelectable:NO];
			[status_view setEditable:NO];
			[self setStatusText:nil];
			[status_view sizeToFit];
			rect = [status_view frame];

			//fixme we need to determine the height of the resize handle
			//this is a hack since tiger and earlier have a larger resize handle
			if([NSWindow instancesRespondToSelector:@selector(setPreferredBackingLocation:)] == YES)
				status_bar_height = rect.size.height + rect.origin.y + 2;
			else
				status_bar_height = rect.size.height + rect.origin.y + 6;

			//move the video output view up
			rect = [video_output_view frame];
			rect.origin.y = status_bar_height;
			[video_output_view setFrame:rect];

			//add status view to the window
			[[window contentView] addSubview:status_view];

			//set the check
			if([toggle_status_bar_item target] == self)
			[toggle_status_bar_item setState:NSOnState];
			
			[window setShowsResizeIndicator:YES];

		} else
		{
			messageDialog(NSLocalizedString(@"Error", nil), @"Couldn't create status bar");

			if([toggle_status_bar_item target] == self)
				[toggle_status_bar_item setState:NSOffState]; //should already be off, but just incase

		}

	} else
	{
		//get rid of status view
		[status_view removeFromSuperview];
		[status_view release];
		status_view = nil;
		status_bar_height = WINDOW_BORDER_PADDING;

		//move the video output view down
		rect = [video_output_view frame];
		rect.origin.y = WINDOW_BORDER_PADDING;
		[video_output_view setFrame:rect];

		//unset the check
		if([toggle_status_bar_item target] == self)
			[toggle_status_bar_item setState:NSOffState];
			
		//hide the resize control (so it doesn't overlap our screen)
		[window setShowsResizeIndicator:NO];
	}

	//resize the window
	CGFloat new_height = [window frameRectForContentRect:NSMakeRect(0, 0, [[window contentView] frame].size.width, [video_output_view frame].size.height + status_bar_height)].size.height;
	rect = [window frame];
	rect.origin.y += rect.size.height - new_height;
	rect.size.height = new_height;
	[window setFrame:rect display:YES];

	//set window min size
	[self resetMinSize];
}

- (void)setRotation0
{
	if([video_output_view rotation] == ROTATION_0)return;

	if([video_output_view rotation] == ROTATION_180)
	{
		[video_output_view setRotation:ROTATION_0];

		[video_output_view setFrame:[video_output_view frame]];
		[video_output_view display];

	} else
	{
		//set the rotation state
		[video_output_view setRotation:ROTATION_0];

		//resize the window/view
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];

		//fix the min size
		[self resetMinSize];
	}

	//set the checkmarks
	if([rotation0_item target] == self)
		[rotation0_item   setState:NSOnState ];
	if([rotation90_item target] == self)
		[rotation90_item  setState:NSOffState];
	if([rotation180_item target] == self)
		[rotation180_item setState:NSOffState];
	if([rotation270_item target] == self)
		[rotation270_item setState:NSOffState];
}

- (void)setRotation90
{
	if([video_output_view rotation] == ROTATION_90)return;

	if([video_output_view rotation] == ROTATION_270)
	{

		[video_output_view setRotation:ROTATION_90];

		[video_output_view setFrame:[video_output_view frame]];
		[video_output_view display];

	} else
	{
		//set the rotation state
		[video_output_view setRotation:ROTATION_90];

		//resize the window/screen
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];

		//fix min size
		[self resetMinSize];
	}

	//set the checkmarks
	if([rotation0_item target] == self)
		[rotation0_item   setState:NSOffState];
	if([rotation90_item target] == self)
		[rotation90_item  setState:NSOnState ];
	if([rotation180_item target] == self)
		[rotation180_item setState:NSOffState];
	if([rotation270_item target] == self)
		[rotation270_item setState:NSOffState];
}

- (void)setRotation180
{
	if([video_output_view rotation] == ROTATION_180)return;

	if([video_output_view rotation] == ROTATION_0)
	{
		[video_output_view setRotation:ROTATION_180];

		[video_output_view setFrame:[video_output_view frame]];
		[video_output_view display];

	} else
	{
		//set the rotation state
		[video_output_view setRotation:ROTATION_180];

		//resize the window/view
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];

		//fix the min size
		[self resetMinSize];
	}

	//set the checkmarks
	if([rotation0_item target] == self)
		[rotation0_item   setState:NSOffState];
	if([rotation90_item target] == self)
		[rotation90_item  setState:NSOffState];
	if([rotation180_item target] == self)
		[rotation180_item setState:NSOnState ];
	if([rotation270_item target] == self)
		[rotation270_item setState:NSOffState];
}

- (void)setRotation270
{
	if([video_output_view rotation] == ROTATION_270)return;

	if([video_output_view rotation] == ROTATION_90)
	{
		[video_output_view setRotation:ROTATION_270];

		[video_output_view setFrame:[video_output_view frame]];
		[video_output_view display];

	} else
	{

		//set the rotation state
		[video_output_view setRotation:ROTATION_270];

		//resize the window/screen
		NSSize video_size = [video_output_view frame].size;
		float temp = video_size.width;
		video_size.width = video_size.height;
		video_size.height = temp;
		[self resizeScreen:video_size];

		//fix the min size
		[self resetMinSize];
	}

	//set the checkmarks
	if([rotation0_item target] == self)
		[rotation0_item   setState:NSOffState];
	if([rotation90_item target] == self)
		[rotation90_item  setState:NSOffState];
	if([rotation180_item target] == self)
		[rotation180_item setState:NSOffState];
	if([rotation270_item target] == self)
		[rotation270_item setState:NSOnState];
}

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
{
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
	//get the window size
	NSSize size = [[window contentView] frame].size;
	size.width -= WINDOW_BORDER_PADDING * 2;
	size.height -= status_bar_height;

    //due to rounding errors in the constrain function or whatnot
    //the size can get off slightly, but we can round for the sake of having menu checkmarks appear correct
#define APPRX_EQL(a, b) ((b >= (a - 1)) && (b <= (a + 1)))

	//take rotation into account
    CGFloat ds_screen_width = (([video_output_view rotation] == ROTATION_0) || ([video_output_view rotation] == ROTATION_180)) ? DS_SCREEN_WIDTH : DS_SCREEN_HEIGHT_COMBINED;
    CGFloat ds_screen_height = (([video_output_view rotation] == ROTATION_0) || ([video_output_view rotation] == ROTATION_180)) ? DS_SCREEN_HEIGHT_COMBINED : DS_SCREEN_WIDTH;

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
	//this is called after the window frame resizes
	//so we must recalculate the video output view
	NSRect rect = [[window contentView] frame];
	rect.origin.x = WINDOW_BORDER_PADDING;
	rect.origin.y = status_bar_height;
	rect.size.width -= WINDOW_BORDER_PADDING*2;
	rect.size.height -= status_bar_height;
	[video_output_view setFrame:rect];

    //set the menu items
	[self resetSizeMenuChecks];

    //recalculate the status_view
	rect.origin.y = 0;
	rect.size.height = status_bar_height;
	[status_view setFrame:rect];
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)proposedFrameSize
{

	//constrain proportions
	if(keep_proportions)
	{
		//
		NSSize content_size = [window contentRectForFrameRect:NSMakeRect(0, 0, proposedFrameSize.width, proposedFrameSize.height)].size;

		//this is a simple algorithm to constrain to the smallest axis
		NSSize constrained_size;

		if([video_output_view rotation] == ROTATION_0 || [video_output_view rotation] == ROTATION_180)
		{
			//this is a simple algorithm to constrain to the smallest axis

			constrained_size.width = (content_size.height - status_bar_height) * DS_SCREEN_X_RATIO;
			constrained_size.width += WINDOW_BORDER_PADDING*2;

			constrained_size.height = (content_size.width - WINDOW_BORDER_PADDING*2) * DS_SCREEN_Y_RATIO;
			constrained_size.height += status_bar_height;

		} else
		{
			//like above but with opposite ratios

			constrained_size.width = (content_size.height - status_bar_height) * DS_SCREEN_Y_RATIO;
			constrained_size.width += WINDOW_BORDER_PADDING*2;

			constrained_size.height = (content_size.width - WINDOW_BORDER_PADDING*2) * DS_SCREEN_X_RATIO;
			constrained_size.height += status_bar_height;
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
	[mute_item setState:NSOffState];
}

- (void)setVolumeFromMenu:(id)sender
{
	int i;
	for(i = 0; i < 10; i++)
	{
		if(sender == volume_item[i])
		{
			[volume_item[i] setState:NSOnState];
			[self setVolume:(i+1)*10];
		} else
			[volume_item[i] setState:NSOffState];
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
			[saveSlot_item[i] setState:NSOnState];
		else
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

	for(i = 0; i < MAX_FRAME_SKIP; i++)
	{
		[frame_skip_item[i] setTarget:self];
		if([self frameSkip] == i)
			[frame_skip_item[i] setState:NSOnState];
		else
			[frame_skip_item[i] setState:NSOffState];
	}
	[frame_skip_auto_item setTarget:self];

	if([self frameSkip] < 0)
		[frame_skip_auto_item setState:NSOnState];
	else
		[frame_skip_auto_item setState:NSOffState];

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
	if([video_output_view rotation] == ROTATION_0)
	{
		[rotation0_item   setState:NSOnState ];
		[rotation90_item  setState:NSOffState];
		[rotation180_item setState:NSOffState];
		[rotation270_item setState:NSOffState];
	} else if([video_output_view rotation] == ROTATION_90)
	{
		[rotation0_item   setState:NSOffState];
		[rotation90_item  setState:NSOnState ];
		[rotation180_item setState:NSOffState];
		[rotation270_item setState:NSOffState];
	} else if([video_output_view rotation] == ROTATION_180)
	{
		[rotation0_item   setState:NSOffState];
		[rotation90_item  setState:NSOffState];
		[rotation180_item setState:NSOnState ];
		[rotation270_item setState:NSOffState];
	} else if([video_output_view rotation] == ROTATION_270)
	{
		[rotation0_item   setState:NSOffState];
		[rotation90_item  setState:NSOffState];
		[rotation180_item setState:NSOffState];
		[rotation270_item setState:NSOnState ];
	} else messageDialog(NSLocalizedString(@"Error", nil), @"Corrupt rotation variable");

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
	//This function is called automaticlly by Cocoa
	//when it needs to know if a menu item should be greyed out

	int i;

	if([self ROMLoaded] == NO)
	{ //if no rom is loaded, various options are disables
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


	return YES;
}

@end