/*
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

#import <Cocoa/Cocoa.h>

@class InputPrefsView;
@class CocoaHIDManager;


#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
@interface AppDelegate : NSObject <NSApplicationDelegate>
#else
@interface AppDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSArrayController *fileMigrationList;
	NSObjectController *aboutWindowController;
	NSObjectController *emuWindowController;
	NSObjectController *prefWindowController;
	NSObjectController *cheatWindowController;
	NSObjectController *cdsCoreController;
	
    NSWindow *mainWindow;
	NSWindow *prefWindow;
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
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSArrayController *fileMigrationList;
@property (readonly) IBOutlet NSObjectController *aboutWindowController;
@property (readonly) IBOutlet NSObjectController *emuWindowController;
@property (readonly) IBOutlet NSObjectController *prefWindowController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet NSWindow *mainWindow;
@property (readonly) IBOutlet NSWindow *prefWindow;
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

// Emulation Menu

// View Menu

// Tools Menu
- (IBAction) showSupportFolderInFinder:(id)sender;

// Help Menu
- (IBAction) launchWebsite:(id)sender;
- (IBAction) launchForums:(id)sender;
- (IBAction) bugReport:(id)sender;

- (void) setupSlotMenuItems;
- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber;
- (void) setupUserDefaults;
- (void) updateInputDisplayFields;
- (IBAction) showMigrationWindow:(id)sender;
- (IBAction) handleMigrationWindow:(id)sender;
- (void) setRomInfoPanelBoxTitleColors;

@end
