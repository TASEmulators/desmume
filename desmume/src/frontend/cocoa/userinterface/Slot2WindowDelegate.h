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

@interface Slot2WindowDelegate : NSObject <NSWindowDelegate, NSTableViewDelegate>
{
	NSObject *__unsafe_unretained dummyObject;
	NSWindow *__weak window;
	NSArrayController *__weak deviceListController;
	NSTableView *__weak deviceListTable;
	NSBox *__weak deviceSettingsBox;
	NSPopUpButton *__weak mpcfFileSearchMenu;
	PreferencesWindowDelegate *__weak prefWindowDelegate;
	
	NSView *currentDeviceView;
	NSView *__weak viewUnsupported;
	NSView *__weak viewNoSelection;
	NSView *__weak viewNone;
	NSView *__weak viewAuto;
	NSView *__weak viewCompactFlash;
	NSView *__weak viewRumblePak;
	NSView *__weak viewGBACartridge;
	NSView *__weak viewGuitarGrip;
	NSView *__weak viewMemoryExpansionPack;
	NSView *__weak viewPiano;
	NSView *__weak viewPaddleController;
	NSView *__weak viewPassME;
	
	CocoaDSSlot2Device *selectedDevice;
	CocoaDSSlot2Manager *deviceManager;
	InputHIDManager *hidManager;
	
	NSString *autoSelectedDeviceText;
	NSURL *mpcfFolderURL;
	NSURL *mpcfDiskImageURL;
	NSURL *gbaCartridgeURL;
	NSURL *gbaSRamURL;
}

@property (unsafe_unretained, readonly) IBOutlet NSObject *dummyObject;
@property (weak, readonly) IBOutlet NSWindow *window;
@property (weak, readonly) IBOutlet NSArrayController *deviceListController;
@property (weak, readonly) IBOutlet NSTableView *deviceListTable;
@property (weak, readonly) IBOutlet NSBox *deviceSettingsBox;
@property (weak, readonly) IBOutlet NSPopUpButton *mpcfFileSearchMenu;
@property (weak, readonly) IBOutlet PreferencesWindowDelegate *prefWindowDelegate;

@property (weak, readonly) IBOutlet NSView *viewUnsupported;
@property (weak, readonly) IBOutlet NSView *viewNoSelection;
@property (weak, readonly) IBOutlet NSView *viewNone;
@property (weak, readonly) IBOutlet NSView *viewAuto;
@property (weak, readonly) IBOutlet NSView *viewCompactFlash;
@property (weak, readonly) IBOutlet NSView *viewRumblePak;
@property (weak, readonly) IBOutlet NSView *viewGBACartridge;
@property (weak, readonly) IBOutlet NSView *viewGuitarGrip;
@property (weak, readonly) IBOutlet NSView *viewMemoryExpansionPack;
@property (weak, readonly) IBOutlet NSView *viewPiano;
@property (weak, readonly) IBOutlet NSView *viewPaddleController;
@property (weak, readonly) IBOutlet NSView *viewPassME;

@property (strong) CocoaDSSlot2Device *selectedDevice;
@property (strong) CocoaDSSlot2Manager *deviceManager;
@property (strong) InputHIDManager *hidManager;

@property (strong) NSString *autoSelectedDeviceText;
@property (strong) NSURL *mpcfFolderURL;
@property (strong) NSURL *mpcfDiskImageURL;
@property (weak) NSString *mpcfFolderName;
@property (weak) NSString *mpcfFolderPath;
@property (weak) NSString *mpcfDiskImageName;
@property (weak) NSString *mpcfDiskImagePath;
@property (strong) NSURL *gbaCartridgeURL;
@property (weak) NSString *gbaCartridgeName;
@property (weak) NSString *gbaCartridgePath;
@property (strong) NSURL *gbaSRamURL;
@property (weak) NSString *gbaSRamName;
@property (weak) NSString *gbaSRamPath;

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

#pragma mark -
#pragma mark GBA Cartridge
- (IBAction) chooseGbaCartridgePath:(id)sender;
- (IBAction) chooseGbaSRamPath:(id)sender;
- (IBAction) clearSRamPath:(id)sender;
- (BOOL) isGbaSRamWithCartridge;

#pragma mark -
#pragma mark Rumble Pak
- (IBAction) testRumble:(id)sender;
- (void) sendForceFeedback:(NSNotification *)aNotification;

#pragma mark -
#pragma mark Paddle

@end
