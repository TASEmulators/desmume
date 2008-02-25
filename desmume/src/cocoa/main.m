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
FIXME: Some bug where states get messed up and hitting execute does nothing......
FIXME: .nds.gba extensions don't work in open panels
FIXME: Traveling windows when constantly resizing
*/

//Globals----------------------------------------------------------------------------------------

NSMenu *menu;

NSAutoreleasePool *autorelease;

//Interfaces--------------------------------------------------------------------------------------

@interface AppDelegate : NSObject
{}

//delegate methods
- (BOOL)application:(NSApplication*)sender openFile:(NSString*)filename;
- (void)application:(NSApplication*)sender openFiles:(NSArray*)filenames;
- (void)applicationWillFinishLaunching:(NSNotification*)aNotification;
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender;
- (void)applicationWillTerminate:(NSNotification*)aNotification;
@end

//Global access gui constructs
VideoOutputWindow *main_window;

//Desmume globals---------------------------------------------------------------------------------------

//accessed from other files
volatile desmume_BOOL execute = FALSE;

//
volatile BOOL finished = FALSE;
volatile BOOL paused = TRUE;

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3Dgl
};

//NDS/Emulator control-----------------------------------------------------------------------------------------
//These functions should be called by GUI handling mechanisms to change the state of the emulator or emulation

NintendoDS *NDS;

bool AskForQuit()
{
	BOOL was_paused = paused;

	//pause while we prompt the user
	[NDS pause];

	//ask the user if quitting is acceptable
	if(messageDialogYN(NSLocalizedString(@"DeSmuME Emulator", nil), NSLocalizedString(@"Are you sure you want to quit?", nil)))
		return true;
	else
	{
		if(!was_paused)[NDS execute]; //unpaused if we weren't paused before
		return false;
	}
}

NSEvent *current_event;

//if wait is true then it will sit around waiting for an
//event for a short period of time taking up very little CPU
//(use when game is not running)
void clearEvents(bool wait)
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
	int i;

	NSMenuItem *temp;

	NSMenu* application_menu;
	NSMenu* services_menu;
	NSMenu* file;
	NSMenu* emulation;
	NSMenu* view;
	NSMenu* sound_menu;
	//NSMenu* window;
	NSMenu* help;

	NSMenu* frame_skip_menu;

	menu = [NSApp mainMenu];

	NSMenuItem *file_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *emulation_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *view_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *sound_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	//NSMenuItem *window_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenuItem *help_item = [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];

	//Create the File Menu
	file = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)];
	[file setAutoenablesItems:NO];
	[menu setSubmenu:file forItem:file_item];

	temp = [file addItemWithTitle:NSLocalizedString(@"Open ROM...", nil) action:@selector(pickROM) keyEquivalent:@"o"];
	[temp setTarget:NDS];

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
	[rom_info_item setEnabled:NO];
	[rom_info_item setTarget:NDS];

#ifdef HAVE_LIBZ //internally, save states only work when zlib is there

	[file addItem:[NSMenuItem separatorItem]];

	save_state_as_item = [file addItemWithTitle:NSLocalizedString(@"Save State As...", nil) action:@selector(saveStateAs) keyEquivalent:@""];
	[save_state_as_item setEnabled:NO];
	[save_state_as_item setTarget:NDS];

	load_state_from_item = [file addItemWithTitle:NSLocalizedString(@"Load State From...", nil) action:@selector(loadStateFrom) keyEquivalent:@""];
	[load_state_from_item setEnabled:NO];
	[load_state_from_item setTarget:NDS];

	[file addItem:[NSMenuItem separatorItem]];

	temp = [file addItemWithTitle:NSLocalizedString(@"Save State", nil) action:@selector(saveTo:) keyEquivalent:@""];
	NSMenu *save_state_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Save State", nil)];
	[save_state_menu setAutoenablesItems:NO];
	[temp setSubmenu:save_state_menu];

	temp = [file addItemWithTitle:NSLocalizedString(@"Load State", nil) action:nil keyEquivalent:@""];
	NSMenu *load_state_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Load State", nil)];
	[load_state_menu setAutoenablesItems:NO];
	[temp setSubmenu:load_state_menu];

	for(i = 0; i < SAVE_SLOTS; i++)
	{
		saveSlot_item[i] = [save_state_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Slot %d", nil), i] action:@selector(saveToSlot:) keyEquivalent:@""];
		[saveSlot_item[i] setEnabled:NO];
		[saveSlot_item[i] setTarget:NDS];

		loadSlot_item[i] = [load_state_menu addItemWithTitle:[saveSlot_item[i] title] action:@selector(loadFromSlot:) keyEquivalent:@""];
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

	clear_all_saves_item = [save_state_menu addItemWithTitle:@"Clear Menu" action:@selector(askAndClearStates) keyEquivalent:@""];
	[clear_all_saves_item setEnabled:NO];
	[clear_all_saves_item setTarget:NDS];
*/
/*
	[save_state_menu addItem:[NSMenuItem separatorItem]];

	temp = [save_state_menu addItemWithTitle:NSLocalizedString(@"View States", nil) action:nil keyEquivalent:@""];
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

	close_rom_item = [file addItemWithTitle:NSLocalizedString(@"Close ROM", nil) action:@selector(askAndCloseROM) keyEquivalent:@"w"];
	[close_rom_item setEnabled:NO];
	[close_rom_item setTarget:NDS];

	[file release];

	//Create the Emulation Menu
	emulation = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Emulation", nil)];
	[emulation setAutoenablesItems:NO];
	[menu setSubmenu:emulation forItem:emulation_item];

	execute_item = [emulation addItemWithTitle:NSLocalizedString(@"Execute", nil) action:@selector(execute) keyEquivalent:@""];
	[execute_item setTarget:NDS];
	[execute_item setEnabled:NO];

	pause_item = [emulation addItemWithTitle:NSLocalizedString(@"Pause", nil) action:@selector(pause) keyEquivalent:@""];
	[pause_item setTarget:NDS];
	[pause_item setEnabled:NO];

	reset_item = [emulation addItemWithTitle:NSLocalizedString(@"Reset", nil) action:@selector(reset) keyEquivalent:@""];
	[reset_item setTarget:NDS];
	[reset_item setEnabled:NO];

	[emulation addItem:[NSMenuItem separatorItem]];

	temp = [emulation addItemWithTitle:NSLocalizedString(@"Frame Skip", nil) action:nil keyEquivalent:@""];
	frame_skip_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Frame Skip", nil)];
	[temp setSubmenu:frame_skip_menu];

	frame_skip_auto_item = [frame_skip_menu addItemWithTitle:NSLocalizedString(@"Auto", nil) action:@selector(setFrameSkip:) keyEquivalent:@""];
	[frame_skip_auto_item setTarget:NDS];
	[frame_skip_auto_item setState:NSOnState];
	[frame_skip_auto_item setEnabled:YES];

	[frame_skip_menu addItem:[NSMenuItem separatorItem]];

	frame_skip_item[0] = [frame_skip_menu addItemWithTitle:NSLocalizedString(@"Off", nil) action:@selector(setFrameSkip:) keyEquivalent:@""];
	[frame_skip_item[0] setTarget:NDS];
	[frame_skip_item[0] setEnabled:YES];

	for(i = 1; i < MAX_FRAME_SKIP; i++)
	{
		frame_skip_item[i] = [frame_skip_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Skip %d", nil), i] action:@selector(setFrameSkip:) keyEquivalent:@""];
		[frame_skip_item[i] setTarget:NDS];
		[frame_skip_item[i] setEnabled:YES];
	}

	[frame_skip_menu release];

	[emulation release];

	//Create the screens menu
	view = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
	[menu setSubmenu:view forItem:view_item];

	resize1x = [view addItemWithTitle:NSLocalizedString(@"Size 1x", nil) action:@selector(resizeScreen1x) keyEquivalent:@"1"];
	[resize1x setState:NSOnState];
	[resize1x setTarget:main_window];

	resize2x = [view addItemWithTitle:NSLocalizedString(@"Size 2x", nil) action:@selector(resizeScreen2x) keyEquivalent:@"2"];
	[resize2x setTarget:main_window];

	resize3x = [view addItemWithTitle:NSLocalizedString(@"Size 3x", nil) action:@selector(resizeScreen3x) keyEquivalent:@"3"];
	[resize3x setTarget:main_window];

	resize4x = [view addItemWithTitle:NSLocalizedString(@"Size 4x", nil) action:@selector(resizeScreen4x) keyEquivalent:@"4"];
	[resize4x setTarget:main_window];

	[view addItem:[NSMenuItem separatorItem]];
/*
	[view addItemWithTitle:NSLocalizedString(@"Full Screen", nil) action:nil keyEquivalent:@"f"];

	[view addItem:[NSMenuItem separatorItem]];
*/
	allows_resize_item = [view addItemWithTitle:NSLocalizedString(@"Allow Resize", nil) action:@selector(toggleAllowsResize) keyEquivalent:@""];
	[allows_resize_item setState:NSOnState];
	[allows_resize_item setTarget:main_window];

	constrain_item = [view addItemWithTitle:NSLocalizedString(@"Constrain Proportions", nil) action:@selector(toggleConstrainProportions) keyEquivalent:@""];
	[constrain_item setState:NSOnState];
	[constrain_item setTarget:main_window];

	min_size_item = [view addItemWithTitle:NSLocalizedString(@"No Smaller Than DS", nil) action:@selector(toggleMinSize) keyEquivalent:@""];
	[min_size_item setState:NSOnState];
	[min_size_item setTarget:main_window];

	[view addItem:[NSMenuItem separatorItem]];

	temp = [view addItemWithTitle:NSLocalizedString(@"Rotation", nil) action:nil keyEquivalent:@""];
	NSMenu *rotation_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Rotation", nil)];
	[temp setSubmenu: rotation_menu];

	rotation0_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 0", nil) action:@selector(setRotation0) keyEquivalent:@""];
	[rotation0_item setState:NSOnState];
	[rotation0_item setTarget:main_window];

	rotation90_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 90", nil) action:@selector(setRotation90) keyEquivalent:@""];
	[rotation90_item setState:NSOffState];
	[rotation90_item setTarget:main_window];

	rotation180_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 180", nil) action:@selector(setRotation180) keyEquivalent:@""];
	[rotation180_item setState:NSOffState];
	[rotation180_item setTarget:main_window];

	rotation270_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 270", nil) action:@selector(setRotation270) keyEquivalent:@""];
	[rotation270_item setState:NSOffState];
	[rotation270_item setTarget:main_window];

	[rotation_menu release];

	[view addItem:[NSMenuItem separatorItem]];

	temp = [view addItemWithTitle:NSLocalizedString(@"Layers", nil) action:nil keyEquivalent:@""];
	NSMenu *layer_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Layers", nil)];
	[temp setSubmenu: layer_menu];

	topBG0_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG0", nil) action:@selector(toggleTopBackground0) keyEquivalent:@""];
	[topBG0_item setState:NSOnState];
	[topBG0_item setTarget:main_window];

	topBG1_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG1", nil) action:@selector(toggleTopBackground1) keyEquivalent:@""];
	[topBG1_item setState:NSOnState];
	[topBG1_item setTarget:main_window];

	topBG2_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG2", nil) action:@selector(toggleTopBackground2) keyEquivalent:@""];
	[topBG2_item setState:NSOnState];
	[topBG2_item setTarget:main_window];

	topBG3_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Top BG3", nil) action:@selector(toggleTopBackground3) keyEquivalent:@""];
	[topBG3_item setState:NSOnState];
	[topBG3_item setTarget:main_window];

	[layer_menu addItem:[NSMenuItem separatorItem]];

	subBG0_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG0", nil) action:@selector(toggleSubBackground0) keyEquivalent:@""];
	[subBG0_item setState:NSOnState];
	[subBG0_item setTarget:main_window];

	subBG1_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG1", nil) action:@selector(toggleSubBackground1) keyEquivalent:@""];
	[subBG1_item setState:NSOnState];
	[subBG1_item setTarget:main_window];

	subBG2_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG2", nil) action:@selector(toggleSubBackground2) keyEquivalent:@""];
	[subBG2_item setState:NSOnState];
	[subBG2_item setTarget:main_window];

	subBG3_item = [layer_menu addItemWithTitle:NSLocalizedString(@"Sub BG3", nil) action:@selector(toggleSubBackground3) keyEquivalent:@""];
	[subBG3_item setState:NSOnState];
	[subBG3_item setTarget:main_window];

	[layer_menu release];

	[view addItem:[NSMenuItem separatorItem]];

	allows_resize_item = [view addItemWithTitle:NSLocalizedString(@"Screenshot to File", nil) action:@selector(screenShotToFile) keyEquivalent:@""];
	[allows_resize_item setTarget:main_window];

	allows_resize_item = [view addItemWithTitle:NSLocalizedString(@"Screenshot to Window", nil) action:@selector(screenShotToWindow) keyEquivalent:@""];
	[allows_resize_item setTarget:main_window];

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
		volume_item[i] = [volume_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Volume %d", nil), (i+1)*10] action:@selector(setVolume:) keyEquivalent:@""];
		[volume_item[i] setTarget:NDS];
		if(i == 9)
			[volume_item[i] setState:NSOnState]; //check 100% volume since it's defaults
	}

	[volume_menu release];

	[sound_menu addItem:[NSMenuItem separatorItem]];

	mute_item = [sound_menu addItemWithTitle:NSLocalizedString(@"Mute", nil) action:@selector(toggleMuting) keyEquivalent:@""];
	[mute_item setTarget:NDS];
/*
	[sound_menu addItem:[NSMenuItem separatorItem]];

	temp = [sound_menu addItemWithTitle:@"Record to File..." action:@selector(chooseSoundOutputFile) keyEquivalent: @"r"];
	[temp setTarget:NDS];

	temp = [sound_menu addItemWithTitle:@"Pause Recording" action:@selector(startRecording) keyEquivalent: @""];
	[temp setTarget:NDS];

	temp = [sound_menu addItemWithTitle:@"Save Recording" action:@selector(pauseRecording) keyEquivalent: @""];
	[temp setTarget:NDS];
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

	return NSApplicationMain(argc,  (const char **) argv);
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

- (BOOL)application:(NSApplication*)sender openFile:(NSString*)filename
{
	//verify everything
	if(sender != NSApp)return NO;
	if(!filename)return NO;
	if([filename length] == 0)return NO;

	if([NDS loadROM:filename])
		return YES;

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

	if([NDS loadROM:filename])
	{
		[sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
		return;
	}

fail:
	[sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
	//Set default values for all preferences
	//(this wont override saved preferences as
	//they work in different preference domains)
	setAppDefaults();

	//Bring the application to the front
	[NSApp activateIgnoringOtherApps:TRUE];

	//create the global ds object
	NDS = [[NintendoDS alloc] init];

	//create the video output window (the only window that opens with the app)
	main_window = [[VideoOutputWindow alloc] init];

	//create the menus
	CreateMenu();
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	//init opengl 3d ness
	NDS_3D_SetDriver (GPU3D_OPENGL);
	if(!gpu3D->NDS_3D_Init())
		messageDialog(NSLocalizedString(@"Error", nil), @"Unable to initialize OpenGL components");

#ifdef MULTITHREADED

	//Breakoff a thread to run
	[NSApplication detachDrawingThread:@selector(run) toTarget:NDS withObject:nil];

#else
	[NDS run];

	//it seems we need an event before it will quit
	//(probably applicationWillTersminatehas something to do with calling run from applicationDidFinishLaunching?)
	[NSEvent startPeriodicEventsAfterDelay:0 withPeriod:1];

#endif
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	//Ask user if he/she wants to quit
	if(AskForQuit())
		return NSTerminateNow;
	else
		return NSTerminateCancel;
}

- (void)applicationWillTerminate:(NSNotification*)aNotification
{
	//Quit
	[NDS destroy];
	finished = true;

	//Free memory
	[main_window release];
	[NSApp setServicesMenu:nil];
	[NSApp setMainMenu:nil];
	[menu release];
}

@end
