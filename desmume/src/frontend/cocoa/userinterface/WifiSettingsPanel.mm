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
#include "../ClientExecutionControl.h"
#include "../../../wifi.h"
#undef BOOL


@implementation WifiSettingsPanelDelegate

@synthesize execControl;

@dynamic wifiEmulationMode;
@dynamic addressSelection;
@dynamic bridgeDeviceSelection;

@synthesize bridgeDevicePopUpButton;

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
	
	_myMACAddressValue = 0x503412BF;
	const uint8_t myMAC4 = (_myMACAddressValue >>  8) & 0x000000FF;
	const uint8_t myMAC5 = (_myMACAddressValue >> 16) & 0x000000FF;
	const uint8_t myMAC6 = (_myMACAddressValue >> 24) & 0x000000F0;
	
	myMACAddress1String = [NSString stringWithFormat:@"My Address #1  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 1];
	myMACAddress2String = [NSString stringWithFormat:@"My Address #2  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 2];
	myMACAddress3String = [NSString stringWithFormat:@"My Address #3  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 3];
	myMACAddress4String = [NSString stringWithFormat:@"My Address #4  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 4];
	
	addressSelection = 0;
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

- (void) setAddressSelection:(NSInteger)theSelection
{
	if (theSelection == 0)
	{
		//wifiHandler->SetFirmwareMACMode(FirmwareMACMode_ReadFromFirmware);
		wifiHandler->SetMACModeForComm(WifiCommInterfaceID_AdHoc, WifiMACMode_ReadFromFirmware);
		wifiHandler->SetMACModeForComm(WifiCommInterfaceID_Infrastructure, WifiMACMode_ReadFromFirmware);
	}
	else if ( (theSelection >= 1) && (theSelection <= 4) )
	{
		const uint8_t myMAC4 = (_myMACAddressValue >>  8) & 0x000000FF;
		const uint8_t myMAC5 = (_myMACAddressValue >> 16) & 0x000000FF;
		const uint8_t myMAC6 = (_myMACAddressValue >> 24) & 0x000000F0;
		
		wifiHandler->SetUserMACValues(myMAC4, myMAC5, myMAC6 + theSelection);
		//wifiHandler->SetFirmwareMACMode(FirmwareMACMode_Manual);
		wifiHandler->SetMACModeForComm(WifiCommInterfaceID_AdHoc, WifiMACMode_Manual);
		wifiHandler->SetMACModeForComm(WifiCommInterfaceID_Infrastructure, WifiMACMode_Manual);
	}
	
	addressSelection = theSelection;
}

- (NSInteger) addressSelection
{
	return addressSelection;
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

- (void) updateMyMACAddressStringsWithValue:(uint32_t)value
{
	const uint8_t myMAC4 = (value >>  8) & 0x000000FF;
	const uint8_t myMAC5 = (value >> 16) & 0x000000FF;
	const uint8_t myMAC6 = (value >> 24) & 0x000000F0;
	
	[self setMyMACAddress1String:[NSString stringWithFormat:@"My Address #1  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 1]];
	[self setMyMACAddress2String:[NSString stringWithFormat:@"My Address #2  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 2]];
	[self setMyMACAddress3String:[NSString stringWithFormat:@"My Address #3  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 3]];
	[self setMyMACAddress4String:[NSString stringWithFormat:@"My Address #4  00:09:BF:%02X:%02X:%02X", myMAC4, myMAC5, myMAC6 + 4]];
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
	
	if (deviceCount > 0)
	{
		[self setAddressSelection:0];
	}
}

- (void) generateRandomMyMACAddressSet
{
	uint32_t randomMACAddressValue = 0;
	
	do
	{
		randomMACAddressValue = (uint32_t)random() & 0x00FFFFFF;
	} while (randomMACAddressValue == 0);
	
	_myMACAddressValue = (randomMACAddressValue << 8) | 0xBF;
	[self updateMyMACAddressStringsWithValue:_myMACAddressValue];
	
	const NSInteger currentAddressSelection = [self addressSelection];
	if (currentAddressSelection != 0)
	{
		[self setAddressSelection:currentAddressSelection];
	}
}

- (IBAction) generateNewAddresses:(id)sender
{
	[self generateRandomMyMACAddressSet];
}

- (IBAction) writeDefaultsWifiSettings:(id)sender
{
	[[NSUserDefaults standardUserDefaults] setInteger:[self wifiEmulationMode] forKey:@"Wifi_EmulationMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self addressSelection] forKey:@"Wifi_AddressSelection"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self bridgeDeviceSelection] forKey:@"Wifi_BridgeDeviceSelectionIndex"];
	[[NSUserDefaults standardUserDefaults] setObject:_libpcapDeviceSelectionName forKey:@"Wifi_BridgeDeviceSelectionName"];
	[[NSUserDefaults standardUserDefaults] setInteger:_myMACAddressValue forKey:@"Wifi_MyMACAddress"];
	
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) readUserDefaults
{
	BOOL needUserDefaultSynchronize = NO;
	
	// Set the user's MAC address set.
	// This needs to happen first since the other properties will have a dependency on this.
	uint32_t userDefaultMACAddressValue = 0;
	
	if ([[NSUserDefaults standardUserDefaults] objectForKey:@"Wifi_MyMACAddress"] != nil)
	{
		userDefaultMACAddressValue = (uint32_t)[[NSUserDefaults standardUserDefaults] integerForKey:@"Wifi_MyMACAddress"];
	}
	
	if (userDefaultMACAddressValue == 0)
	{
		// Generate a new random MAC address set if one does not exist.
		do
		{
			userDefaultMACAddressValue = (uint32_t)random() & 0x00FFFFFF;
		} while (userDefaultMACAddressValue == 0);
		
		userDefaultMACAddressValue = ((userDefaultMACAddressValue << 8) | 0xBF);
		
		[[NSUserDefaults standardUserDefaults] setInteger:userDefaultMACAddressValue forKey:@"Wifi_MyMACAddress"];
		needUserDefaultSynchronize = YES;
	}
	
	_myMACAddressValue = userDefaultMACAddressValue;
	[self updateMyMACAddressStringsWithValue:_myMACAddressValue];
	
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
		needUserDefaultSynchronize = YES;
	}
	
	[self setBridgeDeviceSelection:userDefaultDeviceIndex];
	
	// Set the remaining user defaults.
	[self setWifiEmulationMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"Wifi_EmulationMode"]];
	[self setAddressSelection:[[NSUserDefaults standardUserDefaults] integerForKey:@"Wifi_AddressSelection"]];
	
	// If we wrote to user defaults beforehand, then synchronize the write as the final operation here.
	if (needUserDefaultSynchronize)
	{
		[[NSUserDefaults standardUserDefaults] synchronize];
	}
}

@end
