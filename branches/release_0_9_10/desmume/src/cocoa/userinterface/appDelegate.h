/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2013 DeSmuME team

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

@class InputPrefsView;
@class InputManager;
@class FileMigrationDelegate;


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface AppDelegate : NSObject <NSApplicationDelegate>
#else
@interface AppDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	
	NSObjectController *aboutWindowController;
	NSObjectController *emuControlController;
	NSObjectController *cdsSoundController;
	NSObjectController *romInfoPanelController;
	NSObjectController *prefWindowController;
	NSObjectController *cheatWindowController;
	NSObjectController *cdsCoreController;
	FileMigrationDelegate *migrationDelegate;
	InputManager *inputManager;
	
	NSWindow *prefWindow;
	NSWindow *troubleshootingWindow;
	NSWindow *cheatListWindow;
	NSView *prefGeneralView;
	InputPrefsView *inputPrefsView;
	
	NSMenu *mLoadStateSlot;
	NSMenu *mSaveStateSlot;
	
	NSBox *boxGeneralInfo;
	NSBox *boxTitles;
	NSBox *boxARMBinaries;
	NSBox *boxFileSystem;
	NSBox *boxMisc;
	
	BOOL isAppRunningOnIntel;
}

@property (readonly) IBOutlet NSObject *dummyObject;

@property (readonly) IBOutlet NSObjectController *aboutWindowController;
@property (readonly) IBOutlet NSObjectController *emuControlController;
@property (readonly) IBOutlet NSObjectController *cdsSoundController;
@property (readonly) IBOutlet NSObjectController *romInfoPanelController;
@property (readonly) IBOutlet NSObjectController *prefWindowController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet FileMigrationDelegate *migrationDelegate;
@property (readonly) IBOutlet InputManager *inputManager;
@property (readonly) IBOutlet NSWindow *prefWindow;
@property (readonly) IBOutlet NSWindow *troubleshootingWindow;
@property (readonly) IBOutlet NSWindow *cheatListWindow;
@property (readonly) IBOutlet NSView *prefGeneralView;
@property (readonly) IBOutlet NSMenu *mLoadStateSlot;
@property (readonly) IBOutlet NSMenu *mSaveStateSlot;
@property (readonly) IBOutlet InputPrefsView *inputPrefsView;

@property (readonly) IBOutlet NSBox *boxGeneralInfo;
@property (readonly) IBOutlet NSBox *boxTitles;
@property (readonly) IBOutlet NSBox *boxARMBinaries;
@property (readonly) IBOutlet NSBox *boxFileSystem;
@property (readonly) IBOutlet NSBox *boxMisc;

@property (assign) BOOL isAppRunningOnIntel;

- (IBAction) launchWebsite:(id)sender;
- (IBAction) launchForums:(id)sender;
- (IBAction) supportRequest:(id)sender;
- (IBAction) bugReport:(id)sender;

- (void) setupSlotMenuItems;
- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber;
- (void) setupUserDefaults;
- (void) setRomInfoPanelBoxTitleColors;
- (void) restoreDisplayWindowStates;
- (void) saveDisplayWindowStates;

@end
