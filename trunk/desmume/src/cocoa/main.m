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

//DeSmuME Cocoa includes
#import "globals.h"
#import "main_window.h"
#import "nds_control.h"

//DeSmuME general includes
#define OBJ_C
#include "../SPU.h"
#include "../render3D.h"
#include "../GPU.h"
#include "../Windows/OGLRender.h"
#include "../NDSSystem.h"
#undef BOOL

/*
FIXME: Apple+Q during open dialog = badness
FIXME: Live resize needs to work without pausing emulator
FIXME: Hardware acceleration for openglrender.c ??
FIXME: When cross-platform (core) components end emulation due to error - pause should be called (set the menu checkmark)
FIXME: Multi-threaded version needs to do some sleep()-ing when not running game
FIXME: Some bug where states get messed up and hitting execute does nothing......
FIXME: single threaded version requires an additional event to quit (after you tell the program to exit you gotta click or press something)
FIXME: .nds.gba extensions don't work in open panels
FIXME: Traveling windows when constantly resizing with hotkey
*/

//Globals----------------------------------------------------------------------------------------

NSMenu *menu;

//Interfaces--------------------------------------------------------------------------------------

@interface NSApplication(custom)

//this prototype was removed from the mac os headers
- (void)setAppleMenu:(NSMenu*)menu;

//here we extend NSApplication
- (void)about;
- (void)preferences;

@end

@interface NSApplication(delegate)

//delegate methods
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames;
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;

@end

//Global access gui constructs
VideoOutputWindow *main_window;

//Desmume globals---------------------------------------------------------------------------------------

//accessed from other files
volatile desmume_BOOL execute = FALSE;

//
volatile BOOL finished = FALSE;
volatile BOOL paused = TRUE;

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
//&SNDFile,
//&SNDDIRECTX,
NULL
};

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3Dgl
};

//NDS/Emulator control-----------------------------------------------------------------------------------------
//These functions should be called by GUI handling mechanisms to change the state of the emulator or emulation

NintendoDS *NDS;

void Quit()
{
	//stop emulation if it's going
	[NDS pause];

	//
	finished = true;

	//
	[NSApp stop:NSApp];
}

void AskForQuit()
{
	BOOL was_paused = paused;

	//pause while we prompt the user
	[NDS pause];

	//ask the user if quitting is acceptable
	if(messageDialogYN(localizedString(@"DeSmuME Emulator", nil), localizedString(@"Are you sure you want to quit?", nil)))
		Quit();
	else
		if(!was_paused)[NDS execute]; //unpaused if we weren't paused before
}

NSEvent *current_event;

//if wait is true then it will sit around waiting for an
//event for a short period of time taking up very little CPU
//(use when game is not running)
inline void clearEvents(bool wait)
{
	//wait for the next event
	while((current_event = [NSApp nextEventMatchingMask:0 untilDate:wait?[NSDate dateWithTimeIntervalSinceNow:.2]:nil inMode:NSDefaultRunLoopMode dequeue:YES]))
		[NSApp sendEvent: current_event];
}

///////////////////////////////

//This generates and sets the entire menubar (and sets defaults)
//call after objects (ie NDS main_window) are inited
void CreateMenu()
{
	NSMenuItem *temp;

	NSMenu* application_menu;
	NSMenu* services_menu;
	NSMenu* file;
	NSMenu* emulation;
	NSMenu* view;
	//NSMenu* window;
	NSMenu* help;

	NSMenu* frame_skip_menu;

	SEL action = nil;//@selector(method:);

	//Create the outermost menu that contains all the others
	menu = [[NSMenu alloc] initWithTitle:@"DeSmuME"];

	//Create the "DeSmuME" (application) menu
	application_menu = [[NSMenu alloc] initWithTitle:@"DeSmuME"];

	//Create the sevices menu
	services_menu = [[NSMenu alloc] initWithTitle:@"Services"];
	[NSApp setServicesMenu:services_menu];

	//Add the about menu
	temp = [application_menu addItemWithTitle:@"About DeSmuME" action:@selector(about) keyEquivalent:@""];
	[temp setTarget:NSApp];

	//
	[application_menu addItem: [NSMenuItem separatorItem]];

	//Add the preferences menu
	//temp = [application_menu addItemWithTitle:@"Preferences..." action:@selector(preferences) keyEquivalent:@","];
	//[temp setTarget:NSApp];

	//
	[application_menu addItem: [NSMenuItem separatorItem]];

	//add the services menu to the application menu (after preferences)
	temp = [application_menu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
	[temp setSubmenu:services_menu];

	//
	[application_menu addItem: [NSMenuItem separatorItem]];

	//standard hide option
	temp = [application_menu addItemWithTitle:@"Hide DeSmuME" action:@selector(hide:) keyEquivalent:@""];
	[temp setTarget:NSApp];

	//standard hide others option
	temp = [application_menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@""];
	[temp setTarget:NSApp];

	//standard show all option
	temp = [application_menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
	[temp setTarget:NSApp];

	//
	[application_menu addItem: [NSMenuItem separatorItem]];

	//standard quit option
	temp = [application_menu addItemWithTitle:@"Quit DeSmuME" action:@selector(terminate:) keyEquivalent:@"q"];
	[temp setTarget: NSApp];

	//finally call setAppleMenu to have all our stuff working
	[NSApp setAppleMenu:application_menu];

	temp = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	[temp setSubmenu:application_menu];
	[temp release];

	NSMenuItem *file_item = [menu addItemWithTitle:@"File" action:action keyEquivalent:@""];
	NSMenuItem *emulation_item = [menu addItemWithTitle:@"Emulation" action:action keyEquivalent:@""];
	NSMenuItem *view_item = [menu addItemWithTitle:@"View" action:action keyEquivalent:@""];
	//NSMenuItem *window_item = [menu addItemWithTitle:@"Window" action:action keyEquivalent:@""];
	NSMenuItem *help_item = [menu addItemWithTitle:@"Help" action:action keyEquivalent:@""];

	//Create the File Menu
	file = [[NSMenu alloc] initWithTitle:@"File"];
	[file setAutoenablesItems:NO];
	[menu setSubmenu:file forItem:file_item];

	temp = [file addItemWithTitle:@"Open ROM..." action:@selector(pickROM) keyEquivalent:@"o"];
	[temp setTarget:NDS];

	//recent items menu
	temp = [file addItemWithTitle:@"Open Recent" action:nil keyEquivalent:@""];

	NSMenu *recent_menu = [[NSMenu alloc] initWithTitle:@""];
	[temp setSubmenu:recent_menu];

	[recent_menu addItemWithTitle:@"Blar" action:nil keyEquivalent:@""];

	[file addItem:[NSMenuItem separatorItem]];

	rom_info_item = [file addItemWithTitle:@"ROM Info..." action:@selector(showRomInfo) keyEquivalent:@""];
	[rom_info_item setEnabled:NO];
	[rom_info_item setTarget:NDS];

#ifdef HAVE_LIBZ //internally, save states only work when zlib is there

	[file addItem:[NSMenuItem separatorItem]];

	save_state_as_item = [file addItemWithTitle:@"Save State As..." action:@selector(saveStateAs) keyEquivalent:@""];
	[save_state_as_item setEnabled:NO];
	[save_state_as_item setTarget:NDS];

	load_state_from_item = [file addItemWithTitle:@"Load State From..." action:@selector(loadStateFrom) keyEquivalent:@""];
	[load_state_from_item setEnabled:NO];
	[load_state_from_item setTarget:NDS];

	[file addItem:[NSMenuItem separatorItem]];

	temp = [file addItemWithTitle:@"Save State" action:@selector(saveTo:) keyEquivalent:@""];
	NSMenu *save_state_menu = [[NSMenu alloc] initWithTitle:@"Save State"];
	[save_state_menu setAutoenablesItems:NO];
	[temp setSubmenu:save_state_menu];

	temp = [file addItemWithTitle:@"Load State" action:nil keyEquivalent:@""];
	NSMenu *load_state_menu = [[NSMenu alloc] initWithTitle:@"Load State"];
	[load_state_menu setAutoenablesItems:NO];
	[temp setSubmenu:load_state_menu];

	int i;
	for(i = 0; i < SAVE_SLOTS; i++)
	{
		saveSlot_item[i] = [save_state_menu addItemWithTitle:@"Slot %d" withInt:i action:@selector(saveToSlot:) keyEquivalent:@""];
		[saveSlot_item[i] setEnabled:NO];
		[saveSlot_item[i] setTarget:NDS];

		loadSlot_item[i] = [load_state_menu addItemWithTitle:@"Slot %d" withInt:i action:@selector(loadFromSlot:) keyEquivalent:@""];
		[loadSlot_item[i] setEnabled:NO];
		[loadSlot_item[i] setTarget:NDS];
	}

/* To be implemented when saves.h provides
a way to get the time of a save that's not string/human formatted...

	[save_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [save_state_menu addItemWithTitle:@"Save Over Oldest" action:nil keyEquivalent:@""];
	[temp setEnabled:NO];
	[temp setTarget:NDS];

	temp = [save_state_menu addItemWithTitle:@"Save Over Latest" action:nil keyEquivalent:@""];
	[temp setEnabled:NO];
	[temp setTarget:NDS];
*/
/* to be implemented after saves.h provides a way to delete saves...

	[save_state_menu addItem:[NSMenuItem separatorItem]];

	clear_all_saves_item = [save_state_menu addItemWithTitle:@"Clear All" action:@selector(askAndClearStates) keyEquivalent:@""];
	[clear_all_saves_item setEnabled:NO];
	[clear_all_saves_item setTarget:NDS];
*/
/*
	[save_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [save_state_menu addItemWithTitle:@"View States" action:nil keyEquivalent:@""];
	[temp setEnabled:NO];
	[temp setTarget:NDS];
*/
/* To be implemented when saves.h provides
a way to get the time of a save that's not a string / human formatted...

	[load_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [load_state_menu addItemWithTitle:@"Load Oldest" action:nil keyEquivalent:@""];
	[temp setEnabled:NO];
	[temp setTarget:NDS];

	temp = [load_state_menu addItemWithTitle:@"Load Latest" action:nil keyEquivalent:@""];
	[temp setEnabled:NO];
	[temp setTarget:NDS];
*/
/*
	[load_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [load_state_menu addItemWithTitle:@"View States" action:nil keyEquivalent:@""];
	[temp setEnabled:NO];
	[temp setTarget:NDS];
*/
#endif

	[file addItem:[NSMenuItem separatorItem]];

	close_rom_item = [file addItemWithTitle:@"Close ROM" action:@selector(askAndCloseROM) keyEquivalent:@"w"];
	[close_rom_item setEnabled:NO];
	[close_rom_item setTarget:NDS];

	//Create the Emulation Menu
	emulation = [[NSMenu alloc] initWithTitle:localizedString(@"Emulation", nil)];
	[emulation setAutoenablesItems:NO];
	[menu setSubmenu:emulation forItem:emulation_item];

	execute_item = [emulation addItemWithTitle:@"Execute" action:@selector(execute) keyEquivalent:@""];
	[execute_item setTarget:NDS];
	[execute_item setEnabled:NO];

	pause_item = [emulation addItemWithTitle:@"Pause" action:@selector(pause) keyEquivalent:@""];
	[pause_item setTarget:NDS];
	[pause_item setEnabled:NO];

	reset_item = [emulation addItemWithTitle:@"Reset" action:@selector(reset) keyEquivalent:@""];
	[reset_item setTarget:NDS];
	[reset_item setEnabled:NO];

	[emulation addItem:[NSMenuItem separatorItem]];

	temp = [emulation addItemWithTitle:@"Frame Skip" action:nil keyEquivalent:@""];
	frame_skip_menu = [[NSMenu alloc] initWithTitle:localizedString(@"Frame Skip", nil)];
	[temp setSubmenu:frame_skip_menu];

	frame_skip_auto_item = [frame_skip_menu addItemWithTitle:@"Auto" action:@selector(setFrameSkip:) keyEquivalent:@""];
	[frame_skip_auto_item setTarget:NDS];
	[frame_skip_auto_item setState:NSOnState];
	[frame_skip_auto_item setEnabled:YES];

	[frame_skip_menu addItem:[NSMenuItem separatorItem]];

	frame_skip_item[0] = [frame_skip_menu addItemWithTitle:@"Off" action:@selector(setFrameSkip:) keyEquivalent:@""];
	[frame_skip_item[0] setTarget:NDS];
	[frame_skip_item[0] setEnabled:YES];

	int i2;
	for(i2 = 1; i2 < MAX_FRAME_SKIP; i2++)
	{
		frame_skip_item[i2] = [frame_skip_menu addItemWithTitle:@"%d" withInt:i2 action:@selector(setFrameSkip:) keyEquivalent:@""];
		[frame_skip_item[i2] setTarget:NDS];
		[frame_skip_item[i2] setEnabled:YES];
	}

	//Create the screens menu
	view = [[NSMenu alloc] initWithTitle:localizedString(@"View", nil)];
	[menu setSubmenu:view forItem:view_item];

	resize1x = [view addItemWithTitle:@"Size 1x" action:@selector(resizeScreen1x) keyEquivalent:@"1"];
	[resize1x setState:NSOnState];
	[resize1x setTarget:main_window];

	resize2x = [view addItemWithTitle:@"Size 2x" action:@selector(resizeScreen2x) keyEquivalent:@"2"];
	[resize2x setTarget:main_window];

	resize3x = [view addItemWithTitle:@"Size 3x" action:@selector(resizeScreen3x) keyEquivalent:@"3"];
	[resize3x setTarget:main_window];

	resize4x = [view addItemWithTitle:@"Size 4x" action:@selector(resizeScreen4x) keyEquivalent:@"4"];
	[resize4x setTarget:main_window];
/*
	[view addItem:[NSMenuItem separatorItem]];

	[view addItemWithTitle:@"Full Screen" action:nil keyEquivalent:@"f"];
*/
	[view addItem:[NSMenuItem separatorItem]];

	allows_resize_item = [view addItemWithTitle:@"Allow Resize" action:@selector(toggleAllowsResize) keyEquivalent:@""];
	[allows_resize_item setState:NSOnState];
	[allows_resize_item setTarget:main_window];

	constrain_item = [view addItemWithTitle:@"Constrain Proportions" action:@selector(toggleConstrainProportions) keyEquivalent:@""];
	[constrain_item setState:NSOnState];
	[constrain_item setTarget:main_window];

	min_size_item = [view addItemWithTitle: @"No smaller than DS" action:@selector(toggleMinSize) keyEquivalent:@""];
	[min_size_item setState:NSOnState];
	[min_size_item setTarget:main_window];

	[view addItem:[NSMenuItem separatorItem]];

	temp = [view addItemWithTitle: @"Rotation" action:nil keyEquivalent:@""];

	NSMenu *rotation_menu = [[NSMenu alloc] initWithTitle:@"Rotation"];
	[temp setSubmenu: rotation_menu];

	rotation0_item = [rotation_menu addItemWithTitle:@"0" action:@selector(setRotation0) keyEquivalent:@""];
	[rotation0_item setState:NSOnState];
	[rotation0_item setTarget:main_window];

	rotation90_item = [rotation_menu addItemWithTitle:@"90" action:@selector(setRotation90) keyEquivalent:@""];
	[rotation90_item setState:NSOffState];
	[rotation90_item setTarget:main_window];

	rotation180_item = [rotation_menu addItemWithTitle:@"180" action:@selector(setRotation180) keyEquivalent:@""];
	[rotation180_item setState:NSOffState];
	[rotation180_item setTarget:main_window];

	rotation270_item = [rotation_menu addItemWithTitle:@"270" action:@selector(setRotation270) keyEquivalent:@""];
	[rotation270_item setState:NSOffState];
	[rotation270_item setTarget:main_window];

	[view addItem:[NSMenuItem separatorItem]];

	temp = [view addItemWithTitle:@"Layers" action:nil keyEquivalent:@""];
	NSMenu *layer_menu = [[NSMenu alloc] initWithTitle:@"Layers"];
	[temp setSubmenu: layer_menu];

	topBG0_item = [layer_menu addItemWithTitle:@"Top BG0" action:@selector(toggleTopBackground0) keyEquivalent:@""];
	[topBG0_item setState:NSOnState];
	[topBG0_item setTarget:main_window];

	topBG1_item = [layer_menu addItemWithTitle:@"Top BG1" action:@selector(toggleTopBackground1) keyEquivalent:@""];
	[topBG1_item setState:NSOnState];
	[topBG1_item setTarget:main_window];

	topBG2_item = [layer_menu addItemWithTitle:@"Top BG2" action:@selector(toggleTopBackground2) keyEquivalent:@""];
	[topBG2_item setState:NSOnState];
	[topBG2_item setTarget:main_window];

	topBG3_item = [layer_menu addItemWithTitle:@"Top BG3" action:@selector(toggleTopBackground3) keyEquivalent:@""];
	[topBG3_item setState:NSOnState];
	[topBG3_item setTarget:main_window];

	[layer_menu addItem:[NSMenuItem separatorItem]];

	subBG0_item = [layer_menu addItemWithTitle:@"Sub BG0" action:@selector(toggleSubBackground0) keyEquivalent:@""];
	[subBG0_item setState:NSOnState];
	[subBG0_item setTarget:main_window];

	subBG1_item = [layer_menu addItemWithTitle:@"Sub BG1" action:@selector(toggleSubBackground1) keyEquivalent:@""];
	[subBG1_item setState:NSOnState];
	[subBG1_item setTarget:main_window];

	subBG2_item = [layer_menu addItemWithTitle:@"Sub BG2" action:@selector(toggleSubBackground2) keyEquivalent:@""];
	[subBG2_item setState:NSOnState];
	[subBG2_item setTarget:main_window];

	subBG3_item = [layer_menu addItemWithTitle:@"Sub BG3" action:@selector(toggleSubBackground3) keyEquivalent:@""];
	[subBG3_item setState:NSOnState];
	[subBG3_item setTarget:main_window];

	//Create the window menu
/*
	window = [[NSMenu alloc] initWithTitle:localizedString(@"Window", nil)];
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
*/

	//Create the help menu
	help = [[NSMenu alloc] initWithTitle:localizedString(@"Help", nil)];
	[menu setSubmenu:help forItem:help_item];

	[help addItemWithTitle:@"Website" action:nil keyEquivalent: @""];
	[help addItemWithTitle:@"Forums" action:nil keyEquivalent: @""];
	[help addItemWithTitle:@"Submit A Bug Report" action:nil keyEquivalent: @""];

	[NSApp setMainMenu: menu];

	[menu update];
}

//Main Function--------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{

	srand(time(NULL));

	//Application Init Stuff----------------------------------------------------------------------

	//Get the application instance (global variable called NSApp)
	[NSApplication sharedApplication];

	//Set the application delegate
	[NSApp setDelegate:NSApp];

	[NSApp run];

	//Program Exit ---------------------------------------------------------------------------------------

	return 0;
}

//gdb stuff--------------------------------------------------------------------------------
//don't know and don't care

void *
createThread_gdb( void (*thread_function)( void *data),
				 void *thread_data) {
return NULL;
}

void
joinThread_gdb( void *thread_handle) {
}

//Implementations-------------------------------------------------------------------------

@implementation NSApplication(delegate)

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	if([NDS loadROM:filename])
		return YES;

	return NO;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
	//messageDialog(@"Openfiles",@"");
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{

	//More app Init ------------------------------------------------------------------------------

	//Bring the application to the front
	[NSApp activateIgnoringOtherApps:TRUE];

	//create the global ds object
	NDS = [[NintendoDS alloc] init];

	//create the video output window (the only window that opens with the app)
	main_window = [[VideoOutputWindow alloc] init];

	//start with a blank screen
	[main_window clearScreen];

	//create the menus
	CreateMenu();

	//init opengl 3d ness
	NDS_3D_SetDriver (GPU3D_OPENGL);
	if(!gpu3D->NDS_3D_Init())
		messageDialog(@"Error", @"Unable to initialize OpenGL components");

	//[NDS LoadROM:@"/Users/gecko/nds/0004 - Feel the Magic - XY-XX (U) (Trashman).nds"];
	//^ above line breaks everything!?

	//Main program loop ----------------------------------------------------------------------------

#ifdef MULTITHREADED

	//Breakoff a thread to run
	[NSApplication detachDrawingThread:@selector(run) toTarget:NDS withObject:nil];

#else

	[NDS run];

	//it seems we need an event before it will quit
	//(probably has something to do with calling run from applicationDidFinishLaunching?)
	[NSEvent startPeriodicEventsAfterDelay:0 withPeriod:1];

#endif

}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	//Ask user if he wants to quit
	AskForQuit();

	//AskForQuit takes care of quitting, dont need cocoa to do it for us
	return NSTerminateCancel;
}
@end
