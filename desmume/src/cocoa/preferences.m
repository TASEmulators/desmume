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

#define INTERFACE_INTERNAL_NAME @"Interface"
#define CONTROLS_INTERNAL_NAME @"Controls"
#define FIRMWARE_INTERNAL_NAME @"Firmware"

///////////////////////////////

NSFont *preferences_font;
NSDictionary *preferences_font_attribs;

NSDictionary *desmume_defaults;

///////////////////////////////

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

///////////////////////////////

@interface PreferencesDelegate : NSObject {}
- (void)windowWillClose:(NSNotification*)aNotification;
@end

@implementation PreferencesDelegate
- (void)windowWillClose:(NSNotification*)aNotification
{
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
@end

////////////////////////////////////////////////////

@interface ToolbarDelegate : NSObject
{
	NSTabView *tab_view;
	NSToolbarItem *interface;
	NSToolbarItem *controls;
	NSToolbarItem *ds_firmware;
}
- (id)initWithTabView:(NSTabView*)tabview;
- (void)dealloc;
- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar itemForItemIdentifier:(NSString*)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag;
- (NSArray*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar;
- (NSArray*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar;
- (NSArray*)toolbarSelectableItemIdentifiers:(NSToolbar*)toolbar;
@end

@implementation ToolbarDelegate
- (id)init
{
	//make sure we go through through the designated init function
	[self doesNotRecognizeSelector:_cmd];
	return nil;
}

- (id)initWithTabView:(NSTabView*)tabview
{
	self = [super init];
	if(self == nil)return nil;
	
	tab_view = tabview;
	[tab_view retain];
	
	interface = controls = ds_firmware = nil;
	
	return self;
}

- (void)dealloc
{
	[interface release];
	[controls release];
	[ds_firmware release];
	[tab_view release];
	[super dealloc];
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar itemForItemIdentifier:(NSString*)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
	NSToolbarItem *result;
	
	if([itemIdentifier compare:INTERFACE_INTERNAL_NAME] == NSOrderedSame)
	{
	
		if(interface == nil)
		{
			interface = [[NSToolbarItem alloc] initWithItemIdentifier:INTERFACE_INTERNAL_NAME];
			[interface  setImage:[NSApp applicationIconImage]];
			[interface setLabel:NSLocalizedString(@"Interface", nil)];
			[interface setTarget:self];
			[interface setAction:@selector(toolbarItemClicked:)];
		}
		
		result = interface;
	} else if([itemIdentifier compare:CONTROLS_INTERNAL_NAME] == NSOrderedSame)
	{

		if(controls == nil)
		{
			controls = [[NSToolbarItem alloc] initWithItemIdentifier:CONTROLS_INTERNAL_NAME];
			[controls  setImage:[NSApp applicationIconImage]];
			[controls setLabel:NSLocalizedString(@"Controls", nil)];
			[controls setTarget:self];
			[controls setAction:@selector(toolbarItemClicked:)];
		}
		
		result = controls;
	} else if([itemIdentifier compare:FIRMWARE_INTERNAL_NAME] == NSOrderedSame)
	{

		if(ds_firmware == nil)
		{
			ds_firmware = [[NSToolbarItem alloc] initWithItemIdentifier:FIRMWARE_INTERNAL_NAME];
			[ds_firmware  setImage:[NSApp applicationIconImage]];
			[ds_firmware setLabel:NSLocalizedString(@"DS Firmware", nil)];
			[ds_firmware setTarget:self];
			[ds_firmware setAction:@selector(toolbarItemClicked:)];
		}
		
		result = ds_firmware;
	}
	
	else result = nil;
	
	return result;
}

- (NSArray*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
	return [NSArray arrayWithObjects:INTERFACE_INTERNAL_NAME, CONTROLS_INTERNAL_NAME, FIRMWARE_INTERNAL_NAME, nil];
}

- (NSArray*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar
{
	return [NSArray arrayWithObjects:INTERFACE_INTERNAL_NAME, /*CONTROLS_INTERNAL_NAME, FIRMWARE_INTERNAL_NAME, */nil];
}

- (NSArray*)toolbarSelectableItemIdentifiers:(NSToolbar*)toolbar
{
	return [NSArray arrayWithObjects:INTERFACE_INTERNAL_NAME, CONTROLS_INTERNAL_NAME, FIRMWARE_INTERNAL_NAME, nil];
}

- (void)toolbarItemClicked:(id)sender
{
	[tab_view selectTabViewItemWithIdentifier:[sender itemIdentifier]];
}

@end

////////////////////////////////////////////////////


@implementation NSApplication(custom_extensions)

NSView *createPreferencesView(NSTabViewItem *tab, NSDictionary *options, id delegate)
{
	//create the view
	NSView *view = [[NSView alloc] initWithFrame:[[tab tabView] contentRect]];
	[tab setView:view];

	//loop through each option in the options list
	NSEnumerator *key_enumerator = [options keyEnumerator];
	NSEnumerator *object_enumerator = [options objectEnumerator];
	id key, key_raw, object;
	NSRect text_rect = NSMakeRect(0, [view frame].size.height, 255, 29);
	NSRect button_rect = NSMakeRect(230, [view frame].size.height, 130, 26);
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
	[view release];

	return view;
}

//this is a hack - in the nib we connect preferences to this function name,
//since it's there, and then here we override whatever it's actually supposed to do
//and replace it with the preference panel.
//Incase you were wondering, I actually have no idea what I'm doing.
- (void)orderFrontDataLinkPanel:(id)sender //<- Preferences Display Function
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
	rect.size.width = 365;
	rect.size.height = 300;
	rect.origin.x = 200;
	rect.origin.y = 200;
	NSWindow *preferences_window = [[NSWindow alloc] initWithContentRect:rect styleMask:
	NSTitledWindowMask|NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO screen:nil];
	[preferences_window setTitle:NSLocalizedString(@"DeSmuME Preferences", nil)];
	[preferences_window setDelegate:delegate];
	[preferences_window setFrameAutosaveName:@"DeSmuME Preferences Window"];

	//create a tab view
	NSTabView *tab_view = [[NSTabView alloc] initWithFrame:NSMakeRect(0,0,1,1)];
	[tab_view setTabViewType:NSNoTabsNoBorder];
	[preferences_window setContentView:tab_view];

	//Create the "Interface" pane
	NSTabViewItem *interface_tab = [[NSTabViewItem alloc] initWithIdentifier:INTERFACE_INTERNAL_NAME];
	[tab_view addTabViewItem:interface_tab];
	
	//Create interface view
	NSDictionary *interface_options = [NSDictionary dictionaryWithObjectsAndKeys:

	[NSArray arrayWithObjects:@"Bool", [NSData dataWithBytes:&@selector(executeUponLoad:) length:sizeof(SEL)], @"Yes", @"No",nil]
	, PREF_EXECUTE_UPON_LOAD, nil];

	createPreferencesView(interface_tab, interface_options, delegate);

	[interface_tab release];

	//Create the "Controls" pane
	NSTabViewItem *controls_tab = [[NSTabViewItem alloc] initWithIdentifier:CONTROLS_INTERNAL_NAME];
	[tab_view addTabViewItem:controls_tab];
	[controls_tab release];
	
	//Create the "DS Firmware" pane
	NSTabViewItem *ds_firmware_tab = [[NSTabViewItem alloc] initWithIdentifier:FIRMWARE_INTERNAL_NAME];
	[tab_view addTabViewItem:ds_firmware_tab];
	[ds_firmware_tab release];
	
	//create the toolbar
	NSToolbar *toolbar =[[NSToolbar alloc] initWithIdentifier:@"DeSmuMe Preferences"];

	ToolbarDelegate *toolbar_delegate = [[ToolbarDelegate alloc] initWithTabView:tab_view];
	[toolbar setDelegate:toolbar_delegate];
	
	[toolbar setSelectedItemIdentifier:INTERFACE_INTERNAL_NAME];

	[preferences_window setToolbar:toolbar];
	[toolbar release];
	
	[tab_view release];

	//show the window
	NSWindowController *wc = [[NSWindowController alloc] initWithWindow:preferences_window];
	[wc setShouldCascadeWindows:NO];
	[wc showWindow:nil];
	[wc release];

	//run the preferences
	[NSApp runModalForWindow:preferences_window];

	//
	[toolbar_delegate release];
	[preferences_window release];
}
@end
