/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2018 DeSmuME Team

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
#import <OpenGL/OpenGL.h>
#import "inputPrefsView.h"

@class CocoaVideoFilter;
class OGLImage;

@interface DisplayPreviewView : NSView
{
	OGLImage *oglImage;
	NSOpenGLContext *context;
	CGLContextObj cglDisplayContext;
	
	bool isPreviewImageLoaded;
}

@property (assign) BOOL filtersPreferGPU;
@property (assign) BOOL sourceDeposterize;
@property (assign) NSInteger pixelScaler;
@property (assign) NSInteger outputFilter;

@end

#pragma mark -

@interface PreferencesWindowDelegate : NSObject <NSWindowDelegate, NSControlTextEditingDelegate>
{
	NSDictionary *prefViewDict;
	
	NSImage *iconVolumeFull;
	NSImage *iconVolumeTwoThird;
	NSImage *iconVolumeOneThird;
	NSImage *iconVolumeMute;
	
	NSMutableDictionary *bindings;
}

@property (weak) IBOutlet NSObject *dummyObject;
@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSToolbar *toolbar;
@property (weak) IBOutlet NSWindow *firmwareConfigSheet;
@property (weak) IBOutlet NSObjectController *cdsCoreController;
@property (weak) IBOutlet NSObjectController *emuController;
@property (weak) IBOutlet NSObjectController *prefWindowController;
@property (weak) IBOutlet NSObjectController *cheatWindowController;
@property (weak) IBOutlet NSArrayController *cheatDatabaseController;
@property (weak) IBOutlet NSToolbarItem *toolbarItemGeneral;
@property (weak) IBOutlet NSToolbarItem *toolbarItemInput;
@property (weak) IBOutlet NSToolbarItem *toolbarItemDisplay;
@property (weak) IBOutlet NSToolbarItem *toolbarItemSound;
@property (weak) IBOutlet NSToolbarItem *toolbarItemEmulation;
@property (weak) IBOutlet NSView *viewGeneral;
@property (weak) IBOutlet InputPrefsView *viewInput;
@property (weak) IBOutlet NSView *viewDisplay;
@property (weak) IBOutlet NSView *viewSound;
@property (weak) IBOutlet NSView *viewEmulation;

@property (weak) IBOutlet NSPopUpButton *displayRotationMenu;
@property (weak) IBOutlet NSMenuItem *displayRotationMenuCustomItem;
@property (weak) IBOutlet NSTextField *displayRotationField;
@property (weak) IBOutlet NSPopUpButton *spuSyncMethodMenu;

@property (weak) IBOutlet NSPopUpButton *openglMSAAPopUpButton;

@property (weak) IBOutlet DisplayPreviewView *previewView;

@property (copy) NSString *firmwareMACAddressString;
@property (copy) NSString *subnetMaskString_AP1;
@property (copy) NSString *subnetMaskString_AP2;
@property (copy) NSString *subnetMaskString_AP3;

@property (readonly, strong) NSMutableDictionary *bindings;

- (IBAction) changePrefView:(id)sender;

- (IBAction) chooseAdvansceneDatabase:(id)sender;
- (IBAction) chooseRomForAutoload:(id)sender;
- (IBAction) chooseCheatDatabase:(id)sender;

- (IBAction) selectDisplayRotation:(id)sender;
- (void) updateDisplayRotationMenu:(double)displayRotation;

- (IBAction) updateFiltersPreferGPU:(id)sender;
- (IBAction) updateSourceDeposterize:(id)sender;
- (IBAction) selectOutputFilter:(id)sender;
- (IBAction) selectPixelScaler:(id)sender;

- (IBAction) updateVolumeIcon:(id)sender;
- (IBAction) selectSPUSyncMode:(id)sender;
- (IBAction) selectSPUSyncMethod:(id)sender;

- (IBAction) chooseARM9BiosImage:(id)sender;
- (IBAction) chooseARM7BiosImage:(id)sender;
- (IBAction) chooseFirmwareImage:(id)sender;
- (IBAction) configureInternalFirmware:(id)sender;
- (IBAction) closeFirmwareConfigSheet:(id)sender;

- (IBAction) generateFirmwareMACAddress:(id)sender;
- (IBAction) updateFirmwareMACAddressString:(id)sender;
- (IBAction) updateSubnetMaskString_AP1:(id)sender;
- (IBAction) updateSubnetMaskString_AP2:(id)sender;
- (IBAction) updateSubnetMaskString_AP3:(id)sender;

- (void) switchContentView:(NSView *)theView;
- (void) markUnsupportedOpenGLMSAAMenuItems;
- (void) setupUserDefaults;

@end
