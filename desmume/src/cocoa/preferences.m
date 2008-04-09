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

#import "preferences.h"
#include "globals.h"

/* Preference settings are stored using NSUserDefaults
which should put them in a property list in /Users/username/Library/Preferences

For the keys we use the same strings you see in the preference menu
such as "Execute Upon Load" to keep things simple, of course the unlocalized version
of the strings are used so that when you change language it will still
finds the settings from before. Also theres no guarantee that localized
strings will be unique.
*/


#define PREF_EXECUTE_UPON_LOAD @"Execute Upon Load"

///////////////////////////////

NSWindow *preferences_window = NULL;

NSFont *preferences_font;
NSDictionary *preferences_font_attribs;

NSTabViewItem *interface_pane_tab;
NSTabViewItem *firmware_pane_tab;

NSText *language_selection_text;
NSPopUpButton *language_selection;

NSDictionary *desmume_defaults;

///////////////////////////////

@interface PreferencesDelegate : NSObject {}
- (void)windowWillClose:(NSNotification*)aNotification;
@end

@implementation PreferencesDelegate
- (void)windowWillClose:(NSNotification*)aNotification
{
	//[preferences_window saveFrameUsingName:@"DeSmuME Preferences Window"];
	//[preferences_window setFrameAutosaveName:@"DeSmuME Preferences Window"];

	[NSApp stopModal];

	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)textDidChange:(NSNotification*)notification
{
	//NSText *text_field = [notification object];
	//NSString *text = [text_field string];


}

- (void)executeUponLoad:(id)sender
{
	BOOL value = ([sender indexOfSelectedItem] == 0) ? YES : NO;

	[[NSUserDefaults standardUserDefaults] setBool:value forKey:PREF_EXECUTE_UPON_LOAD];
}
/*
- (void)numRecentItems:(id)sender
{
	NSString *value = @"invalid";

	if([sender indexOfSelectedItem] == 0)
		value = @"5";
	if([sender indexOfSelectedItem] == 1)
		value = @"10";
	if([sender indexOfSelectedItem] == 2)
		value = @"15";

	[[NSUserDefaults standardUserDefaults] setObject:value forKey:PREF_NUM_RECENT_ITEMS];
}*/

@end

////////////////////////////////////////////////////

@implementation NSApplication(custom_extensions)

NSView *createPreferencesView(NSTabViewItem *tab, NSDictionary *options, id delegate)
{
	//create the view
	NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0,0,400-10,300-10)];
	[tab setView:view];

	//loop through each option in the options list
	NSEnumerator *key_enumerator = [options keyEnumerator];
	NSEnumerator *object_enumerator = [options objectEnumerator];
	id key, key_raw, object;
	NSRect text_rect = NSMakeRect(0, 230, 255, 29);
	NSRect button_rect = NSMakeRect(230, 230, 130, 26);
	while ((key_raw = [key_enumerator nextObject]))
	{
		object = [object_enumerator nextObject];

		key = NSLocalizedString(key_raw, nil);

		NSString *current_setting = [[NSUserDefaults standardUserDefaults] objectForKey:key_raw];

		button_rect.origin.y -= 29;

		if([[object objectAtIndex:0] compare:@"Bool"] == NSOrderedSame)
		{
			//Create the button for this option
			NSPopUpButton *button = [[NSPopUpButton alloc] initWithFrame:button_rect pullsDown:NO];

			//Setup the button callback
			//the items array should have a selector encoded in an NSData
			//since we can't stick a selector directly in an NSArray
			SEL action;
			[[object objectAtIndex:1] getBytes:&action];
			[button setAction:action];
			[button setTarget:delegate];

			[button addItemWithTitle:NSLocalizedString(@"Yes",nil)];
			[button addItemWithTitle:NSLocalizedString(@"No",nil)];

			[button selectItemAtIndex:([[NSUserDefaults standardUserDefaults] boolForKey:PREF_EXECUTE_UPON_LOAD] == YES) ? 0 : 1];

			[view addSubview:button];

		}
		else if([[object objectAtIndex:0] compare:@"Array"] == NSOrderedSame)
		{

			//Create the button for this option
			NSPopUpButton *button = [[NSPopUpButton alloc] initWithFrame:button_rect pullsDown:NO];

			//button callback
			SEL action;
			[[object objectAtIndex:1] getBytes:&action];
			[button setAction:action];
			[button setTarget:delegate];

			int i;
			bool found = false;
			for(i = 2; i < [object count]; i++)
			{
				//add the item to the popup buttons list
				[button addItemWithTitle:NSLocalizedString([object objectAtIndex:i],nil)];

				//if this is the currently selected or default item
				if([current_setting compare:[object objectAtIndex:i]] == NSOrderedSame)
				{
					found = true;
					[button selectItemAtIndex:i - 2];
				}
			}

			if(!found)
			{ //the user setting for this option was not found

				//get the default value
				current_setting = [desmume_defaults objectForKey:key_raw];

				//show an error
				messageDialog(NSLocalizedString(@"Error",nil), [NSString stringWithFormat:NSLocalizedString(@"%@ setting corrupt, resetting to default (%@)",nil),key, NSLocalizedString(current_setting, nil)]);

				//set the setting to default
				[[NSUserDefaults standardUserDefaults] setObject:current_setting forKey:key_raw];

				//show the default setting in the button
				for(i = 2; i < [object count]; i++)
					if([current_setting compare:[object objectAtIndex:i]] == NSOrderedSame)
						;//[button selectItemAtIndex:i - 2];
			}

			[view addSubview:button];

		} else if ([[object objectAtIndex:0] caseInsensitiveCompare:@"Text"] == NSOrderedSame)
		{

			//if this preference is a text field
			//we will create a text field and add it to the view
			NSRect temp = button_rect;
			temp.size.height = [current_setting sizeWithAttributes:preferences_font_attribs].height + 2;
			temp.origin.y += (26. - temp.size.height) / 2.;
			NSText *text = [[NSText alloc] initWithFrame:temp];
			[text setMinSize:temp.size];
			[text setMaxSize:temp.size];
			[text setFont:preferences_font];
			[text setString:current_setting];
			[text setEditable:YES];
			[text setDrawsBackground:YES];
			[text setDelegate:delegate];
			[view addSubview:text];

		}

		//Create text for this option
		text_rect.size.height = [key sizeWithAttributes:preferences_font_attribs].height;
		text_rect.origin.y = button_rect.origin.y + (26. - [key sizeWithAttributes:preferences_font_attribs].height) / 2.;
		NSText *text = [[NSText alloc] initWithFrame:text_rect];
		[text setFont:preferences_font];
		[text setString:key];
		[text setEditable:NO];
		[text setDrawsBackground:NO];
		[text setSelectable:NO];
		[text setVerticallyResizable:NO];
		[view addSubview:text];

	}

	//set the view
	[tab setView:view];

	return view;
}

unsigned char utf8_return = 0x0D;
unsigned char utf8_right[3] = { 0xEF, 0x9C, 0x83 };
unsigned char utf8_up[3] = { 0xEF, 0x9C, 0x80 };
unsigned char utf8_down[3] = { 0xEF, 0x9C, 0x81 };
unsigned char utf8_left[3] = { 0xEF, 0x9C, 0x82 };

void setAppDefaults()
{
	desmume_defaults = [NSDictionary dictionaryWithObjectsAndKeys:

	//Interface defaults
	@"Yes", PREF_EXECUTE_UPON_LOAD,

	//Firmware defaults
	//@"DeSmuME User", PREF_FIRMWARE_PLAYER_NAME,
	//@"English", PREF_FIRMWARE_LANGUAGE,

	//Flash file default
	@"", PREF_FLASH_FILE,
	
	//Key defaults
	@"v", PREF_KEY_A,
	@"b", PREF_KEY_B,
	@"g", PREF_KEY_X,
	@"h", PREF_KEY_Y,
	@"c", PREF_KEY_L,
	@"n", PREF_KEY_R,
	@" ", PREF_KEY_SELECT,
	[[[NSString alloc] initWithBytesNoCopy:utf8_up length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_UP,
	[[[NSString alloc] initWithBytesNoCopy:utf8_down length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_DOWN,
	[[[NSString alloc] initWithBytesNoCopy:utf8_left length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_LEFT,
	[[[NSString alloc] initWithBytesNoCopy:utf8_right length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_RIGHT,
	[[[NSString alloc] initWithBytesNoCopy:&utf8_return length:1 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_START,
	nil];

	[desmume_defaults retain];

	//window size defaults
	NSRect temp;
	temp.origin.x = 600;
	temp.origin.y = 600;
	temp.size.width = 500;
	temp.size.height = 600;
	//[NSData dataWithBytes:&temp length:sizeof(NSRect)], @"DeSmuME Preferences Window", nil];

	[[NSUserDefaults standardUserDefaults] registerDefaults:desmume_defaults];
}

//this is a hack - in the nib we connect preferences to this function name,
//since it's there, and then here we override whatever it's actually supposed to do
//and replace it with the preference panel.
//Incase you were wondering, I actually have no idea what I'm doing.
- (void)orderFrontDataLinkPanel:(id)sender //<- Preferences Display Function
{

	//bool was_paused = paused;
	//[NDS pause]; fixme

	if(!preferences_window)
	{

		//----------------------------------------------------------------------------------------------

		//get the applications main bundle
		//NSBundle* app_bundle = [NSBundle mainBundle];

		//get a font for displaying text
		preferences_font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];

		preferences_font_attribs = [NSDictionary dictionaryWithObjectsAndKeys:preferences_font, NSFontAttributeName, nil];

		//create our delegate
		PreferencesDelegate *delegate = [[PreferencesDelegate alloc] init];

		//Create the window ------------------------------------------------------------------------------

		//create a preferences window
		NSRect rect;
		rect.size.width = 400;
		rect.size.height = 300;
		rect.origin.x = 200;
		rect.origin.y = 200;
		preferences_window = [[NSWindow alloc] initWithContentRect:rect styleMask:
		NSTitledWindowMask|NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO screen:nil];

		//set the window title
		[preferences_window setTitle:NSLocalizedString(@"DeSmuME Preferences", nil)];

		//set the window delegate
		[preferences_window setDelegate:delegate];

		//create a tab view
		rect.size.width = 400 - 10;
		rect.size.height = 300 - 10;
		rect.origin.x = 5;
		rect.origin.y = 5;
		NSTabView *tab_view = [[NSTabView alloc] initWithFrame:rect];
		[[preferences_window contentView] addSubview:tab_view];

		//Create the "Interface" pane
		interface_pane_tab = [[NSTabViewItem alloc] initWithIdentifier:nil];
		[interface_pane_tab setLabel:NSLocalizedString(@"Interface", nil)];
		[tab_view addTabViewItem:interface_pane_tab];

		//Create interface view
		NSDictionary *interface_options = [NSDictionary dictionaryWithObjectsAndKeys:

		[NSArray arrayWithObjects:@"Bool", [NSData dataWithBytes:&@selector(executeUponLoad:) length:sizeof(SEL)], @"Yes", @"No",nil]
		, PREF_EXECUTE_UPON_LOAD, nil];

		/*NSView *interface_view = */createPreferencesView(interface_pane_tab, interface_options, delegate);
/*
		//Create the firmware pane
		firmware_pane_tab = [[NSTabViewItem alloc] initWithIdentifier:nil];
		[firmware_pane_tab setLabel:NSLocalizedString(@"DS Firmware", nil)];
		[tab_view addTabViewItem:firmware_pane_tab];

		NSDictionary *firmware_options = [NSDictionary dictionaryWithObjectsAndKeys:
		//[NSAsrray arrayWithObjects:@"Text", [NSData dataWithBytes:&@selector(playerName:) length:sizeof(SEL)], nil], PREF_FIRMWARE_PLAYER_NAME,
		//[NSArray arrayWithObjects:@"Array", [NSData dataWithBytes:&@selector(nonexisant:) length:sizeof(SEL)], @"Japanese",@"English",@"French",@"German",@"Italian",@"Spanish",nil], PREF_FIRMWARE_LANGUAGE,
		nil];

		NSView *firmware_view = createPreferencesView(firmware_pane_tab, firmware_options, delegate);
*/
	}

	//make the window controller
	NSWindowController *wc = [[NSWindowController alloc] initWithWindow:preferences_window];
	[wc setShouldCascadeWindows:NO];

	//tell it to store/retrieve window frame from/to previous/later sessions
	//[preferences_window setFrameUsingName:@"DeSmuME Preferences Window" force:YES];
	//[preferences_window setFrameAutosaveName:@"DeSmuME Preferences Window"];
	//messageDialog([preferences_window frameAutosaveName],@"");

	//show the window
	[wc showWindow:nil];


	[NSApp runModalForWindow:preferences_window];

	//if(!was_paused)[NDS execute]; fixme
}
@end
