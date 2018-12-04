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


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface AppDelegate : NSObject <NSApplicationDelegate>
#else
@interface AppDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	
	NSObjectController *aboutWindowController;
	NSObjectController *emuControlController;
	NSObjectController *prefWindowController;
	NSObjectController *cdsCoreController;
	FileMigrationDelegate *migrationDelegate;
	MacAVCaptureToolDelegate *avCaptureToolDelegate;
	WifiSettingsPanelDelegate *wifiSettingsPanelDelegate;
	
	NSWindow *prefWindow;
	NSWindow *troubleshootingWindow;
	InputPrefsView *inputPrefsView;
	
	NSMenu *mLoadStateSlot;
	NSMenu *mSaveStateSlot;
	
	BOOL isAppRunningOnIntel;
    BOOL isDeveloperPlusBuild;
	BOOL didApplicationFinishLaunching;
	NSString *delayedROMFileName;
}

@property (readonly) IBOutlet NSObject *dummyObject;

@property (readonly) IBOutlet NSObjectController *aboutWindowController;
@property (readonly) IBOutlet NSObjectController *emuControlController;
@property (readonly) IBOutlet NSObjectController *prefWindowController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet FileMigrationDelegate *migrationDelegate;
@property (readonly) IBOutlet MacAVCaptureToolDelegate *avCaptureToolDelegate;
@property (readonly) IBOutlet WifiSettingsPanelDelegate *wifiSettingsPanelDelegate;
@property (readonly) IBOutlet NSWindow *prefWindow;
@property (readonly) IBOutlet NSWindow *troubleshootingWindow;
@property (readonly) IBOutlet NSMenu *mLoadStateSlot;
@property (readonly) IBOutlet NSMenu *mSaveStateSlot;
@property (readonly) IBOutlet InputPrefsView *inputPrefsView;

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
