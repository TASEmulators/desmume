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
	NSView *__weak currentDeviceView;
	
	CocoaDSSlot2Device *selectedDevice;
	CocoaDSSlot2Manager *deviceManager;
	InputHIDManager *hidManager;
	
	NSString *autoSelectedDeviceText;
	NSURL *mpcfFolderURL;
	NSURL *mpcfDiskImageURL;
	NSURL *gbaCartridgeURL;
	NSURL *gbaSRamURL;
}

@property (weak) IBOutlet NSObject *dummyObject;
@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSArrayController *deviceListController;
@property (weak) IBOutlet NSTableView *deviceListTable;
@property (weak) IBOutlet NSBox *deviceSettingsBox;
@property (weak) IBOutlet NSPopUpButton *mpcfFileSearchMenu;
@property (weak) IBOutlet PreferencesWindowDelegate *prefWindowDelegate;

@property (weak) IBOutlet NSView *viewUnsupported;
@property (weak) IBOutlet NSView *viewNoSelection;
@property (weak) IBOutlet NSView *viewNone;
@property (weak) IBOutlet NSView *viewAuto;
@property (weak) IBOutlet NSView *viewCompactFlash;
@property (weak) IBOutlet NSView *viewRumblePak;
@property (weak) IBOutlet NSView *viewGBACartridge;
@property (weak) IBOutlet NSView *viewGuitarGrip;
@property (weak) IBOutlet NSView *viewMemoryExpansionPack;
@property (weak) IBOutlet NSView *viewPiano;
@property (weak) IBOutlet NSView *viewPaddleController;
@property (weak) IBOutlet NSView *viewPassME;

@property (strong) CocoaDSSlot2Device *selectedDevice;
@property (strong) CocoaDSSlot2Manager *deviceManager;
@property (strong) InputHIDManager *hidManager;

@property (copy) NSString *autoSelectedDeviceText;
@property (nonatomic, strong) NSURL *mpcfFolderURL;
@property (nonatomic, strong) NSURL *mpcfDiskImageURL;
@property (readonly, copy) NSString *mpcfFolderName;
@property (readonly, copy) NSString *mpcfFolderPath;
@property (readonly, copy) NSString *mpcfDiskImageName;
@property (readonly, copy) NSString *mpcfDiskImagePath;
@property (nonatomic, strong) NSURL *gbaCartridgeURL;
@property (readonly, copy) NSString *gbaCartridgeName;
@property (readonly, copy) NSString *gbaCartridgePath;
@property (nonatomic, strong) NSURL *gbaSRamURL;
@property (readonly, copy) NSString *gbaSRamName;
@property (readonly, copy) NSString *gbaSRamPath;

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
