/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2018 DeSmuME Team

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
@class FileMigrationDelegate;
@class MacAVCaptureToolDelegate;
@class WifiSettingsPanelDelegate;


@interface AppDelegate : NSObject <NSApplicationDelegate>
{
	BOOL isAppRunningOnIntel;
    BOOL isDeveloperPlusBuild;
	BOOL didApplicationFinishLaunching;
	NSString *delayedROMFileName;
}

@property (weak) IBOutlet NSObject *dummyObject;

@property (weak) IBOutlet NSObjectController *aboutWindowController;
@property (weak) IBOutlet NSObjectController *emuControlController;
@property (weak) IBOutlet NSObjectController *prefWindowController;
@property (weak) IBOutlet NSObjectController *cdsCoreController;
@property (weak) IBOutlet FileMigrationDelegate *migrationDelegate;
@property (weak) IBOutlet MacAVCaptureToolDelegate *avCaptureToolDelegate;
@property (weak) IBOutlet WifiSettingsPanelDelegate *wifiSettingsPanelDelegate;
@property (weak) IBOutlet NSWindow *prefWindow;
@property (weak) IBOutlet NSWindow *troubleshootingWindow;
@property (weak) IBOutlet NSMenu *mLoadStateSlot;
@property (weak) IBOutlet NSMenu *mSaveStateSlot;
@property (weak) IBOutlet InputPrefsView *inputPrefsView;

@property (assign) BOOL isAppRunningOnIntel;
@property (assign) BOOL isDeveloperPlusBuild;
@property (assign) BOOL didApplicationFinishLaunching;
@property (copy) NSString *delayedROMFileName;

- (IBAction) launchWebsite:(id)sender;
- (IBAction) launchForums:(id)sender;
- (IBAction) supportRequest:(id)sender;
- (IBAction) bugReport:(id)sender;

- (void) setupSlotMenuItems;
- (NSMenuItem *) addSlotMenuItem:(NSMenu *)menu slotNumber:(NSUInteger)slotNumber;
- (void) setupUserDefaults;

@end
