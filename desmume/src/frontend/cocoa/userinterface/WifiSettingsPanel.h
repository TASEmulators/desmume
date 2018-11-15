/*
	Copyright (C) 2018 DeSmuME team

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

class ClientExecutionControl;

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface WifiSettingsPanelDelegate : NSObject <NSWindowDelegate>
#else
@interface WifiSettingsPanelDelegate : NSObject
#endif
{
	NSObjectController *firmwarePanelController;
	NSPopUpButton *bridgeDevicePopUpButton;
	
	ClientExecutionControl *execControl;
	NSString *_libpcapDeviceSelectionName;
	
	NSString *myMACAddress1String;
	NSString *myMACAddress2String;
	NSString *myMACAddress3String;
	NSString *myMACAddress4String;
}

@property (assign) ClientExecutionControl *execControl;

@property (assign) IBOutlet NSObjectController *firmwarePanelController;
@property (assign) IBOutlet NSPopUpButton *bridgeDevicePopUpButton;

@property (assign) NSInteger wifiEmulationMode;
@property (assign) NSInteger bridgeDeviceSelection;

@property (assign) NSString *myMACAddress1String;
@property (assign) NSString *myMACAddress2String;
@property (assign) NSString *myMACAddress3String;
@property (assign) NSString *myMACAddress4String;

- (void) updateCustomMACAddressStrings;
- (void) fillLibpcapDeviceMenu;
- (IBAction) generateNewAddresses:(id)sender;
- (IBAction) writeDefaultsWifiSettings:(id)sender;
- (void) readUserDefaults;

@end
