/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2025 DeSmuME team

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

#import "cocoa_firmware.h"

#include "ClientFirmwareControl.h"
#include "ClientExecutionControl.h"
#include "../../firmware.h"
#undef BOOL


@implementation CocoaDSFirmware

@synthesize execControl;

@dynamic addressSelection;
@dynamic firmwareMACAddressPendingValue;
@dynamic firmwareMACAddressValue;
@synthesize firmwareMACAddressPendingString;
@synthesize firmwareMACAddressString;
@dynamic customMACAddressValue;
@synthesize customMACAddress1String;
@synthesize customMACAddress2String;
@synthesize customMACAddress3String;
@synthesize customMACAddress4String;

@dynamic ipv4Address_AP1_1;
@dynamic ipv4Address_AP1_2;
@dynamic ipv4Address_AP1_3;
@dynamic ipv4Address_AP1_4;
@dynamic ipv4Gateway_AP1_1;
@dynamic ipv4Gateway_AP1_2;
@dynamic ipv4Gateway_AP1_3;
@dynamic ipv4Gateway_AP1_4;
@dynamic ipv4PrimaryDNS_AP1_1;
@dynamic ipv4PrimaryDNS_AP1_2;
@dynamic ipv4PrimaryDNS_AP1_3;
@dynamic ipv4PrimaryDNS_AP1_4;
@dynamic ipv4SecondaryDNS_AP1_1;
@dynamic ipv4SecondaryDNS_AP1_2;
@dynamic ipv4SecondaryDNS_AP1_3;
@dynamic ipv4SecondaryDNS_AP1_4;
@dynamic subnetMask_AP1;
@synthesize subnetMaskString_AP1;

@dynamic ipv4Address_AP2_1;
@dynamic ipv4Address_AP2_2;
@dynamic ipv4Address_AP2_3;
@dynamic ipv4Address_AP2_4;
@dynamic ipv4Gateway_AP2_1;
@dynamic ipv4Gateway_AP2_2;
@dynamic ipv4Gateway_AP2_3;
@dynamic ipv4Gateway_AP2_4;
@dynamic ipv4PrimaryDNS_AP2_1;
@dynamic ipv4PrimaryDNS_AP2_2;
@dynamic ipv4PrimaryDNS_AP2_3;
@dynamic ipv4PrimaryDNS_AP2_4;
@dynamic ipv4SecondaryDNS_AP2_1;
@dynamic ipv4SecondaryDNS_AP2_2;
@dynamic ipv4SecondaryDNS_AP2_3;
@dynamic ipv4SecondaryDNS_AP2_4;
@dynamic subnetMask_AP2;
@synthesize subnetMaskString_AP2;

@dynamic ipv4Address_AP3_1;
@dynamic ipv4Address_AP3_2;
@dynamic ipv4Address_AP3_3;
@dynamic ipv4Address_AP3_4;
@dynamic ipv4Gateway_AP3_1;
@dynamic ipv4Gateway_AP3_2;
@dynamic ipv4Gateway_AP3_3;
@dynamic ipv4Gateway_AP3_4;
@dynamic ipv4PrimaryDNS_AP3_1;
@dynamic ipv4PrimaryDNS_AP3_2;
@dynamic ipv4PrimaryDNS_AP3_3;
@dynamic ipv4PrimaryDNS_AP3_4;
@dynamic ipv4SecondaryDNS_AP3_1;
@dynamic ipv4SecondaryDNS_AP3_2;
@dynamic ipv4SecondaryDNS_AP3_3;
@dynamic ipv4SecondaryDNS_AP3_4;
@dynamic subnetMask_AP3;
@synthesize subnetMaskString_AP3;

@dynamic consoleType;
@dynamic nickname;
@dynamic message;
@dynamic favoriteColor;
@dynamic birthday;
@dynamic language;
@dynamic backlightLevel;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	execControl = NULL;
	_fwControl = new ClientFirmwareControl;
	_appliedFWConfig = (FirmwareConfig *)malloc(sizeof(FirmwareConfig));
	_birth_year = 1970;
	
	firmwareMACAddressString = [[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Firmware) encoding:NSUTF8StringEncoding] retain];
	firmwareMACAddressPendingString = [[firmwareMACAddressString copy] retain];
	customMACAddress1String  = [[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom1)  encoding:NSUTF8StringEncoding] retain];
	customMACAddress2String  = [[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom2)  encoding:NSUTF8StringEncoding] retain];
	customMACAddress3String  = [[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom3)  encoding:NSUTF8StringEncoding] retain];
	customMACAddress4String  = [[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom4)  encoding:NSUTF8StringEncoding] retain];
	
	subnetMaskString_AP1 = [[NSString stringWithCString:_fwControl->GetSubnetMaskString(FirmwareCfgAPID_AP1)  encoding:NSUTF8StringEncoding] retain];
	subnetMaskString_AP2 = [[NSString stringWithCString:_fwControl->GetSubnetMaskString(FirmwareCfgAPID_AP2)  encoding:NSUTF8StringEncoding] retain];
	subnetMaskString_AP3 = [[NSString stringWithCString:_fwControl->GetSubnetMaskString(FirmwareCfgAPID_AP3)  encoding:NSUTF8StringEncoding] retain];
	
	return self;
}

- (void)dealloc
{
	[self setFirmwareMACAddressString:nil];
	[self setFirmwareMACAddressPendingString:nil];
	[self setCustomMACAddress1String:nil];
	[self setCustomMACAddress2String:nil];
	[self setCustomMACAddress3String:nil];
	[self setCustomMACAddress4String:nil];
	
	[self setSubnetMaskString_AP1:nil];
	[self setSubnetMaskString_AP2:nil];
	[self setSubnetMaskString_AP3:nil];
	
	delete _fwControl;
	free(_appliedFWConfig);
	
	[super dealloc];
}

- (void) writeUserDefaultWFCUserID
{
	uint8_t macAddress[6];
	uint8_t wfcUserID[6];
	uint64_t wfcUserIDValue = 0;
	
	NDS_GetCurrentWFCUserID(macAddress, wfcUserID);
	
	wfcUserIDValue = (uint64_t)wfcUserID[0] |
	                ((uint64_t)wfcUserID[1] <<  8) |
	                ((uint64_t)wfcUserID[2] << 16) |
	                ((uint64_t)wfcUserID[3] << 24) |
	                ((uint64_t)wfcUserID[4] << 32) |
	                ((uint64_t)wfcUserID[5] << 40);
	
	if ( wfcUserIDValue != 0 && ((wfcUserIDValue & 0x000007FFFFFFFFFFULL) == 0) )
	{
		wfcUserIDValue = 0;
	}
	
	NSString *wfcMACString = [NSString stringWithFormat:@"%02X:%02X:%02X:%02X:%02X:%02X",
							  macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]];
	
	NSNumber *wfcUserIDNumber = [NSNumber numberWithUnsignedLongLong:(unsigned long long)wfcUserIDValue];
	NSDictionary *wfcUserIDDict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"FirmwareConfig_WFCUserID"];
	NSMutableDictionary *outDictionary = nil;
	
	if (wfcUserIDDict == nil)
	{
		outDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:wfcUserIDNumber, wfcMACString, nil];
	}
	else
	{
		outDictionary = [NSMutableDictionary dictionaryWithDictionary:wfcUserIDDict];
		[outDictionary setObject:wfcUserIDNumber forKey:wfcMACString];
	}
	
	[[NSUserDefaults standardUserDefaults] setObject:outDictionary forKey:@"FirmwareConfig_WFCUserID"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) setAddressSelection:(NSInteger)selection
{
	_fwControl->SetMACAddressSelection((FirmwareCfgMACAddrSetID)selection);
}

- (NSInteger) addressSelection
{
	return (NSInteger)_fwControl->GetMACAddressSelection();
}

- (void) setFirmwareMACAddressPendingValue:(uint32_t)inMACAddressValue
{
	_fwControl->SetFirmwareMACAddressValue(inMACAddressValue);
	[self setFirmwareMACAddressPendingString:[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Firmware) encoding:NSUTF8StringEncoding]];
}

- (uint32_t) firmwareMACAddressPendingValue
{
	return _fwControl->GetMACAddressValue(FirmwareCfgMACAddrSetID_Firmware);
}

- (uint32_t) firmwareMACAddressValue
{
	const uint32_t macAddressValue =  (uint32_t)_appliedFWConfig->MACAddress[2]        |
	                                 ((uint32_t)_appliedFWConfig->MACAddress[3] <<  8) |
	                                 ((uint32_t)_appliedFWConfig->MACAddress[4] << 16) |
	                                 ((uint32_t)_appliedFWConfig->MACAddress[5] << 24);
	
	return macAddressValue;
}

- (void) setCustomMACAddressValue:(uint32_t)inMACAddressValue
{
	_fwControl->SetCustomMACAddressValue(inMACAddressValue);
	[self setCustomMACAddress1String:[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom1) encoding:NSUTF8StringEncoding]];
	[self setCustomMACAddress2String:[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom2) encoding:NSUTF8StringEncoding]];
	[self setCustomMACAddress3String:[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom3) encoding:NSUTF8StringEncoding]];
	[self setCustomMACAddress4String:[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom4) encoding:NSUTF8StringEncoding]];
}

- (uint32_t) customMACAddressValue
{
	return (_fwControl->GetMACAddressValue(FirmwareCfgMACAddrSetID_Custom1) - 1);
}

- (void) setIpv4Address_AP1_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_1
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4Address_AP1_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_2
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4Address_AP1_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_3
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4Address_AP1_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_4
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setIpv4Gateway_AP1_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_1
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4Gateway_AP1_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_2
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4Gateway_AP1_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_3
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4Gateway_AP1_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_4
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setIpv4PrimaryDNS_AP1_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_1
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4PrimaryDNS_AP1_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_2
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4PrimaryDNS_AP1_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_3
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4PrimaryDNS_AP1_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_4
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setIpv4SecondaryDNS_AP1_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_1
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4SecondaryDNS_AP1_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_2
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4SecondaryDNS_AP1_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_3
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4SecondaryDNS_AP1_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_4
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setSubnetMask_AP1:(NSInteger)subnetMask
{
	_fwControl->SetSubnetMask(FirmwareCfgAPID_AP1, (uint8_t)subnetMask);
	
	NSString *subnetMaskString = [NSString stringWithCString:_fwControl->GetSubnetMaskString(FirmwareCfgAPID_AP1) encoding:NSUTF8StringEncoding];
	[self setSubnetMaskString_AP1:subnetMaskString];
}

- (NSInteger) subnetMask_AP1
{
	return (NSInteger)_fwControl->GetSubnetMask(FirmwareCfgAPID_AP1);
}

- (void) setIpv4Address_AP2_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_1
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4Address_AP2_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_2
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4Address_AP2_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_3
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4Address_AP2_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_4
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setIpv4Gateway_AP2_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_1
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4Gateway_AP2_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_2
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4Gateway_AP2_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_3
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4Gateway_AP2_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_4
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setIpv4PrimaryDNS_AP2_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_1
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4PrimaryDNS_AP2_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_2
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4PrimaryDNS_AP2_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_3
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4PrimaryDNS_AP2_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_4
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setIpv4SecondaryDNS_AP2_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_1
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4SecondaryDNS_AP2_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_2
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4SecondaryDNS_AP2_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_3
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4SecondaryDNS_AP2_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_4
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setSubnetMask_AP2:(NSInteger)subnetMask
{
	_fwControl->SetSubnetMask(FirmwareCfgAPID_AP2, (uint8_t)subnetMask);
	
	NSString *subnetMaskString = [NSString stringWithCString:_fwControl->GetSubnetMaskString(FirmwareCfgAPID_AP2) encoding:NSUTF8StringEncoding];
	[self setSubnetMaskString_AP2:subnetMaskString];
}

- (NSInteger) subnetMask_AP2
{
	return (NSInteger)_fwControl->GetSubnetMask(FirmwareCfgAPID_AP2);
}

- (void) setIpv4Address_AP3_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_1
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4Address_AP3_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_2
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4Address_AP3_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_3
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4Address_AP3_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_4
{
	return (NSInteger)_fwControl->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setIpv4Gateway_AP3_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_1
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4Gateway_AP3_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_2
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4Gateway_AP3_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_3
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4Gateway_AP3_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_4
{
	return (NSInteger)_fwControl->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setIpv4PrimaryDNS_AP3_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_1
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4PrimaryDNS_AP3_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_2
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4PrimaryDNS_AP3_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_3
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4PrimaryDNS_AP3_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_4
{
	return (NSInteger)_fwControl->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setIpv4SecondaryDNS_AP3_1:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_1
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4SecondaryDNS_AP3_2:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_2
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4SecondaryDNS_AP3_3:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_3
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4SecondaryDNS_AP3_4:(NSInteger)addrValue
{
	_fwControl->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_4
{
	return (NSInteger)_fwControl->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setSubnetMask_AP3:(NSInteger)subnetMask
{
	_fwControl->SetSubnetMask(FirmwareCfgAPID_AP3, (uint8_t)subnetMask);
	
	NSString *subnetMaskString = [NSString stringWithCString:_fwControl->GetSubnetMaskString(FirmwareCfgAPID_AP3) encoding:NSUTF8StringEncoding];
	[self setSubnetMaskString_AP3:subnetMaskString];
}

- (NSInteger) subnetMask_AP3
{
	return (NSInteger)_fwControl->GetSubnetMask(FirmwareCfgAPID_AP3);
}

- (void) setConsoleType:(NSInteger)theType
{
	_fwControl->SetConsoleType((int)theType);
}

- (NSInteger) consoleType
{
	return (NSInteger)_fwControl->GetConsoleType();
}

- (void) setNickname:(NSString *)theNickname
{
	if (theNickname != nil)
	{
		NSRange characterRange = {0, [theNickname length]};
		
		if (characterRange.length > MAX_FW_NICKNAME_LENGTH)
		{
			characterRange.length = MAX_FW_NICKNAME_LENGTH;
		}
		
		[theNickname getBytes:_fwControl->GetNicknameStringBuffer()
					maxLength:(sizeof(uint16_t) * characterRange.length)
				   usedLength:NULL
					 encoding:NSUTF16LittleEndianStringEncoding
					  options:NSStringEncodingConversionAllowLossy
						range:characterRange
			   remainingRange:NULL];
		
		_fwControl->SetNicknameStringLength((uint8_t)characterRange.length);
	}
	else
	{
		_fwControl->SetNicknameWithStringBuffer(NULL, 0);
	}
}

- (NSString *) nickname
{
	return [[[NSString alloc] initWithBytes:_fwControl->GetNicknameStringBuffer() length:(sizeof(uint16_t) * _fwControl->GetNicknameStringLength()) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
}

- (void) setMessage:(NSString *)theMessage
{
	if (theMessage != nil)
	{
		NSRange characterRange = {0, [theMessage length]};
		
		if (characterRange.length > MAX_FW_MESSAGE_LENGTH)
		{
			characterRange.length = MAX_FW_MESSAGE_LENGTH;
		}
		
		[theMessage getBytes:_fwControl->GetMessageStringBuffer()
				   maxLength:(sizeof(uint16_t) * characterRange.length)
				  usedLength:NULL
					encoding:NSUTF16LittleEndianStringEncoding
					 options:NSStringEncodingConversionAllowLossy
					   range:characterRange
			  remainingRange:NULL];
		
		_fwControl->SetMessageStringLength((uint8_t)characterRange.length);
	}
	else
	{
		_fwControl->SetMessageWithStringBuffer(NULL, 0);
	}
}

- (NSString *) message
{
	return [[[NSString alloc] initWithBytes:_fwControl->GetMessageStringBuffer() length:(sizeof(uint16_t) * _fwControl->GetMessageStringLength()) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
}

- (void) setFavoriteColor:(NSInteger)colorID
{
	_fwControl->SetFavoriteColorByID((int)colorID);
}

- (NSInteger) favoriteColor
{
	return (NSInteger)_fwControl->GetFavoriteColorByID();
}

- (void) setBirthday:(NSDate *)theDate
{
	int theMonth = 1;
	int theDay   = 1;
	int theYear  = 1970;
	
	if (theDate != nil)
	{
		NSDateFormatter *df = [[NSDateFormatter alloc] init];
		[df setDateFormat:@"Y M d"];
		
		NSString *dateString = [df stringFromDate:theDate];
		sscanf([dateString cStringUsingEncoding:NSUTF8StringEncoding], "%i %i %i", &theYear, &theMonth, &theDay);
		[df release];
	}
	
	_fwControl->SetBirthdayMonth(theMonth);
	_fwControl->SetBirthdayDay(theDay);
	_birth_year = theYear;
}

- (NSDate *) birthday
{
	int theMonth = _fwControl->GetBirthdayMonth();
	int theDay   = _fwControl->GetBirthdayDay();
	int theYear  = (int)_birth_year;
	
	NSDateFormatter *df = [[NSDateFormatter alloc] init];
	[df setDateFormat:@"YYYY-MM-dd HH:mm:ss Z"];
	
	NSString *dateString = [NSString stringWithFormat:@"%i-%i-%i 12:00:00 +0000", theYear, theMonth, theDay];
	NSDate *birthdayDate = [df dateFromString:dateString];
	[df release];
	
	return birthdayDate;
}

- (void) setLanguage:(NSInteger)languageID
{
	_fwControl->SetLanguageByID((int)languageID);
}

- (NSInteger) language
{
	return (NSInteger)_fwControl->GetLanguageByID();
}

- (void) setBacklightLevel:(NSInteger)level
{
	_fwControl->SetBacklightLevel((int)level);
}

- (NSInteger) backlightLevel
{
	return (NSInteger)_fwControl->GetBacklightLevel();
}

- (void) applySettings
{
	*_appliedFWConfig = _fwControl->GetFirmwareConfig();
	[self setFirmwareMACAddressString:[NSString stringWithCString:_fwControl->GetMACAddressString(FirmwareCfgMACAddrSetID_Firmware) encoding:NSUTF8StringEncoding]];
}

- (void) updateFirmwareConfigSessionValues
{
	NSNumber *wfcUserIDNumber = nil;
	NSDictionary *wfcUserIDDict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"FirmwareConfig_WFCUserID"];
	const FirmwareCfgMACAddrSetID selectionID = _fwControl->GetMACAddressSelection();
	
	uint32_t macAddressValue = 0;
	if (selectionID == FirmwareCfgMACAddrSetID_Firmware)
	{
		macAddressValue = (uint32_t)_appliedFWConfig->MACAddress[2] |
		                 ((uint32_t)_appliedFWConfig->MACAddress[3] <<  8) |
		                 ((uint32_t)_appliedFWConfig->MACAddress[4] << 16) |
		                 ((uint32_t)_appliedFWConfig->MACAddress[5] << 24);
	}
	else
	{
		macAddressValue = _fwControl->GetMACAddressValue(selectionID);
	}
	
	if ( (wfcUserIDDict != nil) && ([wfcUserIDDict count] > 0) )
	{
		if ( (macAddressValue != 0) && (macAddressValue != 0x000000BF) )
		{
			NSString *macAddressValueString = nil;
			
			if (selectionID == FirmwareCfgMACAddrSetID_Firmware)
			{
				macAddressValueString = [NSString stringWithFormat:@"%02X:%02X:%02X:%02X:%02X:%02X",
										 _appliedFWConfig->MACAddress[0], _appliedFWConfig->MACAddress[1], _appliedFWConfig->MACAddress[2],
										 _appliedFWConfig->MACAddress[3], _appliedFWConfig->MACAddress[4], _appliedFWConfig->MACAddress[5]];
			}
			else
			{
				macAddressValueString = [NSString stringWithCString:_fwControl->GetSelectedMACAddressString() encoding:NSUTF8StringEncoding];
			}
			
			wfcUserIDNumber = (NSNumber *)[wfcUserIDDict objectForKey:macAddressValueString];
		}
	}
	
	const uint64_t wfcUserIDValue = (wfcUserIDNumber == nil) ? 0 : (uint64_t)[wfcUserIDNumber unsignedLongLongValue];
	_fwControl->SetWFCUserID64(wfcUserIDValue);
	
	FirmwareConfig finalConfig = *_appliedFWConfig;
	
	finalConfig.MACAddress[0] = 0x00;
	finalConfig.MACAddress[1] = 0x09;
	finalConfig.MACAddress[2] = (uint8_t)( macAddressValue        & 0x000000FF);
	finalConfig.MACAddress[3] = (uint8_t)((macAddressValue >>  8) & 0x000000FF);
	finalConfig.MACAddress[4] = (uint8_t)((macAddressValue >> 16) & 0x000000FF);
	finalConfig.MACAddress[5] = (uint8_t)((macAddressValue >> 24) & 0x000000FF);
	
	finalConfig.WFCUserID[0] = (uint8_t)( wfcUserIDValue        & 0x00000000000000FFULL);
	finalConfig.WFCUserID[1] = (uint8_t)((wfcUserIDValue >>  8) & 0x00000000000000FFULL);
	finalConfig.WFCUserID[2] = (uint8_t)((wfcUserIDValue >> 16) & 0x00000000000000FFULL);
	finalConfig.WFCUserID[3] = (uint8_t)((wfcUserIDValue >> 24) & 0x00000000000000FFULL);
	finalConfig.WFCUserID[4] = (uint8_t)((wfcUserIDValue >> 32) & 0x00000000000000FFULL);
	finalConfig.WFCUserID[5] = (uint8_t)((wfcUserIDValue >> 40) & 0x00000000000000FFULL);
	
	execControl->SetFirmwareConfig(finalConfig);
}

- (uint32_t) generateRandomFirmwareMACAddress
{
	const uint32_t randomMACAddressValue = _fwControl->GenerateRandomMACValue();
	[self setFirmwareMACAddressPendingValue:randomMACAddressValue];
	
	return randomMACAddressValue;
}

- (uint32_t) generateRandomCustomMACAddress
{
	const uint32_t randomMACAddressValue = _fwControl->GenerateRandomMACValue();
	[self setCustomMACAddressValue:randomMACAddressValue];
	
	return randomMACAddressValue;
}

@end
