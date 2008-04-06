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

/*
This file is part of the Cocoa (Mac OS X) port of DeSmuME emulator
By Jeff Bland (reversegecko@gmail.com)
Based on work by yopyop and the DeSmuME team!
*/

#import "main_window.h"
#import "preferences.h"

/*
FIXME: Hardware acceleration for openglrender.c ??
FIXME: When cross-platform (core) components end emulation due to error - pause should be called (set the menu checkmark)
FIXME: .nds.gba support?
*/

//Globals----------------------------------------------------------------------------------------

NSAutoreleasePool *autorelease;

//view (defined/managed in main_window.m)
extern NSMenuItem *resize1x;
extern NSMenuItem *resize2x;
extern NSMenuItem *resize3x;
extern NSMenuItem *resize4x;

extern NSMenuItem *constrain_item;
extern NSMenuItem *min_size_item;
extern NSMenuItem *toggle_status_bar_item;

extern NSMenuItem *rotation0_item;
extern NSMenuItem *rotation90_item;
extern NSMenuItem *rotation180_item;
extern NSMenuItem *rotation270_item;

extern NSMenuItem *topBG0_item;
extern NSMenuItem *topBG1_item;
extern NSMenuItem *topBG2_item;
extern NSMenuItem *topBG3_item;
extern NSMenuItem *subBG0_item;
extern NSMenuItem *subBG1_item;
extern NSMenuItem *subBG2_item;
extern NSMenuItem *subBG3_item;

extern NSMenuItem *screenshot_to_file_item;
extern NSMenuItem *screenshot_to_window_item;

//execution control (defined/managed in nds_control.m)
extern NSMenuItem *close_rom_item;

extern NSMenuItem *execute_item;
extern NSMenuItem *pause_item;
extern NSMenuItem *reset_item;

extern NSMenuItem *save_state_as_item;
extern NSMenuItem *load_state_from_item;

//sound (defined/managed in nds_control.m)
extern NSMenuItem *volume_item[];
extern NSMenuItem *mute_item;

#define SAVE_SLOTS 10 //this should never be more than NB_SAVES in saves.h
extern NSMenuItem *saveSlot_item[];
extern NSMenuItem *loadSlot_item[];

extern NSMenuItem *frame_skip_auto_item;
extern NSMenuItem *frame_skip_item[];

//extern NSMenuItem *clear_all_saves_item; waiting for more functionality from saves.h

extern NSMenuItem *rom_info_item;

//Interfaces--------------------------------------------------------------------------------------

@interface AppDelegate : NSObject
{}
//our methods
- (void)pickROM;

//delegate methods
- (BOOL)application:(NSApplication*)sender openFile:(NSString*)filename;
- (void)application:(NSApplication*)sender openFiles:(NSArray*)filenames;
- (void)applicationWillFinishLaunching:(NSNotification*)aNotification;
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender;
- (void)applicationWillTerminate:(NSNotification*)aNotification;
@end

//
VideoOutputWindow *main_window;

///////////////////////////////

//This generates and sets the entire menubar (and sets defaults)
void CreateMenu(AppDelegate *delegate)
{
	int i;

	NSMenuItem *temp;

	NSMenu* file;
	NSMenu* emulation;
	NSMenu* view;
	NSMenu* sound_menu;
	//NSMenu* window;
	NSMenu* help;

	NSMenu* frame_skip_menu;

	NSMenu *menu = [NSApp mainMenu];

	NSMenuItem *file_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *emulation_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *view_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *sound_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	//NSMenuItem *window_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *help_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];

	//Create the File Menu
	file = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)];
	[menu setSubmenu:file forItem:file_item];

	temp = [file addItemWithTitle:NSLocalizedString(@"Open ROM...", nil) action:@selector(pickROM) keyEquivalent:@"o"];
	[temp setTarget:delegate];

	//recent items menu
	/* Thanks to Jeff Johnson and the Lap Cat Software Blog for their information on the Open Recent menu in Cocoa*/
	/* http://lapcatsoftware.com/blog/ */

	temp = [file addItemWithTitle:NSLocalizedString(@"Open Recent", nil) action:nil keyEquivalent:@""];

	//All the recent documents menu stuff is managed by Cocoa

	NSMenu *recent_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Open Recent", nil)];
	[recent_menu performSelector:@selector(_setMenuName:) withObject:@"NSRecentDocumentsMenu"];
	[temp setSubmenu:recent_menu];

	temp = [recent_menu addItemWithTitle:@"Clear Menu" action:@selector(clearRecentDocuments:) keyEquivalent:@""];
	[temp setTarget:[NSDocumentController sharedDocumentController]];

	[recent_menu release];

	[file addItem:[NSMenuItem separatorItem]];

	rom_info_item = [file addItemWithTitle:NSLocalizedString(@"ROM Info...", nil) action:@selector(showRomInfo) keyEquivalent:@""];

#ifdef HAVE_LIBZ //internally, save states only work when zlib is there

	[file addItem:[NSMenuItem separatorItem]];

	save_state_as_item = [file addItemWithTitle:NSLocalizedString(@"Save State As...", nil) action:@selector(saveStateAs) keyEquivalent:@""];
	[save_state_as_item setEnabled:NO];

	load_state_from_item = [file addItemWithTitle:NSLocalizedString(@"Load State From...", nil) action:@selector(loadStateFrom) keyEquivalent:@""];
	[load_state_from_item setEnabled:NO];

	[file addItem:[NSMenuItem separatorItem]];

	temp = [file addItemWithTitle:NSLocalizedString(@"Save State", nil) action:@selector(saveTo:) keyEquivalent:@""];
	NSMenu *save_state_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Save State", nil)];
	[temp setSubmenu:save_state_menu];

	temp = [file addItemWithTitle:NSLocalizedString(@"Load State", nil) action:nil keyEquivalent:@""];
	NSMenu *load_state_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Load State", nil)];
	[temp setSubmenu:load_state_menu];

	for(i = 0; i < SAVE_SLOTS; i++)
	{
		saveSlot_item[i] = [save_state_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Slot %d", nil), i] action:@selector(saveToSlot:) keyEquivalent:@""];
		[saveSlot_item[i] setEnabled:NO];

		loadSlot_item[i] = [load_state_menu addItemWithTitle:[saveSlot_item[i] title] action:@selector(loadFromSlot:) keyEquivalent:@""];
		[loadSlot_item[i] setEnabled:NO];
	}
//fixme changed save state item function names
/* To be implemented when saves.h provides
a way to get the time of a save that's not string/human formatted...

	[save_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [save_state_menu addItemWithTitle:@"Save Over Oldest" action:nil keyEquivalent:@""];

	temp = [save_state_menu addItemWithTitle:@"Save Over Latest" action:nil keyEquivalent:@""];
*/
/* to be implemented after saves.h provides a way to delete saves...

	[save_state_menu addItem:[NSMenuItem separatorItem]];

	clear_all_saves_item = [save_state_menu addItemWithTitle:@"Clear Menu" action:@selector(askAndClearStates) keyEquivalent:@""];
*/
/*
	[save_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [save_state_menu addItemWithTitle:NSLocalizedString(@"View States", nil) action:nil keyEquivalent:@""];
*/
/* To be implemented when saves.h provides
a way to get the time of a save that's not a string / human formatted...

	[load_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [load_state_menu addItemWithTitle:@"Load Oldest" action:nil keyEquivalent:@""];

	temp = [load_state_menu addItemWithTitle:@"Load Latest" action:nil keyEquivalent:@""];
*/
/*
	[load_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [load_state_menu addItemWithTitle:@"View States" action:nil keyEquivalent:@""];
*/
#else
	for(i = 0; i < SAVE_SLOTS; i++)
	{
		saveSlot_item[i] = nil;
		loadSlot_item[i] = nil;
	}
#endif

	[file addItem:[NSMenuItem separatorItem]];

	close_rom_item = [file addItemWithTitle:NSLocalizedString(@"Close ROM", nil) action:@selector(askAndCloseROM) keyEquivalent:@"w"];

	[file release];

	//Create the Emulation Menu
	emulation = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Emulation", nil)];
	[menu setSubmenu:emulation forItem:emulation_item];

	execute_item = [emulation addItemWithTitle:NSLocalizedString(@"Execute", nil) action:@selector(execute) keyEquivalent:@""];

	pause_item = [emulation addItemWithTitle:NSLocalizedString(@"Pause", nil) action:@selector(pause) keyEquivalent:@""];

	reset_item = [emulation addItemWithTitle:NSLocalizedString(@"Reset", nil) action:@selector(reset) keyEquivalent:@""];

	[emulation addItem:[NSMenuItem separatorItem]];

	temp = [emulation addItemWithTitle:NSLocalizedString(@"Frame Skip", nil) action:nil keyEquivalent:@""];
	frame_skip_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Frame Skip", nil)];
	[temp setSubmenu:frame_skip_menu];

	frame_skip_auto_item = [frame_skip_menu addItemWithTitle:NSLocalizedString(@"Auto", nil) action:@selector(setFrameSkipFromMenuItem:) keyEquivalent:@""];

	[frame_skip_menu addItem:[NSMenuItem separatorItem]];

	frame_skip_item[0] = [frame_skip_menu addItemWithTitle:NSLocalizedString(@"Off", nil) action:@selector(setFrameSkipFromMenuItem:) keyEquivalent:@""];

	for(i = 1; i < MAX_FRAME_SKIP; i++)
	{
		frame_skip_item[i] = [frame_skip_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Skip %d", nil), i] action:@selector(setFrameSkipFromMenuItem:) keyEquivalent:@""];
	}

	[frame_skip_menu release];

	[emulation addItem:[NSMenuItem separatorItem]];
	
	temp = [emulation addItemWithTitle:NSLocalizedString(@"Set FAT Image File...", nil) action:@selector(pickFlash) keyEquivalent:@""];
	
	[emulation release];

	//Create the screens menu
	view = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
	[menu setSubmenu:view forItem:view_item];

	resize1x = [view addItemWithTitle:NSLocalizedString(@"Size 1x", nil) action:@selector(resizeScreen1x) keyEquivalent:@"1"];
	resize2x = [view addItemWithTitle:NSLocalizedString(@"Size 2x", nil) action:@selector(resizeScreen2x) keyEquivalent:@"2"];
	resize3x = [view addItemWithTitle:NSLocalizedString(@"Size 3x", nil) action:@selector(resizeScreen3x) keyEquivalent:@"3"];
	resize4x = [view addItemWithTitle:NSLocalizedString(@"Size 4x", nil) action:@selector(resizeScreen4x) keyEquivalent:@"4"];

	[view addItem:[NSMenuItem separatorItem]];
/*
	[view addItemWithTitle:NSLocalizedString(@"Full Screen", nil) action:nil keyEquivalent:@"f"];

	[view addItem:[NSMenuItem separatorItem]];
*/
	constrain_item = [view addItemWithTitle:NSLocalizedString(@"Constrain Proportions", nil) action:@selector(toggleConstrainProportions) keyEquivalent:@""];
	min_size_item = [view addItemWithTitle:NSLocalizedString(@"No Smaller Than DS", nil) action:@selector(toggleMinSize) keyEquivalent:@""];
	toggle_status_bar_item = [view addItemWithTitle:NSLocalizedString(@"Show Status Bar", nil) action:@selector(toggleStatusBar) keyEquivalent:@"/"];

	[view addItem:[NSMenuItem separatorItem]];

	temp = [view addItemWithTitle:NSLocalizedString(@"Rotation", nil) action:nil keyEquivalent:@""];
	NSMenu *rotation_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Rotation", nil)];
	[temp setSubmenu: rotation_menu];

	rotation0_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 0", nil) action:@selector(setRotation0) keyEquivalent:@""];
	rotation90_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 90", nil) action:@selector(setRotation90) keyEquivalent:@""];
	rotation180_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 180", nil) action:@selector(setRotation180) keyEquivalent:@""];
	rotation270_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 270", nil) action:@selector(setRotation270) keyEquivalent:@""];

	[rotation_menu release];

	[view addItem:[NSMenuItem separatorItem]];

	temp = [view addItemWithTitle:NSLocalizedString(@"Layers", nil) action:nil keyEquivalent:@""];
	NSMenu *layer_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Layers", nil)];
	[temp setSubmenu: layer_menu];

	topBG0_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG0", nil) action:@selector(toggleTopBackground0) keyEquivalent:@""];
	topBG1_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG1", nil) action:@selector(toggleTopBackground1) keyEquivalent:@""];
	topBG2_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG2", nil) action:@selector(toggleTopBackground2) keyEquivalent:@""];
	topBG3_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG3", nil) action:@selector(toggleTopBackground3) keyEquivalent:@""];

	[layer_menu addItem:[NSMenuItem separatorItem]];

	subBG0_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG0", nil) action:@selector(toggleSubBackground0) keyEquivalent:@""];
	subBG1_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG1", nil) action:@selector(toggleSubBackground1) keyEquivalent:@""];
	subBG2_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG2", nil) action:@selector(toggleSubBackground2) keyEquivalent:@""];
	subBG3_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG3", nil) action:@selector(toggleSubBackground3) keyEquivalent:@""];

	[layer_menu release];

	[view addItem:[NSMenuItem separatorItem]];

	screenshot_to_file_item = [view addItemWithTitle:NSLocalizedString(@"Save Screenshot...", nil) action:@selector(saveScreenshot) keyEquivalent:@""];

	[view release];

	//Create the sound menu
	sound_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Sound", nil)];
	[menu setSubmenu:sound_menu forItem:sound_item];

	temp = [sound_menu addItemWithTitle:NSLocalizedString(@"Volume", nil) action:nil keyEquivalent:@""];
	[temp setTarget:[NSApp delegate]];

	NSMenu *volume_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Volume", nil)];
	[sound_menu setSubmenu:volume_menu forItem:temp];

	for(i = 0; i < 10; i++)
	{
		volume_item[i] = [volume_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Volume %d", nil), (i+1)*10] action:@selector(setVolumeFromMenu:) keyEquivalent:@""];
	}

	[volume_menu release];

	[sound_menu addItem:[NSMenuItem separatorItem]];

	mute_item = [sound_menu addItemWithTitle:NSLocalizedString(@"Mute", nil) action:@selector(toggleMute) keyEquivalent:@""];
/*
	[sound_menu addItem:[NSMenuItem separatorItem]];

	temp = [sound_menu addItemWithTitle:@"Record to File..." action:@selector(chooseSoundOutputFile) keyEquivalent: @"r"];

	temp = [sound_menu addItemWithTitle:@"Pause Recording" action:@selector(startRecording) keyEquivalent: @""];

	temp = [sound_menu addItemWithTitle:@"Save Recording" action:@selector(pauseRecording) keyEquivalent: @""];
*/

	[sound_menu release];

	//Create the window menu
/*
	window = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", nil)];
	[menu setSubmenu:window forItem:window_item];

	[window addItemWithTitle:@"Minimize" action:nil keyEquivalent:@""];
	[window addItemWithTitle:@"Zoom" action:nil keyEquivalent:@""];
	[window addItem:[NSMenuItem separatorItem]];
	[window addItemWithTitle:@"Bring All to Front" action:nil keyEquivalent:@""];
	[window addItem:[NSMenuItem separatorItem]];
	[window addItemWithTitle:@"DS Display" action:nil keyEquivalent:@""];
	//[window addItemWithTitle:@"Save State Viewer" action:nil keyEquivalent:@""];
	//[window addItemWithTitle:@"Debugger" action:nil keyEquivalent:@""];
	//[window addItem:[NSMenuItem separatorItem]];
	//[window addItemWithTitle:@"(screenshots)" action:nil keyEquivalent:@""];

	//[window release];
*/

	//Create the help menu
	help = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Help", nil)];
	[menu setSubmenu:help forItem:help_item];

	temp = [help addItemWithTitle:NSLocalizedString(@"Go to Website", nil) action:@selector(launchWebsite) keyEquivalent: @""];
	[temp setTarget:NSApp];

	temp = [help addItemWithTitle:NSLocalizedString(@"Go to Forums", nil) action:@selector(launchForums) keyEquivalent: @""];
	[temp setTarget:NSApp];

	temp = [help addItemWithTitle:NSLocalizedString(@"Submit a Bug Report", nil) action:@selector(bugReport) keyEquivalent: @""];
	[temp setTarget:NSApp];

	[help release];
}

//Main Function--------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	srand(time(NULL));

	autorelease = [[NSAutoreleasePool alloc] init];

	[NSApplication sharedApplication];
	[NSApp setDelegate:[[AppDelegate alloc] init]];

	return NSApplicationMain(argc, (const char**)argv);
}

//gdb stuff--------------------------------------------------------------------------------
//don't know and don't care

void *createThread_gdb(void (*thread_function)(void *data), void *thread_data)
{
	return NULL;
}

void joinThread_gdb(void *thread_handle)
{
}

//Implementations-------------------------------------------------------------------------

@implementation AppDelegate

- (void)pickROM
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];

	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setAllowsMultipleSelection:NO];

	[panel setTitle:NSLocalizedString(@"Open ROM...", nil)];

	if([panel runModalForDirectory:nil file:nil types:[NSArray arrayWithObjects:@"NDS", @"DS.GBA", nil]] == NSOKButton)
	{
		NSString* selected_file = [[panel filenames] lastObject]; //hopefully also the first object

		[self application:NSApp openFile:selected_file];
	}
}

- (void)pickFlash
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
 
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setAllowsMultipleSelection:NO];

	[panel setTitle:NSLocalizedString(@"Set FAT Image File...", nil)];

	if([panel runModalForDirectory:nil file:nil types:[NSArray arrayWithObjects:@"FAT", @"IMG", nil]] == NSOKButton)
	{
		NSString* selected_file = [[panel filenames] lastObject];

		[main_window setFlashFile:selected_file];
	}
}

- (BOOL)application:(NSApplication*)sender openFile:(NSString*)filename
{
	//verify everything
	if(sender != NSApp)return NO;
	if(!filename)return NO;
	if([filename length] == 0)return NO;

	if([main_window loadROM:filename])
	{
		//[main_window execute];
		return YES;
	}

	return NO;
}

- (void)application:(NSApplication*)sender openFiles:(NSArray*)filenames
{
	//verify everything
	if(sender != NSApp)goto fail;
	if(!filenames)goto fail;
	if([filenames count] == 0)goto fail;
	NSString *filename = [filenames lastObject];
	if(!filename)goto fail;
	if([filename length] == 0)goto fail;

	if([main_window loadROM:filename])
	{
		//[main_window execute];
		[sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
		return;
	}

fail:
	[sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)applicationWillFinishLaunching:(NSNotification*)notification
{
	//Set default values for all preferences
	//(this wont override saved preferences as
	//they work in different preference domains)
	setAppDefaults();

	//create the menus
	CreateMenu(self);
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
	//Bring the application to the front
	[NSApp activateIgnoringOtherApps:TRUE];

	//create the video output window (the only window that opens with the app)
	main_window = [[VideoOutputWindow alloc] init];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
	//Ask user if he/she wants to quit
	//ask the user if quitting is acceptable
	if(messageDialogYN(NSLocalizedString(@"DeSmuME Emulator", nil), NSLocalizedString(@"Are you sure you want to quit?", nil)))
		return NSTerminateNow;
	else
		return NSTerminateCancel;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
	[main_window release];
}

@end
