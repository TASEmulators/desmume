/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2013 DeSmuME team

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
#import "inputPrefsView.h"

@class CocoaVideoFilter;


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface PreferencesWindowDelegate : NSObject <NSWindowDelegate>
#else
@interface PreferencesWindowDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	NSWindow *firmwareConfigSheet;
	NSObjectController *cdsCoreController;
	NSObjectController *emuController;
	NSObjectController *prefWindowController;
	NSObjectController *cheatWindowController;
	NSArrayController *cheatDatabaseController;
	
	NSView *viewGeneral;
	InputPrefsView *viewInput;
	NSView *viewDisplay;
	NSView *viewSound;
	NSView *viewEmulation;
		
	NSPopUpButton *displayRotationMenu;
	NSMenuItem *displayRotationMenuCustomItem;
	NSTextField *displayRotationField;
	
	NSImage *iconVolumeFull;
	NSImage *iconVolumeTwoThird;
	NSImage *iconVolumeOneThird;
	NSImage *iconVolumeMute;
	NSPopUpButton *spuSyncMethodMenu;
	
	NSImageView *previewImageView;
	CocoaVideoFilter *videoFilter;
	CocoaVideoFilter *bilinearVideoFilter;
	
	NSMutableDictionary *bindings;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSWindow *firmwareConfigSheet;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet NSObjectController *emuController;
@property (readonly) IBOutlet NSObjectController *prefWindowController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSArrayController *cheatDatabaseController;
@property (readonly) IBOutlet NSView *viewGeneral;
@property (readonly) IBOutlet InputPrefsView *viewInput;
@property (readonly) IBOutlet NSView *viewDisplay;
@property (readonly) IBOutlet NSView *viewSound;
@property (readonly) IBOutlet NSView *viewEmulation;

@property (readonly) IBOutlet NSPopUpButton *displayRotationMenu;
@property (readonly) IBOutlet NSMenuItem *displayRotationMenuCustomItem;
@property (readonly) IBOutlet NSTextField *displayRotationField;
@property (readonly) IBOutlet NSPopUpButton *spuSyncMethodMenu;

@property (readonly) IBOutlet NSImageView *previewImageView;

@property (readonly) NSMutableDictionary *bindings;

- (IBAction) showGeneralView:(id)sender;
- (IBAction) showInputView:(id)sender;
- (IBAction) showDisplayView:(id)sender;
- (IBAction) showSoundView:(id)sender;
- (IBAction) showEmulationView:(id)sender;

- (IBAction) chooseAdvansceneDatabase:(id)sender;
- (void) chooseAdvansceneDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction) chooseRomForAutoload:(id)sender;
- (void) chooseRomForAutoloadDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction) chooseCheatDatabase:(id)sender;
- (void) chooseCheatDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (IBAction) selectDisplayRotation:(id)sender;
- (void) updateDisplayRotationMenu:(double)displayRotation;

- (IBAction) setUseBilinear:(id)sender;
- (IBAction) selectVideoFilterType:(id)sender;

- (IBAction) updateVolumeIcon:(id)sender;
- (IBAction) selectSPUSyncMode:(id)sender;
- (IBAction) selectSPUSyncMethod:(id)sender;

- (IBAction) chooseARM9BiosImage:(id)sender;
- (IBAction) chooseARM7BiosImage:(id)sender;
- (IBAction) chooseFirmwareImage:(id)sender;
- (void) chooseArm9BiosImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (void) chooseArm7BiosImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (void) chooseFirmwareImageDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction) configureInternalFirmware:(id)sender;
- (IBAction) closeFirmwareConfigSheet:(id)sender;
- (void) didEndFirmwareConfigSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (void) switchContentView:(NSView *)theView;

@end
