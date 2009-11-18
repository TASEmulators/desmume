/*  Copyright (C) 2008 Jeff Bland

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

#import "speed_limit_selection_window.h"

@implementation SpeedLimitSelectionWindow

- (id)initWithDS:(NintendoDS*)ds
{
	//Create window
	self = [super initWithContentRect:NSMakeRect(300, 300, 300, 138) styleMask:NSTitledWindowMask|NSClosableWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:NO screen:nil];
	if(self == nil)return nil;
	
	//General
	[self setTitle:NSLocalizedString(@"Custom Speed Limit Window", nil)];
	[self setDelegate:self]; //hopefully this doesn't retain self
	[self setContentMinSize:NSMakeSize(300, 138)];
	[self setContentMaxSize:NSMakeSize(800, 138)];

	//Member setup
	modal = NO;
	
	//Target
	target = ds;
	[target retain];
	initial_value = [target speedLimit];
	
	//Add slider
	NSSlider *slider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 60, 260, 24)];
	[slider setMinValue:10];
	[slider setMaxValue:1000];
	[slider setIntValue:initial_value];
	[slider setTarget:self];
	[slider setAction:@selector(newValue:)];
	[slider setNumberOfTickMarks:2];
	[slider setAutoresizingMask:NSViewWidthSizable];
	[[self contentView] addSubview:slider];
	[slider release];
	
	//Add label
	NSTextField *text = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 101, 260, 17)];
	[text setStringValue:NSLocalizedString(@"Set Max Speed:", nil)];
	[text setSelectable:NO];
	[text setEditable:NO];
	[text setDrawsBackground:NO];
	[text setBordered:NO];
	[text setBezeled:NO];
	[text setAutoresizingMask:NSViewWidthSizable];
	[[self contentView] addSubview:text];
	[text release];

	//Add value text
	value = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 101, 260, 17)];
	[value setStringValue:[NSString stringWithFormat:NSLocalizedString(@"Speed %d%%", nil), initial_value]];
	[value setSelectable:NO];
	[value setEditable:NO];
	[value setDrawsBackground:NO];
	[value setBordered:NO];
	[value setBezeled:NO];
	[value setAlignment:NSRightTextAlignment];
	[value setAutoresizingMask:NSViewWidthSizable];
	[[self contentView] addSubview:value];
	
	//Add OK button
	NSButton *ok_button = [[NSButton alloc] initWithFrame:NSMakeRect(190, 12, 96, 32)];
	[ok_button setTitle:NSLocalizedString(@"OK", nil)];
	[ok_button setButtonType:NSMomentaryPushInButton];
	[ok_button setBezelStyle:NSRoundedBezelStyle];
	[ok_button setTarget:self];
	[ok_button setAction:@selector(ok)];
	[ok_button setAutoresizingMask:NSViewMinXMargin];
	[[self contentView] addSubview:ok_button];
	[ok_button release];

	//Add cancel button
	NSButton *cancel_button = [[NSButton alloc] initWithFrame:NSMakeRect(94, 12, 96, 32)];
	[cancel_button setTitle:NSLocalizedString(@"Cancel", nil)];
	[cancel_button setButtonType:NSMomentaryPushInButton];
	[cancel_button setBezelStyle:NSRoundedBezelStyle];
	[cancel_button setTarget:self];
	[cancel_button setAction:@selector(cancel)];
	[cancel_button setAutoresizingMask:NSViewMinXMargin];
	[[self contentView] addSubview:cancel_button];
	[cancel_button release];

	//Recall size info (happens after adding buttons so the subviews resize accordingly)
	[self setFrameUsingName:@"SpeedLimitSelectionWindow"];
	
	return self;
}

- (void)runModal
{
	modal = YES;
	[NSApp runModalForWindow:self];
}

- (void)dealloc
{
	[self saveFrameUsingName:@"SpeedLimitSelectionWindow"];

	[target release];
	[value release];
	[super dealloc];
}

- (void)newValue:(id)sender
{
	int int_value = [sender intValue];
	[value setStringValue:[NSString stringWithFormat:NSLocalizedString(@"Speed %d%%", nil), int_value]];

	//updates on each value so you can watch it change in realtime
	[target setSpeedLimit:int_value];
}

- (void)ok
{
	if(modal)
	{
		[NSApp stopModal];
		modal = NO;
	}
}

- (void)cancel
{
	if(modal)
	{
		[NSApp stopModal];
		modal = NO;

	}
	[target setSpeedLimit:initial_value];
}

- (BOOL)windowShouldClose:(id)window
{
	[self cancel];
	return NO;
}
@end
