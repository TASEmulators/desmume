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
By Jeff Bland
Based on work by yopyop and the DeSmuME team!
Mac related questions can go to osx@desmume.org
*/

#import "main_window.h"
#import "preferences.h"

#ifdef GDB_STUB
#include <pthread.h>
#endif

/*
FIXME: .nds.gba support?
*/

//Globals----------------------------------------------------------------------------------------

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

extern NSMenuItem *frame_skip_auto_item;
extern NSMenuItem *frame_skip_item[];

extern NSMenuItem *save_type_item[];
extern NSString *save_types[];

extern NSMenuItem *speed_limit_25_item;
extern NSMenuItem *speed_limit_50_item;
extern NSMenuItem *speed_limit_75_item;
extern NSMenuItem *speed_limit_100_item;
extern NSMenuItem *speed_limit_200_item;
extern NSMenuItem *speed_limit_none_item;
extern NSMenuItem *speed_limit_custom_item;

extern NSMenuItem *save_state_as_item;
extern NSMenuItem *load_state_from_item;

//sound (defined/managed in nds_control.m)
extern NSMenuItem *volume_item[];
extern NSMenuItem *mute_item;

#define SAVE_SLOTS 10 //this should never be more than NB_SAVES in saves.h
extern NSMenuItem *saveSlot_item[];
extern NSMenuItem *loadSlot_item[];

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
	//Grab the main menu
	NSMenu *main_menu = [NSApp mainMenu];
	if(main_menu == nil)return;
	
	//
	int i;
	NSMenuItem *temp;

	//File Menu
	
	temp = [main_menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenu *file_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)];
	if(file_menu != nil)
	{
		[main_menu setSubmenu:file_menu forItem:temp];

		[[file_menu addItemWithTitle:NSLocalizedString(@"Open ROM...", nil) action:@selector(pickROM) keyEquivalent:@"o"] setTarget:delegate];

		//Recent items menu
		// Thanks to Jeff Johnson and the Lap Cat Software Blog for their information on the Open Recent menu in Cocoa
		// http://lapcatsoftware.com/blog/

		temp = [file_menu addItemWithTitle:NSLocalizedString(@"Open Recent", nil) action:nil keyEquivalent:@""];

		NSMenu *recent_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Open Recent", nil)];

		if(recent_menu != nil)
		{
			[recent_menu performSelector:@selector(_setMenuName:) withObject:@"NSRecentDocumentsMenu"];
			[temp setSubmenu:recent_menu];

			[[recent_menu addItemWithTitle:@"Clear Menu" action:@selector(clearRecentDocuments:) keyEquivalent:@""] setTarget:[NSDocumentController sharedDocumentController]];

			[recent_menu release];
		}
		
		[file_menu addItem:[NSMenuItem separatorItem]];

		rom_info_item = [file_menu addItemWithTitle:NSLocalizedString(@"ROM Info...", nil) action:@selector(showRomInfo) keyEquivalent:@""];

#ifdef HAVE_LIBZ //internally, save states only work when zlib is there
		
		[file_menu addItem:[NSMenuItem separatorItem]];

		[save_state_as_item = [file_menu addItemWithTitle:NSLocalizedString(@"Save State As...", nil) action:@selector(saveStateAs) keyEquivalent:@""] setEnabled:NO];

		[load_state_from_item = [file_menu addItemWithTitle:NSLocalizedString(@"Load State From...", nil) action:@selector(loadStateFrom) keyEquivalent:@""] setEnabled:NO];

		[file_menu addItem:[NSMenuItem separatorItem]];

		//Save state menu

		[save_state_as_item setEnabled:NO];
		
		temp = [file_menu addItemWithTitle:NSLocalizedString(@"Save State", nil) action:nil keyEquivalent:@""];

		NSMenu *save_state_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Save State", nil)];
		if(save_state_menu != nil)
		{
			[temp setSubmenu:save_state_menu];

			for(i = 0; i < SAVE_SLOTS; i++)
			{
				saveSlot_item[i] = [save_state_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Slot %d", nil), i+1] action:@selector(saveToSlot:) keyEquivalent:[NSString stringWithFormat:@"%d",  i<9?i+1:0]];
				[saveSlot_item[i] setKeyEquivalentModifierMask:NSShiftKeyMask];
			}
			[save_state_menu release];
		}

		//Load state menu

		temp = [file_menu addItemWithTitle:NSLocalizedString(@"Load State", nil) action:nil keyEquivalent:@""];
		NSMenu *load_state_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Load State", nil)];
		if(load_state_menu != nil)
		{
			[temp setSubmenu:load_state_menu];

			for(i = 0; i < SAVE_SLOTS; i++)
			{
				loadSlot_item[i] = [load_state_menu addItemWithTitle:[saveSlot_item[i] title] action:@selector(loadFromSlot:) keyEquivalent:[NSString stringWithFormat:@"%d", i<9?i+1:0]];
				[loadSlot_item[i] setKeyEquivalentModifierMask:0];
			}
			
			[load_state_menu release];
		}

#endif

		[file_menu addItem:[NSMenuItem separatorItem]];

		close_rom_item = [file_menu addItemWithTitle:NSLocalizedString(@"Close ROM", nil) action:@selector(askAndCloseROM) keyEquivalent:@"w"];

		[file_menu release];
	}

	//Emulation menu
	
	temp = [main_menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenu *emulation_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Emulation", nil)];
	if(emulation_menu != nil)
	{
		[main_menu setSubmenu:emulation_menu forItem:temp];

		execute_item = [emulation_menu addItemWithTitle:NSLocalizedString(@"Execute", nil) action:@selector(execute) keyEquivalent:@"e"];
		pause_item = [emulation_menu addItemWithTitle:NSLocalizedString(@"Pause", nil) action:@selector(pause) keyEquivalent:@"p"];
		reset_item = [emulation_menu addItemWithTitle:NSLocalizedString(@"Reset", nil) action:@selector(reset) keyEquivalent:@"r"];

		[emulation_menu addItem:[NSMenuItem separatorItem]];

		//Frake skip menu
		
		temp = [emulation_menu addItemWithTitle:NSLocalizedString(@"Frame Skip", nil) action:nil keyEquivalent:@""];
		
		NSMenu *frame_skip_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Frame Skip", nil)];
		if(frame_skip_menu != nil)
		{
			[temp setSubmenu:frame_skip_menu];

			frame_skip_auto_item = [frame_skip_menu addItemWithTitle:NSLocalizedString(@"Auto", nil) action:@selector(setFrameSkipFromMenuItem:) keyEquivalent:@""];

			[frame_skip_menu addItem:[NSMenuItem separatorItem]];

			frame_skip_item[0] = [frame_skip_menu addItemWithTitle:NSLocalizedString(@"Off", nil) action:@selector(setFrameSkipFromMenuItem:) keyEquivalent:@""];

			for(i = 1; i < MAX_FRAME_SKIP; i++)
			{
				frame_skip_item[i] = [frame_skip_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Skip %d", nil), i] action:@selector(setFrameSkipFromMenuItem:) keyEquivalent:@""];
			}

			[frame_skip_menu release];
		}

		//Speed limit menu
		
		temp = [emulation_menu addItemWithTitle:NSLocalizedString(@"Speed Limit", nil) action:nil keyEquivalent:@""];
		
		NSMenu *speed_limit_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Speed Limit", nil)];
		if(speed_limit_menu != nil)
		{
			[temp setSubmenu:speed_limit_menu];

			speed_limit_25_item = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"25% Speed Limit", nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];
			speed_limit_50_item = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"50% Speed Limit", nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];
			speed_limit_75_item = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"75% Speed Limit", nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];
			
			[speed_limit_menu addItem:[NSMenuItem separatorItem]];

			speed_limit_100_item = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"100% Speed Limit", nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];

			[speed_limit_menu addItem:[NSMenuItem separatorItem]];

			speed_limit_200_item  = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"200% Speed Limit", nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];
			speed_limit_none_item = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"No Speed Limit"  , nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];

			[speed_limit_menu addItem:[NSMenuItem separatorItem]];

			speed_limit_custom_item = [speed_limit_menu addItemWithTitle:NSLocalizedString(@"Custom Speed Limit", nil) action:@selector(setSpeedLimitFromMenuItem:) keyEquivalent:@""];

			[speed_limit_menu release];
		}
		
		// Backup media type
		temp = [emulation_menu addItemWithTitle:NSLocalizedString(@"Backup Media Type", nil) action:nil keyEquivalent:@""];
		
		NSMenu *save_type_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Backup Media Type", nil)];
		if(save_type_menu != nil)
		{
			[temp setSubmenu:save_type_menu];
			
			// Add autodetect apart from the others
			save_type_item[0] = [save_type_menu addItemWithTitle:save_types[0] action:@selector(setSaveTypeFromMenuItem:) keyEquivalent:@""];
			[save_type_menu addItem:[NSMenuItem separatorItem]];
			
			// Add the rest
			for(i = 1; i < MAX_SAVE_TYPE; i++)
			{
				save_type_item[i] = [save_type_menu addItemWithTitle:save_types[i] action:@selector(setSaveTypeFromMenuItem:) keyEquivalent:@""];
			}
			
			[save_type_menu release];
		}
		

		[emulation_menu addItem:[NSMenuItem separatorItem]];
	
		[emulation_menu addItemWithTitle:NSLocalizedString(@"Set FAT Image File...", nil) action:@selector(pickFlash) keyEquivalent:@""];
	
		[emulation_menu release];
	}

	//View menu

	temp = [main_menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenu *view_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
	if(view_menu)
	{
		[main_menu setSubmenu:view_menu forItem:temp];

		resize1x = [view_menu addItemWithTitle:NSLocalizedString(@"Size 1x", nil) action:@selector(resizeScreen1x) keyEquivalent:@"1"];
		resize2x = [view_menu addItemWithTitle:NSLocalizedString(@"Size 2x", nil) action:@selector(resizeScreen2x) keyEquivalent:@"2"];
		resize3x = [view_menu addItemWithTitle:NSLocalizedString(@"Size 3x", nil) action:@selector(resizeScreen3x) keyEquivalent:@"3"];
		resize4x = [view_menu addItemWithTitle:NSLocalizedString(@"Size 4x", nil) action:@selector(resizeScreen4x) keyEquivalent:@"4"];

		[view_menu addItem:[NSMenuItem separatorItem]];

		constrain_item         = [view_menu addItemWithTitle:NSLocalizedString(@"Constrain Proportions", nil) action:@selector(toggleConstrainProportions) keyEquivalent:@""];
		min_size_item          = [view_menu addItemWithTitle:NSLocalizedString(@"No Smaller Than DS", nil) action:@selector(toggleMinSize) keyEquivalent:@""];
		toggle_status_bar_item = [view_menu addItemWithTitle:NSLocalizedString(@"Show Status Bar", nil) action:@selector(toggleStatusBar) keyEquivalent:@"/"];

		[view_menu addItem:[NSMenuItem separatorItem]];

		//Rotation menu
		
		temp = [view_menu addItemWithTitle:NSLocalizedString(@"Rotation", nil) action:nil keyEquivalent:@""];
		NSMenu *rotation_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Rotation", nil)];
		if(rotation_menu != nil)
		{
			[temp setSubmenu:rotation_menu];

			rotation0_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 0", nil) action:@selector(setRotation0) keyEquivalent:@""];
			rotation90_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 90", nil) action:@selector(setRotation90) keyEquivalent:@""];
			rotation180_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 180", nil) action:@selector(setRotation180) keyEquivalent:@""];
			rotation270_item = [rotation_menu addItemWithTitle:NSLocalizedString(@"Rotation 270", nil) action:@selector(setRotation270) keyEquivalent:@""];

			[rotation_menu release];
		}
		
		[view_menu addItem:[NSMenuItem separatorItem]];

		//Layer Menu
		
		temp = [view_menu addItemWithTitle:NSLocalizedString(@"Layers", nil) action:nil keyEquivalent:@""];
		NSMenu *layer_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Layers", nil)];
		if(layer_menu != nil)
		{
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
		}
		
		[view_menu addItem:[NSMenuItem separatorItem]];

		screenshot_to_file_item = [view_menu addItemWithTitle:NSLocalizedString(@"Save Screenshot...", nil) action:@selector(saveScreenshot) keyEquivalent:@""];

		[view_menu release];
	}

	//Sound Menu
	
	temp = [main_menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenu *sound_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Sound", nil)];
	if(sound_menu != nil)
	{
		[main_menu setSubmenu:sound_menu forItem:temp];

		[temp = [sound_menu addItemWithTitle:NSLocalizedString(@"Volume", nil) action:nil keyEquivalent:@""] setTarget:[NSApp delegate]];

		NSMenu *volume_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Volume", nil)];
		if(volume_menu)
		{
			[sound_menu setSubmenu:volume_menu forItem:temp];

			for(i = 0; i < 10; i++)
				volume_item[i] = [volume_menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Volume %d", nil), (i+1)*10] action:@selector(setVolumeFromMenu:) keyEquivalent:@""];

			[volume_menu release];
		}
		
		[sound_menu addItem:[NSMenuItem separatorItem]];

		mute_item = [sound_menu addItemWithTitle:NSLocalizedString(@"Mute", nil) action:@selector(toggleMute) keyEquivalent:@""];

		//[sound_menu addItem:[NSMenuItem separatorItem]];
		//temp = [sound_menu addItemWithTitle:@"Record to File..." action:@selector(chooseSoundOutputFile) keyEquivalent: @"r"];
		//temp = [sound_menu addItemWithTitle:@"Pause Recording" action:@selector(startRecording) keyEquivalent: @""];
		//temp = [sound_menu addItemWithTitle:@"Save Recording" action:@selector(pauseRecording) keyEquivalent: @""];

		[sound_menu release];
	}

	//Create the help menu
	
	temp = [main_menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenu *help_menu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Help", nil)];
	if(help_menu != nil)
	{
		[main_menu setSubmenu:help_menu forItem:temp];

		[[help_menu addItemWithTitle:NSLocalizedString(@"Go to Website", nil) action:@selector(launchWebsite) keyEquivalent: @""] setTarget:NSApp];
		[[help_menu addItemWithTitle:NSLocalizedString(@"Go to Forums", nil) action:@selector(launchForums) keyEquivalent: @""] setTarget:NSApp];
        [[help_menu addItemWithTitle:NSLocalizedString(@"Submit a Bug Report", nil) action:@selector(bugReport) keyEquivalent: @""] setTarget:NSApp];

		[help_menu release];
	}
}

#ifdef GDB_STUB
//GDB Stub implementation----------------------------------------------------------------------------

void * createThread_gdb(void (*thread_function)( void *data),void *thread_data)
{
  // Create the thread using POSIX routines.
  pthread_attr_t  attr;
  pthread_t*      posixThreadID = (pthread_t*)malloc(sizeof(pthread_t));
  
  assert(!pthread_attr_init(&attr));
  assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
  
  int threadError = pthread_create(posixThreadID, &attr, (void* (*)(void *))thread_function, thread_data);
  
  assert(!pthread_attr_destroy(&attr));
  
  if (threadError != 0)
  {
    // Report an error.
    return NULL;
  }
  else
  {
    return posixThreadID;
  }
}

void joinThread_gdb( void *thread_handle)
{
  pthread_join(*((pthread_t*)thread_handle), NULL);
  free(thread_handle);
}

#endif

//Main Function--------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	srand(time(NULL));

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	[NSApplication sharedApplication];
	[NSApp setDelegate:[[AppDelegate alloc] init]];

	int result = NSApplicationMain(argc, (const char**)argv);
	
	[pool release];
	
	return result;
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
	
	//check if it should load something by default
	if([[[NSUserDefaults standardUserDefaults] stringForKey:PREF_AFTER_LAUNCHED] compare:PREF_AFTER_LAUNCHED_OPTION_LAST_ROM]==NSOrderedSame)
	{
		NSArray *recent_documents = [[NSDocumentController sharedDocumentController] recentDocumentURLs];
		
		if([recent_documents count] > 0)
		{
			//we have to convert from a URL to file path. in the future, URL's ought to be used since they are more capable/robust...
			
			NSString *file = [[recent_documents objectAtIndex:0] absoluteString]; //gets it in url form
			file = [file stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding]; //convert escaped characters in url
			file = [file substringFromIndex:16]; //gets rid of "File://localhost" url component (there should be a much better way to do this....)
			
			[main_window loadROM:file];
		}
	}
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
	//Ask user about quitting if a rom is loaded (avoid accidentally quiting with unsaved progress)
	if([main_window ROMLoaded])
	if(!messageDialogYN(NSLocalizedString(@"DeSmuME Emulator", nil), NSLocalizedString(@"Are you sure you want to quit?", nil)))
		return NSTerminateCancel;

	return NSTerminateNow;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
	[main_window release];
}

@end
