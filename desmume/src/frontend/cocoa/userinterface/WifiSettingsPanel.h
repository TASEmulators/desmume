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

@interface WifiSettingsPanelDelegate : NSObject <NSWindowDelegate>
{
	ClientExecutionControl *execControl;
	NSString *_libpcapDeviceSelectionName;
	
	NSString *myMACAddress1String;
	NSString *myMACAddress2String;
	NSString *myMACAddress3String;
	NSString *myMACAddress4String;
}

@property (assign) ClientExecutionControl *execControl;

@property (weak) IBOutlet NSObjectController *firmwarePanelController;
@property (weak) IBOutlet NSPopUpButton *bridgeDevicePopUpButton;

@property (assign) NSInteger wifiEmulationMode;
@property (assign) NSInteger bridgeDeviceSelection;

@property (copy) NSString *myMACAddress1String;
@property (copy) NSString *myMACAddress2String;
@property (copy) NSString *myMACAddress3String;
@property (copy) NSString *myMACAddress4String;

- (void) updateCustomMACAddressStrings;
- (void) fillLibpcapDeviceMenu;
- (IBAction) generateNewAddresses:(id)sender;
- (IBAction) writeDefaultsWifiSettings:(id)sender;
- (void) readUserDefaults;

@end
