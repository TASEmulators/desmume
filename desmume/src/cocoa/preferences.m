#import "globals.h"
#import "nds_control.h"

///////////////////////////////

NSWindow *preferences_window;

NSFont *preferences_font;
NSDictionary *preferences_font_attribs;

NSTabViewItem *interface_pane_tab;
NSTabViewItem *firmware_pane_tab;
NSTabViewItem *plugins_pane_tab;

NSText *language_selection_text;
NSPopUpButton *language_selection;

///////////////////////////////

@interface PreferencesDelegate : NSObject {}
- (void)windowWillClose:(NSNotification *)aNotification;
@end

@implementation PreferencesDelegate
- (void)windowWillClose:(NSNotification *)aNotification
{
	[NSApp stopModal];
}

- (void)languageChange:(id)sender
{

}
@end

////////////////////////////////////////////////////

@implementation NSApplication(custom_extensions)

- (void)about
{
	bool was_paused = paused;
	[NDS pause];

	NSRunAlertPanel(localizedString(@"About DeSmuME", nil),
	@"DeSmuME is an open source Nintendo DS emulator.\n\nBased off of YopYop's original work, and continued by the DeSmuME team.\n\
\nhttp://www.desmume.org\n\n\n\
This program is free software; you can redistribute it and/or \
modify it under the terms of the GNU General Public License as \
published by the Free Software Foundation; either version 2 of \
the License, or (at your option) any later version. \
\n\n\
This program is distributed in the hope that it will be \
useful,but WITHOUT ANY WARRANTY; without even the implied \
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR \
PURPOSE.  See the GNU General Public License for more details. \
\n\n\
You should have received a copy of the GNU General Public \
License along with this program; if not, write to the Free \
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, \
MA  02111-1307  USA\
\n\n\
See the GNU General Public License details in COPYING.", nil/*OK*/, nil, nil);

	if(!was_paused)[NDS execute];
}

///////////////////////////////////////////////////

- (void)preferences
{
	bool was_paused = paused;
	[NDS pause];

	//----------------------------------------------------------------------------------------------

	//get the applications main bundle
	NSBundle* app_bundle = [NSBundle mainBundle];

	//grab the list of languages
	NSArray *languages = [app_bundle localizations];

	//get a font for displaying text
	preferences_font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];

	preferences_font_attribs = [NSDictionary dictionaryWithObjectsAndKeys:preferences_font, NSFontAttributeName, nil];

	//create our delegate
	PreferencesDelegate *delegate = [[PreferencesDelegate alloc] init];
	//
	int i; //general iterator

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
	[preferences_window setTitle:localizedString(@"DeSmuME Preferences", nil)];

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
	[interface_pane_tab setLabel:localizedString(@"Interface", nil)];
	[tab_view addTabViewItem:interface_pane_tab];

	//Create interface view
	rect.size.width = 400 - 10;
	rect.size.height = 300 - 10;
	rect.origin.x = 0;
	rect.origin.y = 0;
	NSView *interface_view = [[NSView alloc] initWithFrame:rect];
	[interface_pane_tab setView:interface_view];

	//Create language selection text
	rect.size.width = 225;
	rect.size.height = 29 - [localizedString(@"Language",nil) sizeWithAttributes:preferences_font_attribs].height / 2;
	rect.origin.x = 0;
	rect.origin.y = 200;// + [localizedString(@"Language",nil) sizeWithAttributes:preferences_font_attribs].height / 2;
	language_selection_text = [[NSText alloc] initWithFrame:rect];
	[language_selection_text setFont: preferences_font];
	[language_selection_text setString:localizedString(@"Language",nil)];
	[language_selection_text setEditable:NO];
	[language_selection_text setDrawsBackground:NO];
	[interface_view addSubview:language_selection_text];

	//Create language selection button
	rect.size.width = 130;
	rect.size.height = 26;
	rect.origin.x = 230;
	rect.origin.y = 200;
	language_selection = [[NSPopUpButton alloc] initWithFrame:rect pullsDown:NO];
	[language_selection addItemWithTitle:@"Default"];
	for(i = 0; i < [languages count]; i++)
		[language_selection addItemWithTitle:[languages objectAtIndex:i]];
	[language_selection setAction:@selector(languageChange:)];
	[language_selection setTarget:delegate];
	[interface_view addSubview:language_selection];

	//Create the "DS Firmware" pane
	firmware_pane_tab = [[NSTabViewItem alloc] initWithIdentifier:nil];
	[firmware_pane_tab setLabel:localizedString(@"DS Firmware", nil)];
	[tab_view addTabViewItem:firmware_pane_tab];

	//Create the "Plugins" pane
	plugins_pane_tab = [[NSTabViewItem alloc] initWithIdentifier:nil];
	[plugins_pane_tab setLabel:localizedString(@"Plugins", nil)];
	[tab_view addTabViewItem:plugins_pane_tab];

	//show the window
	[[[NSWindowController alloc] initWithWindow:preferences_window] showWindow:nil];


	[NSApp runModalForWindow:preferences_window];

	if(!was_paused)[NDS execute];
}
@end
