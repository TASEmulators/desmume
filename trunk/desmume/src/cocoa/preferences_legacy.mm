/*
	Copyright (C) 2007 Jeff Bland
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

#import "preferences_legacy.h"
#import "cocoa_util.h"
#import "input_legacy.h"

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

const CGFloat PREFERENCES_WIDTH = 365;

///////////////////////////////

NSFont *preferences_font;
NSDictionary *preferences_font_attribs;
NSMutableDictionary *defaults;

///////////////////////////////

//This needs to be called when the program starts
void setAppDefaults()
{
	defaults = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
						
	//Interface defaults
	@"Yes", PREF_EXECUTE_UPON_LOAD,
	PREF_AFTER_LAUNCHED_OPTION_NOTHING, PREF_AFTER_LAUNCHED,
	
	//Firmware defaults
	//@"DeSmuME User", PREF_FIRMWARE_PLAYER_NAME,
	//@"English", PREF_FIRMWARE_LANGUAGE,
		
	nil];
	
	//Input defaults
	[defaults addEntriesFromDictionary:[InputHandler appDefaults]];
	
	[[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
}

///////////////////////////////
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
@interface PreferencesDelegate : NSObject <NSWindowDelegate>
#else
@interface PreferencesDelegate : NSObject
#endif
{}
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
	
	[[NSUserDefaults standardUserDefaults] setBool:value forKey:@"ExecuteROMOnLoad"];
}

- (void)afterLaunch:(id)sender
{
	if([sender indexOfSelectedItem] == 0)
		[[NSUserDefaults standardUserDefaults] setObject:PREF_AFTER_LAUNCHED_OPTION_NOTHING forKey:PREF_AFTER_LAUNCHED];
	else
		[[NSUserDefaults standardUserDefaults] setObject:PREF_AFTER_LAUNCHED_OPTION_LAST_ROM forKey:PREF_AFTER_LAUNCHED];
}

@end

////////////////////////////////////////////////////
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
@interface ToolbarDelegate : NSObject <NSToolbarDelegate>
#else
@interface ToolbarDelegate : NSObject
#endif
{
	NSWindow *window;
	NSToolbarItem *interface;
	NSView *interface_view;
	NSToolbarItem *controls;
	NSView *controls_view;
	NSToolbarItem *ds_firmware;
	NSView *firmware_view;
}
- (id)initWithWindow:(NSWindow*)_window generalView:(NSView*)g controlsView:(NSView*)c firmwareView:(NSView*)f;
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

- (id)initWithWindow:(NSWindow*)_window generalView:(NSView*)g controlsView:(NSView*)c firmwareView:(NSView*)f;
{
	self = [super init];
	if(self == nil)return nil;
	
	window = _window;
	[window retain];
	
	interface_view = g;
	[interface_view retain];
	
	controls_view = c;
	[controls_view retain];
	
	firmware_view = f;
	[firmware_view retain];
	
	interface = controls = ds_firmware = nil;
	
	return self;
}

- (void)dealloc
{
	[interface release];
	[interface_view release];
	[controls release];
	[controls_view release];
	[ds_firmware release];
	[firmware_view release];
	[window release];
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
	return [NSArray arrayWithObjects:INTERFACE_INTERNAL_NAME, CONTROLS_INTERNAL_NAME, /*FIRMWARE_INTERNAL_NAME, */nil];
}

- (NSArray*)toolbarSelectableItemIdentifiers:(NSToolbar*)toolbar
{
	return [NSArray arrayWithObjects:INTERFACE_INTERNAL_NAME, CONTROLS_INTERNAL_NAME, FIRMWARE_INTERNAL_NAME, nil];
}

- (void)toolbarItemClicked:(id)sender
{
	
	//Check what button was clicked 
	
	NSString *item_clicked = [sender itemIdentifier];
	NSView *new_view;
	
	if([item_clicked compare:INTERFACE_INTERNAL_NAME] == NSOrderedSame)
	{
		
		if([interface_view superview] == [window contentView])return;
		new_view = interface_view;
		
	} else if([item_clicked compare:CONTROLS_INTERNAL_NAME] == NSOrderedSame)
	{
		
		if([controls_view superview] == [window contentView])return;
		new_view  = controls_view;
		
	} else if([item_clicked compare:FIRMWARE_INTERNAL_NAME] == NSOrderedSame)
	{
		
		if([firmware_view superview] == [window contentView])return;
		new_view  = firmware_view;
	} else return;
	
	//Add/remove views to show only the view we now want to see
	
	NSView *content_view = [window contentView];
	
	if(interface_view == new_view)[content_view addSubview:interface_view];
	else [interface_view removeFromSuperview];
	
	if(controls_view == new_view)[content_view addSubview:controls_view];
	else [controls_view removeFromSuperview];
	
	if(firmware_view == new_view)[content_view addSubview:firmware_view];
	else [firmware_view removeFromSuperview];
	
	//Resize window to fit the new information perfectly
	//we must take into account the size of the titlebar and toolbar
	//and we also have to recalc the window position to keep the top of the window in the same place,
	//since coordinates are based from the bottom left
	
	NSRect old_window_size = [window frame];
	NSRect new_window_size = [window frameRectForContentRect:[new_view frame]];
	new_window_size.size.width = PREFERENCES_WIDTH;
	new_window_size.origin.x = old_window_size.origin.x;
	new_window_size.origin.y = old_window_size.origin.y + old_window_size.size.height - new_window_size.size.height;
	[window setFrame:new_window_size display:YES animate:YES];
	
}

@end

////////////////////////////////////////////////////

@implementation NSApplication(custom_extensions)

NSView *createPreferencesView(NSString *helpinfo, NSDictionary *options, id delegate)
{
	//create the view
	NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, PREFERENCES_WIDTH, 10)];
	
	//loop through each option in the options list
	//this is done backwards since we build the view upwards
	//fixme: because dictionaries lack order, options should not be passed as a dictionary
	
	NSArray* keys = [[options allKeys] sortedArrayUsingSelector:@selector(localizedCompare:)];	
	
	NSEnumerator *key_enumerator = [keys reverseObjectEnumerator];
	NSString *key;
	NSString *key_raw;
	NSArray *object;
	NSRect text_rect = NSMakeRect(5, 5, 220, 29);
	NSRect button_rect = NSMakeRect(230, 5, PREFERENCES_WIDTH - 235, 26);
	while ((key_raw = [key_enumerator nextObject]))
	{
		object = [options objectForKey:key_raw];
		
		key = NSLocalizedString(key_raw, nil);
		
		NSString *current_setting = [[NSUserDefaults standardUserDefaults] objectForKey:key_raw];
		
		if([(NSString *)[object objectAtIndex:0] compare:@"Bool"] == NSOrderedSame)
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
			
			[button selectItemAtIndex:([[NSUserDefaults standardUserDefaults] boolForKey:@"ExecuteROMOnLoad"] == YES) ? 0 : 1];
			
			[view addSubview:button];
			
		}
		else if([(NSString *)[object objectAtIndex:0] compare:@"Array"] == NSOrderedSame)
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
				NSString *default_setting = [defaults objectForKey:key_raw];
				
				//show an error
				[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:[NSString stringWithFormat:@"%@ setting corrupt (%@), resetting to default (%@)", key, NSLocalizedString(current_setting, nil), NSLocalizedString(default_setting, nil)]];
				
				//set the setting to default
				[[NSUserDefaults standardUserDefaults] setObject:default_setting forKey:key_raw];
				
				//show the default setting in the button
				for(i = 2; i < [object count]; i++)
					if([current_setting compare:[object objectAtIndex:i]] == NSOrderedSame)
						;//[button selectItemAtIndex:i - 2];　fixme
			}
			
			[view addSubview:button];
			
		}
		else if ([(NSString *)[object objectAtIndex:0] caseInsensitiveCompare:@"Text"] == NSOrderedSame)
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
		else if([(NSString *)[object objectAtIndex:0] compare:@"Dictionary"] == NSOrderedSame)
		{
			//Create the button for this option
			NSPopUpButton *button = [[NSPopUpButton alloc] initWithFrame:button_rect pullsDown:NO];

			//button callback
			SEL action;
			[[object objectAtIndex:1] getBytes:&action];
			[button setAction:action];
			[button setTarget:delegate];
			
			NSDictionary* keymap = [object objectAtIndex:2];			
			
			// Sort elements by name
			NSArray* keys = [[keymap allKeys] sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];

			int i;

			for(i = 0; i < [keys count]; i++)
			{
				//add the item to the popup buttons list
				NSString* key = [keys objectAtIndex:i];
				[button addItemWithTitle:NSLocalizedString(key,nil)];
				[[button lastItem] setRepresentedObject:[keymap objectForKey:key]];
				
				if ( [current_setting compare:[keymap valueForKey:key] ] == NSOrderedSame ) {
					[button selectItemAtIndex:i];
				}
			}
				
			[view addSubview:button];
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
		
		//Increase the y
		button_rect.origin.y += 29;
		
	}
	
	//add the info text at the top
	NSText *help_text = [[NSText alloc] initWithFrame:NSMakeRect(5, button_rect.origin.y, PREFERENCES_WIDTH-10, 25)];
	[help_text setDrawsBackground:NO];
	[help_text setVerticallyResizable:YES];
	[help_text setHorizontallyResizable:NO];
	[help_text insertText:helpinfo];
	[help_text setSelectable:NO];
	[help_text setEditable:NO];
	[view addSubview:help_text];
	
	//resize the view to fit the stuff vertically
	NSRect temprect = [view frame];
	temprect.size.height = button_rect.origin.y + [help_text frame].size.height + 3;
	[view setFrame:temprect];
	
	[help_text release];
	
	return [view autorelease];
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
	
	//Create interface view
	NSDictionary *interface_options = [NSDictionary dictionaryWithObjectsAndKeys:
	
	[NSArray arrayWithObjects:@"Bool", [NSData dataWithBytes:&@selector(executeUponLoad:) length:sizeof(SEL)], @"Yes", @"No",nil], PREF_EXECUTE_UPON_LOAD,
	[NSArray arrayWithObjects:@"Array", [NSData dataWithBytes:&@selector(afterLaunch:) length:sizeof(SEL)], PREF_AFTER_LAUNCHED_OPTION_NOTHING, PREF_AFTER_LAUNCHED_OPTION_LAST_ROM, nil], PREF_AFTER_LAUNCHED,
	
	nil];
	
	NSView *interface_view = createPreferencesView(@"Use the popup buttons on the right to change settings", interface_options, delegate);
	
	//Create the controls view
	
	NSView *controls_view = [InputHandler createPreferencesView:PREFERENCES_WIDTH];
	[controls_view setFrame:NSMakeRect(0, 0, PREFERENCES_WIDTH, [controls_view frame].size.height)];
	
	//create the preferences window
	NSWindow *preferences_window = [[NSWindow alloc] initWithContentRect:[interface_view frame] styleMask:
									NSTitledWindowMask|NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO screen:nil];
	[preferences_window setTitle:NSLocalizedString(@"DeSmuME Preferences", nil)];
	[preferences_window setDelegate:delegate];
	[preferences_window setFrameAutosaveName:@"DeSmuME Preferences Window"];
	[preferences_window setFrameOrigin:NSMakePoint(500,500)];
	[[preferences_window contentView] addSubview:interface_view];
	
	//create the toolbar delegate
	ToolbarDelegate *toolbar_delegate = [[ToolbarDelegate alloc]
										 initWithWindow:preferences_window
										 generalView:interface_view
										 controlsView:controls_view
										 firmwareView:nil];
	
	//create the toolbar
	NSToolbar *toolbar =[[NSToolbar alloc] initWithIdentifier:@"DeSmuMe Preferences"];
	[toolbar setDelegate:toolbar_delegate];
	[toolbar setSelectedItemIdentifier:INTERFACE_INTERNAL_NAME]; //start with the general tab selected
	[preferences_window setToolbar:toolbar];
	[toolbar release];
	
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
