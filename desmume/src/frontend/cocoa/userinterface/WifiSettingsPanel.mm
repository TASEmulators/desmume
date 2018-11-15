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

#include <string>
#include <vector>

#import "WifiSettingsPanel.h"
#import "../cocoa_firmware.h"
#include "../ClientExecutionControl.h"
#include "../../../wifi.h"
#undef BOOL


@implementation WifiSettingsPanelDelegate

@synthesize firmwarePanelController;
@synthesize bridgeDevicePopUpButton;

@synthesize execControl;

@dynamic wifiEmulationMode;
@dynamic bridgeDeviceSelection;

@synthesize myMACAddress1String;
@synthesize myMACAddress2String;
@synthesize myMACAddress3String;
@synthesize myMACAddress4String;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	execControl = NULL;
	
	const uint8_t myMAC4 = 0x12;
	const uint8_t myMAC5 = 0x34;
	const uint8_t myMAC6 = 0x50;
	
	myMACAddress1String = [NSString stringWithFormat:@"My Address #1  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 1];
	myMACAddress2String = [NSString stringWithFormat:@"My Address #2  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 2];
	myMACAddress3String = [NSString stringWithFormat:@"My Address #3  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 3];
	myMACAddress4String = [NSString stringWithFormat:@"My Address #4  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 4];
	
	_libpcapDeviceSelectionName = @"";
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void) setWifiEmulationMode:(NSInteger)theMode
{
	execControl->SetWifiEmulationMode((int)theMode);
}

- (NSInteger) wifiEmulationMode
{
	if (execControl == NULL)
	{
		return WifiEmulationLevel_Off;
	}
	
	return (NSInteger)execControl->GetWifiEmulationMode();
}

- (void) setBridgeDeviceSelection:(NSInteger)theSelection
{
	_libpcapDeviceSelectionName = [bridgeDevicePopUpButton itemTitleAtIndex:theSelection];
	execControl->SetWifiBridgeDeviceIndex((int)theSelection);
}

- (NSInteger) bridgeDeviceSelection
{
	if (execControl == NULL)
	{
		return 0;
	}
	
	return execControl->GetWifiBridgeDeviceIndex();
}

- (void) updateCustomMACAddressStrings
{
	CocoaDSFirmware *cdsFirmware = (CocoaDSFirmware *)[firmwarePanelController content];
	
	[self setMyMACAddress1String:[NSString stringWithFormat:@"My Address #1  %@", [cdsFirmware customMACAddress1String]]];
	[self setMyMACAddress2String:[NSString stringWithFormat:@"My Address #2  %@", [cdsFirmware customMACAddress2String]]];
	[self setMyMACAddress3String:[NSString stringWithFormat:@"My Address #3  %@", [cdsFirmware customMACAddress3String]]];
	[self setMyMACAddress4String:[NSString stringWithFormat:@"My Address #4  %@", [cdsFirmware customMACAddress4String]]];
}

- (void) fillLibpcapDeviceMenu
{
	std::vector<std::string> deviceList;
	int deviceCount = wifiHandler->GetBridgeDeviceList(&deviceList);
	
	if (deviceCount <= 0)
	{
		return;
	}
	
	[bridgeDevicePopUpButton removeAllItems];
	
	for (int i = 0; i < deviceCount; i++)
	{
		NSString *itemTitle = [NSString stringWithCString:deviceList[i].c_str() encoding:NSUTF8StringEncoding];
		[bridgeDevicePopUpButton addItemWithTitle:itemTitle];
		
		NSMenuItem *menuItem = [bridgeDevicePopUpButton itemAtIndex:i];
		[menuItem setTag:i];
	}
}

- (IBAction) generateNewAddresses:(id)sender
{
	CocoaDSFirmware *cdsFirmware = (CocoaDSFirmware *)[firmwarePanelController content];
	[cdsFirmware generateRandomCustomMACAddress];
	[self updateCustomMACAddressStrings];
}

- (IBAction) writeDefaultsWifiSettings:(id)sender
{
	CocoaDSFirmware *cdsFirmware = (CocoaDSFirmware *)[firmwarePanelController content];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[self wifiEmulationMode] forKey:@"Wifi_EmulationMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[cdsFirmware addressSelection] forKey:@"FirmwareConfig_AddressSelection"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self bridgeDeviceSelection] forKey:@"Wifi_BridgeDeviceSelectionIndex"];
	[[NSUserDefaults standardUserDefaults] setObject:_libpcapDeviceSelectionName forKey:@"Wifi_BridgeDeviceSelectionName"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) readUserDefaults
{
	CocoaDSFirmware *cdsFirmware = (CocoaDSFirmware *)[firmwarePanelController content];
	[self updateCustomMACAddressStrings];
	
	// Set the bridge device selection.
	NSInteger userDefaultDeviceIndex = [[NSUserDefaults standardUserDefaults] integerForKey:@"Wifi_BridgeDeviceSelectionIndex"];
	NSString *userDefaultDeviceName = [[NSUserDefaults standardUserDefaults] stringForKey:@"Wifi_BridgeDeviceSelectionName"];
	NSString *selectedName = [bridgeDevicePopUpButton itemTitleAtIndex:userDefaultDeviceIndex];
	
	// Check to see if the selected device name matches what we've got in user defaults.
	// If the user's hardware configuration hasn't changed in between app runs, then we
	// should get a match. But if we don't get a match, then we can't be sure if the
	// user's hardware configuration is the same. In the case of a mismatch, just revert
	// to using en0.
	if ( (userDefaultDeviceName == nil) || [userDefaultDeviceName isEqualToString:@""] || ![userDefaultDeviceName isEqualToString:selectedName])
	{
		userDefaultDeviceIndex = [bridgeDevicePopUpButton indexOfItemWithTitle:@"en0"];
		
		// en0 SHOULD be found, as all Macs since ye olden times have come equipped with
		// at least one Ethernet port. If we can't find a device with name "en0", then as
		// a last resort, just pick the first device in the list.
		if (userDefaultDeviceIndex == -1)
		{
			userDefaultDeviceIndex = 0;
		}
		
		userDefaultDeviceName = [bridgeDevicePopUpButton itemTitleAtIndex:userDefaultDeviceIndex];
		
		[[NSUserDefaults standardUserDefaults] setInteger:userDefaultDeviceIndex forKey:@"Wifi_BridgeDeviceSelectionIndex"];
		[[NSUserDefaults standardUserDefaults] setObject:userDefaultDeviceName forKey:@"Wifi_BridgeDeviceSelectionName"];
		[[NSUserDefaults standardUserDefaults] synchronize];
	}
	
	[self setBridgeDeviceSelection:userDefaultDeviceIndex];
	
	// Set the remaining user defaults.
	[self setWifiEmulationMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"Wifi_EmulationMode"]];
	[cdsFirmware setAddressSelection:[[NSUserDefaults standardUserDefaults] integerForKey:@"FirmwareConfig_AddressSelection"]];
}

@end
