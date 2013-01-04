/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2013 DeSmuME team

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

#import "appDelegate_legacy.h"
#import "emuWindowDelegate_legacy.h"
#import "cocoa_input_legacy.h"
#import "cocoa_file.h"
#import "cocoa_util.h"
#import "cocoa_globals.h"
#import "nds_control_legacy.h"
#import "input_legacy.h"
#import "preferences_legacy.h"
#include "sndOSX.h"

#undef BOOL

#ifdef GDB_STUB
#include <pthread.h>
#endif

#ifdef GDB_STUB
//GDB Stub implementation----------------------------------------------------------------------------

void* createThread_gdb(void (*thread_function)( void *data),void *thread_data)
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

void joinThread_gdb(void *thread_handle)
{
	pthread_join(*((pthread_t*)thread_handle), NULL);
	free(thread_handle);
}

#endif

@implementation AppDelegate

- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
	BOOL result = NO;
	NSURL *fileURL = [NSURL fileURLWithPath:filename];
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	
	if (cdsCore == nil)
	{
		return result;
	}
	
	NSString *fileKind = [CocoaDSFile fileKindByURL:fileURL];
	if ([fileKind isEqualToString:@"ROM"])
	{
		result = [mainWindowDelegate handleLoadRom:fileURL];
	}
	
	return result;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
	EmuWindowDelegate *mainWindowDelegate = (EmuWindowDelegate *)[mainWindow delegate];
	
	// Create the needed directories in Application Support if they haven't already
	// been created.
	if (![CocoaDSFile setupAllAppDirectories])
	{
		[CocoaDSUtil quickDialogUsingTitle:NSLocalizedString(@"Error", nil) message:NSLocalizedString(@"Could not create the required directories in your Application Support folder. DeSmuME will now quit.", nil)];
		return;
	}
	
	[CocoaDSFile setupAllFilePaths];
	
	// Setup the About window.
	NSMutableDictionary *aboutWindowProperties = [NSMutableDictionary dictionaryWithObjectsAndKeys:
												  [[NSBundle mainBundle] pathForResource:@FILENAME_README ofType:@""], @"readMePath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_COPYING ofType:@""], @"licensePath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_AUTHORS ofType:@""], @"authorsPath",
												  [[NSBundle mainBundle] pathForResource:@FILENAME_CHANGELOG ofType:@""], @"changeLogPath",
												  [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"], @"versionString",
												  @__DATE__, @"dateString",
												  nil];
	
	[aboutWindowController setContent:aboutWindowProperties];
	
	//Set default values for all preferences
	//(this wont override saved preferences as
	//they work in different preference domains)
	setAppDefaults();
	
	// Setup the slot menu items. We set this up manually instead of through Interface
	// Builder because we're assuming an arbitrary number of slot items.
	[self setupSlotMenuItems];
	
	// Setup the user interface controllers.
	[romInfoPanelController setContent:[CocoaDSRom romNotLoadedBindings]];
	[emuWindowController setContent:[mainWindowDelegate bindings]];
	
	// Init the DS emulation core.
	CocoaDSCore *newCore = [[CocoaDSCore alloc] init];
	[newCore setVideoUpdateCallback:@selector(setScreenState:) withObject:[mainWindowDelegate displayView]];
	[newCore setErrorCallback:@selector(emulationError) withObject:[mainWindow delegate]];
	[cdsCoreController setContent:newCore];
	
	// Init the DS controller.
	CocoaDSController *newController = [[CocoaDSController alloc] init];
	[newCore setCdsController:newController];
	[[mainWindowDelegate displayView] setCdsController:newController];
	
	keyboardHandler = [[InputHandler alloc] initWithCdsController:newController];
	NSResponder *mainNextResponder = [mainWindow nextResponder];
	[mainWindow setNextResponder:keyboardHandler];
	[keyboardHandler setNextResponder:mainNextResponder];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	
	//Bring the application to the front
	[NSApp activateIgnoringOtherApps:TRUE];
	
	//check if it should load something by default
	if([[[NSUserDefaults standardUserDefaults] stringForKey:PREF_AFTER_LAUNCHED] compare:PREF_AFTER_LAUNCHED_OPTION_LAST_ROM]==NSOrderedSame)
	{
		NSArray *recent_documents = [[NSDocumentController sharedDocumentController] recentDocumentURLs];
		
		if([recent_documents count] > 0)
		{
			NSURL *romURL = [recent_documents objectAtIndex:0];
			
			[mainWindowDelegate loadRom:romURL];
		}
	}
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	EmuWindowDelegate *mainWindowDelegate = [mainWindow delegate];
	
	//Ask user about quitting if a rom is loaded (avoid accidentally quitting with unsaved progress)
	if([mainWindowDelegate isRomLoaded])
		if(![CocoaDSUtil quickYesNoDialogUsingTitle:NSLocalizedString(@"DeSmuME Emulator", nil) message:NSLocalizedString(@"Are you sure you want to quit?", nil)])
			return NSTerminateCancel;
	
	return NSTerminateNow;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
	CocoaDSCore *cdsCore = (CocoaDSCore *)[cdsCoreController content];
	[cdsCore pause];
	
	[keyboardHandler release];
	[[cdsCore cdsController] release];
	
	[cdsCore release];
	[cdsCoreController setContent:nil];
}

- (IBAction) showSupportFolderInFinder:(id)sender
{
	NSURL *folderURL = [CocoaDSFile userAppSupportBaseURL];
	
	[[NSWorkspace sharedWorkspace] openFile:[folderURL path] withApplication:@"Finder"];
}

- (IBAction) launchWebsite:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_WEBSITE]];
}

- (IBAction) launchForums:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_FORUM_SITE]];
}

- (IBAction) bugReport:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@STRING_DESMUME_BUG_REPORT_SITE]];
}

- (void) setupSlotMenuItems
{
	NSInteger i;
	NSMenuItem *loadItem = nil;
	NSMenuItem *saveItem = nil;
	
	for(i = 0; i < MAX_SAVESTATE_SLOTS; i++)
	{
		loadItem = [self addSlotMenuItem:mLoadStateSlot slotNumber:(NSUInteger)(i + 1)];
		[loadItem setKeyEquivalentModifierMask:0];
		[loadItem setTag:i];
		[loadItem setAction:@selector(loadEmuSaveStateSlot:)];
		
		saveItem = [self addSlotMenuItem:mSaveStateSlot slotNumber:(NSUInteger)(i + 1)];
		[saveItem setKeyEquivalentModifierMask:NSShiftKeyMask];
		[saveItem setTag:i];
		[saveItem setAction:@selector(saveEmuSaveStateSlot:)];
	}
}

- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber
{
	NSUInteger slotNumberKey = slotNumber;
	
	if (slotNumber == 10)
	{
		slotNumberKey = 0;
	}
	
	NSMenuItem *mItem = [menu addItemWithTitle:[NSString stringWithFormat:NSSTRING_TITLE_SLOT_NUMBER, slotNumber]
										action:nil
								 keyEquivalent:[NSString stringWithFormat:@"%d", slotNumberKey]];
	
	[mItem setTarget:[mainWindow delegate]];
	
	return mItem;
}

@end
