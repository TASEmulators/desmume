/*
	Copyright (C) 2007 Jeff Bland
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

#import <Cocoa/Cocoa.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif


@class NintendoDS;
@class InputHandler;
@class VideoOutputWindow;
@compatibility_alias CocoaDSCore NintendoDS;
@compatibility_alias CocoaDSRom NintendoDS;

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
@interface AppDelegate : NSObject <NSApplicationDelegate>
#else
@interface AppDelegate : NSObject
#endif
{
	IBOutlet NSWindow *mainWindow;
	IBOutlet NSObjectController *aboutWindowController;
	IBOutlet NSObjectController *emuWindowController;
	IBOutlet NSObjectController *cdsCoreController;
	IBOutlet NSObjectController *romInfoPanelController;
	IBOutlet NSTextView *readMeTextView;
	
	IBOutlet NSMenu *mLoadStateSlot;
	IBOutlet NSMenu *mSaveStateSlot;
	
	InputHandler *keyboardHandler;
}

// Tools Menu
- (IBAction) showSupportFolderInFinder:(id)sender;

// Help Menu
- (IBAction) launchWebsite:(id)sender;
- (IBAction) launchForums:(id)sender;
- (IBAction) bugReport:(id)sender;

- (void) setupSlotMenuItems;
- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber;

@end

#ifdef __cplusplus
extern "C"
{
#endif

void* createThread_gdb(void (*thread_function)( void *data),void *thread_data);
void joinThread_gdb(void *thread_handle);
	
#ifdef __cplusplus
	
}
#endif
