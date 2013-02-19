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
@class CocoaHIDManager;
@class EmuControllerDelegate;


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface AppDelegate : NSObject <NSApplicationDelegate>
#else
@interface AppDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	
	NSArrayController *fileMigrationList;
	NSObjectController *aboutWindowController;
	NSObjectController *emuControlController;
	NSObjectController *cdsSoundController;
	NSObjectController *romInfoPanelController;
	NSObjectController *prefWindowController;
	NSObjectController *cheatWindowController;
	NSObjectController *cdsCoreController;
	
	NSWindow *prefWindow;
	NSWindow *troubleshootingWindow;
	NSWindow *cheatListWindow;
	NSWindow *migrationWindow;
	NSView *prefGeneralView;
	InputPrefsView *inputPrefsView;
	
	NSMenu *mLoadStateSlot;
	NSMenu *mSaveStateSlot;
	
	NSBox *boxGeneralInfo;
	NSBox *boxTitles;
	NSBox *boxARMBinaries;
	NSBox *boxFileSystem;
	NSBox *boxMisc;
	
	CocoaHIDManager *hidManager;
	BOOL migrationFilesPresent;
	BOOL isAppRunningOnIntel;
}

@property (readonly) IBOutlet NSObject *dummyObject;

@property (readonly) IBOutlet NSArrayController *fileMigrationList;
@property (readonly) IBOutlet NSObjectController *aboutWindowController;
@property (readonly) IBOutlet NSObjectController *emuControlController;
@property (readonly) IBOutlet NSObjectController *cdsSoundController;
@property (readonly) IBOutlet NSObjectController *romInfoPanelController;
@property (readonly) IBOutlet NSObjectController *prefWindowController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet NSWindow *prefWindow;
@property (readonly) IBOutlet NSWindow *troubleshootingWindow;
@property (readonly) IBOutlet NSWindow *cheatListWindow;
@property (readonly) IBOutlet NSWindow *migrationWindow;
@property (readonly) IBOutlet NSView *prefGeneralView;
@property (readonly) IBOutlet NSMenu *mLoadStateSlot;
@property (readonly) IBOutlet NSMenu *mSaveStateSlot;
@property (readonly) IBOutlet InputPrefsView *inputPrefsView;

@property (readonly) IBOutlet NSBox *boxGeneralInfo;
@property (readonly) IBOutlet NSBox *boxTitles;
@property (readonly) IBOutlet NSBox *boxARMBinaries;
@property (readonly) IBOutlet NSBox *boxFileSystem;
@property (readonly) IBOutlet NSBox *boxMisc;

@property (retain) CocoaHIDManager *hidManager;
@property (assign) BOOL migrationFilesPresent;
@property (assign) BOOL isAppRunningOnIntel;

// Emulation Menu

// View Menu

// Tools Menu
- (IBAction) showSupportFolderInFinder:(id)sender;

// Help Menu
- (IBAction) launchWebsite:(id)sender;
- (IBAction) launchForums:(id)sender;
- (IBAction) supportRequest:(id)sender;
- (IBAction) bugReport:(id)sender;

- (void) setupSlotMenuItems;
- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber;
- (void) setupUserDefaults;
- (void) updateInputDisplayFields;
- (IBAction) showMigrationWindow:(id)sender;
- (IBAction) handleMigrationWindow:(id)sender;
- (void) setRomInfoPanelBoxTitleColors;

@end
