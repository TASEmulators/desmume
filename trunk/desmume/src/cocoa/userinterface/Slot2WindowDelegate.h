/*
	Copyright (C) 2014 DeSmuME Team

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
#import "preferencesWindowDelegate.h"
#import "cocoa_slot2.h"

@class InputHIDManager;

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface Slot2WindowDelegate : NSObject <NSWindowDelegate, NSTableViewDelegate>
#else
@interface Slot2WindowDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	NSArrayController *deviceListController;
	NSTableView *deviceListTable;
	NSBox *deviceSettingsBox;
	NSPopUpButton *mpcfFileSearchMenu;
	PreferencesWindowDelegate *prefWindowDelegate;
	
	NSView *currentDeviceView;
	NSView *viewUnsupported;
	NSView *viewNoSelection;
	NSView *viewNone;
	NSView *viewAuto;
	NSView *viewCompactFlash;
	NSView *viewRumblePak;
	NSView *viewGBACartridge;
	NSView *viewGuitarGrip;
	NSView *viewMemoryExpansionPack;
	NSView *viewPiano;
	NSView *viewPaddleController;
	NSView *viewPassME;
	
	CocoaDSSlot2Device *selectedDevice;
	CocoaDSSlot2Manager *deviceManager;
	InputHIDManager *hidManager;
	
	NSString *autoSelectedDeviceText;
	NSURL *mpcfFolderURL;
	NSURL *mpcfDiskImageURL;
	NSURL *gbaCartridgeURL;
	NSURL *gbaSRamURL;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSArrayController *deviceListController;
@property (readonly) IBOutlet NSTableView *deviceListTable;
@property (readonly) IBOutlet NSBox *deviceSettingsBox;
@property (readonly) IBOutlet NSPopUpButton *mpcfFileSearchMenu;
@property (readonly) IBOutlet PreferencesWindowDelegate *prefWindowDelegate;

@property (readonly) IBOutlet NSView *viewUnsupported;
@property (readonly) IBOutlet NSView *viewNoSelection;
@property (readonly) IBOutlet NSView *viewNone;
@property (readonly) IBOutlet NSView *viewAuto;
@property (readonly) IBOutlet NSView *viewCompactFlash;
@property (readonly) IBOutlet NSView *viewRumblePak;
@property (readonly) IBOutlet NSView *viewGBACartridge;
@property (readonly) IBOutlet NSView *viewGuitarGrip;
@property (readonly) IBOutlet NSView *viewMemoryExpansionPack;
@property (readonly) IBOutlet NSView *viewPiano;
@property (readonly) IBOutlet NSView *viewPaddleController;
@property (readonly) IBOutlet NSView *viewPassME;

@property (retain) CocoaDSSlot2Device *selectedDevice;
@property (retain) CocoaDSSlot2Manager *deviceManager;
@property (retain) InputHIDManager *hidManager;

@property (retain) NSString *autoSelectedDeviceText;
@property (retain) NSURL *mpcfFolderURL;
@property (retain) NSURL *mpcfDiskImageURL;
@property (assign) NSString *mpcfFolderName;
@property (assign) NSString *mpcfFolderPath;
@property (assign) NSString *mpcfDiskImageName;
@property (assign) NSString *mpcfDiskImagePath;
@property (retain) NSURL *gbaCartridgeURL;
@property (assign) NSString *gbaCartridgeName;
@property (assign) NSString *gbaCartridgePath;
@property (retain) NSURL *gbaSRamURL;
@property (assign) NSString *gbaSRamName;
@property (assign) NSString *gbaSRamPath;

- (IBAction) applySettings:(id)sender;
- (IBAction) showInputPreferences:(id)sender;

- (void) update;
- (void) selectDeviceByType:(NSInteger)theType;
- (void) setDeviceViewByDevice:(CocoaDSSlot2Device *)theDevice;
- (void) setupUserDefaults;

#pragma mark -
#pragma mark Auto

#pragma mark -
#pragma mark Compact Flash
- (IBAction) chooseMPCFPath:(id)sender;
- (void) chooseMPCFPathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

#pragma mark -
#pragma mark GBA Cartridge
- (IBAction) chooseGbaCartridgePath:(id)sender;
- (void) chooseGbaCartridgePathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction) chooseGbaSRamPath:(id)sender;
- (void) chooseGbaSRamPathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction) clearSRamPath:(id)sender;
- (BOOL) isGbaSRamWithCartridge;

#pragma mark -
#pragma mark Rumble Pak
- (IBAction) testRumble:(id)sender;
- (void) sendForceFeedback:(NSNotification *)aNotification;

#pragma mark -
#pragma mark Paddle

@end
