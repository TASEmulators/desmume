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

#import "rom_info.h"
#import "../nds_control.h"

bool leopard_or_later = false;
int HUDWindowMask = 1 << 13; //this is NSHUDWindowMask from Leopard, defined here so we can mantain tiger compile compatability

float sectionMaxWidth;

NSWindowController *my_window_controller = nil;
NSWindow *my_window;

NSImageView *rom_icon;
NSTextField *rom_file;
NSTextField *rom_title;
NSTextField *rom_maker;
NSTextField *rom_size;
NSTextField *rom_ARM9_size;
NSTextField *rom_ARM7_size;
NSTextField *rom_data_size;

inline void setUpTextField(NSTextField *text, bool data)
{
	[text setEditable:NO];
	[text setDrawsBackground:NO];
	[text setBordered:NO];
	[text setBezeled:NO];
	if(!data)
	{
		[text setSelectable:NO];
		if(leopard_or_later)
			[text setTextColor:[NSColor whiteColor]];
		else
			[text setTextColor:[NSColor blackColor]];
	} else
	{
		[text setSelectable:YES];
		if(leopard_or_later)
			[text setTextColor:[NSColor colorWithCalibratedRed:.85 green:.85 blue:.9 alpha:1]];
		else
			[text setTextColor:[NSColor colorWithCalibratedRed:.15 green:.15 blue:.1 alpha:1]];
	}
	[text sizeToFit];
}

@implementation ROMInfo

+ (void)showROMInfo:(NintendoDS*)DS;
{
	if(my_window_controller != nil)
	{
		//window has already been created, just change the info
		[ROMInfo changeDS:DS];
		return;
	}
	
	//Check for MAC OS 10.5 or later (if so, we'll use a nice pretty HUD window)
	//this is a bit hacky. check if nswindow will respond to a function thats new in 10.5
	if([NSWindow instancesRespondToSelector:@selector(setPreferredBackingLocation:)] == YES)
		leopard_or_later = true;
	else
		leopard_or_later = false;

	//Window has not been created

	my_window = [[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,360,210) styleMask:(leopard_or_later?HUDWindowMask:0)|NSClosableWindowMask|NSTitledWindowMask|NSUtilityWindowMask backing:NSBackingStoreBuffered defer:NO];
	[my_window setTitle:NSLocalizedString(@"ROM Info...", nil)];

	if(my_window == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"ROM Info window couldn't be created");
		return;
	}

	//Rom icon text
	NSTextField *text = [[NSTextField alloc] initWithFrame:NSMakeRect(15,173,90,17)];
	[text setStringValue:NSLocalizedString(@"ROM Icon", nil)];
	setUpTextField(text, false);
	[[my_window contentView] addSubview:text];
	[text release];

	//Rom icon picture
	rom_icon = [[NSImageView alloc] initWithFrame:NSMakeRect(15, 70, 96, 96)];
	[rom_icon setImageFrameStyle:NSImageFramePhoto];
	[rom_icon setImageScaling:NSScaleToFit];
	[rom_icon setEditable:NO];
	[rom_icon setAllowsCutCopyPaste:NO];
	[rom_icon setImage:[DS ROMIcon]];
	[[my_window contentView] addSubview:rom_icon];

	sectionMaxWidth = 0;
	float valueMaxWidth = 0;

	//Rom file
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,173,96,17)];
	[text setStringValue:NSLocalizedString(@"ROM File", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	//Rom title
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,148,96,17)];
	[text setStringValue:NSLocalizedString(@"ROM Title", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	//Rom maker
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,123,96,17)];
	[text setStringValue:NSLocalizedString(@"ROM Maker", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	//Rom size
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,98,96,17)];
	[text setStringValue:NSLocalizedString(@"ROM Size", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	//ARM 9 Size
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,73,96,17)];
	[text setStringValue:NSLocalizedString(@"ARM9 Size", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	//ARM 7 Size
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,48,96,17)];
	[text setStringValue:NSLocalizedString(@"ARM7 Size", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	//Data Size
	text = [[NSTextField alloc] initWithFrame:NSMakeRect(127,23,96,17)];
	[text setStringValue:NSLocalizedString(@"Data Size", nil)];
	setUpTextField(text, false);
	if([text frame].size.width > sectionMaxWidth)sectionMaxWidth = [text frame].size.width;
	[[my_window contentView] addSubview:text];
	[text release];

	sectionMaxWidth += 5; //padding between the keys and their values
#define text sjdkasdladsjkalsd
	//Rom file value
	rom_file = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,173,120,17)];
	[rom_file setStringValue:[[DS ROMFile] lastPathComponent]];
	setUpTextField(rom_file, true);
	if([rom_file frame].size.width > valueMaxWidth)valueMaxWidth = [rom_file frame].size.width;
	[[my_window contentView] addSubview:rom_file];

	//Rom title value
	rom_title = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,148,120,17)];
	[rom_title setStringValue:[DS ROMTitle]];
	setUpTextField(rom_title, true);
	if([rom_title frame].size.width > valueMaxWidth)valueMaxWidth = [rom_title frame].size.width;
	[[my_window contentView] addSubview:rom_title];

	//Rom maker value
	rom_maker = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,123,120,17)];
	[rom_maker setStringValue:[NSString stringWithFormat:@"%d", [DS ROMMaker]]];
	setUpTextField(rom_maker, true);
	if([rom_maker frame].size.width > valueMaxWidth)valueMaxWidth = [rom_maker frame].size.width;
	[[my_window contentView] addSubview:rom_maker];

	//Rom size value
	rom_size = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,98,120,17)];
	[rom_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMSize]]];
	setUpTextField(rom_size, true);
	if([rom_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_size frame].size.width;
	[[my_window contentView] addSubview:rom_size];

	//ARM 9 Size value
	rom_ARM9_size = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,73,120,17)];
	[rom_ARM9_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMARM9Size]]];
	setUpTextField(rom_ARM9_size, true);
	if([rom_ARM9_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_ARM9_size frame].size.width;
	[[my_window contentView] addSubview:rom_ARM9_size];

	//ARM 7 Size value
	rom_ARM7_size = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,48,120,17)];
	[rom_ARM7_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMARM7Size]]];
	setUpTextField(rom_ARM7_size, true);
	if([rom_ARM7_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_ARM7_size frame].size.width;
	[[my_window contentView] addSubview:rom_ARM7_size];

	//Data Size value
	rom_data_size = [[NSTextField alloc] initWithFrame:NSMakeRect(127+sectionMaxWidth,23,120,17)];
	[rom_data_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMDataSize]]];
	setUpTextField(rom_data_size, true);
	if([rom_data_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_data_size frame].size.width;
	[[my_window contentView] addSubview:rom_data_size];

	valueMaxWidth += 5; //padding after the values column

	//Resize window to fit content

	NSRect temp = [my_window frame];
	temp.size.width = 127 + sectionMaxWidth + valueMaxWidth;
	[my_window setFrame:temp display:NO];

	//Create the window controller and display the window

	my_window_controller = [[NSWindowController alloc] initWithWindow:my_window];
	if(my_window_controller == nil)
	{
		messageDialog(NSLocalizedString(@"Error", nil), @"ROM Info window controller couldn't be created");
		[my_window release];
		return;
	}
	[my_window_controller showWindow:nil];
	[my_window release];
}

+ (void)changeDS:(NintendoDS*)DS
{
	if(my_window_controller == nil)return;

	float valueMaxWidth = 0;

	[rom_icon setImage:[DS ROMIcon]];

	[rom_file setStringValue:[[DS ROMFile] lastPathComponent]];
	[rom_file sizeToFit];
	if([rom_file frame].size.width > valueMaxWidth)valueMaxWidth = [rom_file frame].size.width;

	[rom_title setStringValue:[DS ROMTitle]];
	[rom_title sizeToFit];
	if([rom_title frame].size.width > valueMaxWidth)valueMaxWidth = [rom_title frame].size.width;

	[rom_maker setStringValue:[NSString stringWithFormat:@"%d", [DS ROMMaker]]];
	[rom_maker sizeToFit];
	if([rom_maker frame].size.width > valueMaxWidth)valueMaxWidth = [rom_maker frame].size.width;

	[rom_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMSize]]];
	[rom_size sizeToFit];
	if([rom_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_size frame].size.width;

	[rom_ARM9_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMARM9Size]]];
	[rom_ARM9_size sizeToFit];
	if([rom_ARM9_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_ARM9_size frame].size.width;

	[rom_ARM7_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMARM7Size]]];
	[rom_ARM7_size sizeToFit];
	if([rom_ARM7_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_ARM7_size frame].size.width;

	[rom_data_size setStringValue:[NSString stringWithFormat:@"%d", [DS ROMDataSize]]];
	[rom_data_size sizeToFit];
	if([rom_data_size frame].size.width > valueMaxWidth)valueMaxWidth = [rom_data_size frame].size.width;

	//Resize window to fit new content

	NSRect temp = [my_window frame];
	temp.size.width = 127 + sectionMaxWidth + valueMaxWidth;
	[my_window setFrame:temp display:NO];

}

+ (void)closeROMInfo
{
	if(my_window_controller == nil)return;

	[my_window_controller release];
	my_window_controller = nil;

	[rom_icon release];
	[rom_file release];
	[rom_title release];
	[rom_maker release];
	[rom_size release];
	[rom_ARM9_size release];
	[rom_ARM7_size release];
	[rom_data_size release];
}

@end
