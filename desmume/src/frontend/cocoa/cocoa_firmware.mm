/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2018 DeSmuME team

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
#include "ClientExecutionControl.h"
#include "../../NDSSystem.h"
#include "../../firmware.h"
#include "../../MMU.h"
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
	_fwConfigInterface = new FirmwareConfigInterface;
	_appliedFWConfig = (FirmwareConfig *)malloc(sizeof(FirmwareConfig));
	_birth_year = 1970;
	
	firmwareMACAddressString = [[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Firmware) encoding:NSUTF8StringEncoding] retain];
	firmwareMACAddressPendingString = [[firmwareMACAddressString copy] retain];
	customMACAddress1String  = [[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom1)  encoding:NSUTF8StringEncoding] retain];
	customMACAddress2String  = [[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom2)  encoding:NSUTF8StringEncoding] retain];
	customMACAddress3String  = [[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom3)  encoding:NSUTF8StringEncoding] retain];
	customMACAddress4String  = [[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom4)  encoding:NSUTF8StringEncoding] retain];
	
	subnetMaskString_AP1 = [[NSString stringWithCString:_fwConfigInterface->GetSubnetMaskString(FirmwareCfgAPID_AP1)  encoding:NSUTF8StringEncoding] retain];
	subnetMaskString_AP2 = [[NSString stringWithCString:_fwConfigInterface->GetSubnetMaskString(FirmwareCfgAPID_AP2)  encoding:NSUTF8StringEncoding] retain];
	subnetMaskString_AP3 = [[NSString stringWithCString:_fwConfigInterface->GetSubnetMaskString(FirmwareCfgAPID_AP3)  encoding:NSUTF8StringEncoding] retain];
	
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
	
	delete _fwConfigInterface;
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
	_fwConfigInterface->SetMACAddressSelection((FirmwareCfgMACAddrSetID)selection);
}

- (NSInteger) addressSelection
{
	return (NSInteger)_fwConfigInterface->GetMACAddressSelection();
}

- (void) setFirmwareMACAddressPendingValue:(uint32_t)inMACAddressValue
{
	_fwConfigInterface->SetFirmwareMACAddressValue(inMACAddressValue);
	[self setFirmwareMACAddressPendingString:[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Firmware) encoding:NSUTF8StringEncoding]];
}

- (uint32_t) firmwareMACAddressPendingValue
{
	return _fwConfigInterface->GetMACAddressValue(FirmwareCfgMACAddrSetID_Firmware);
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
	_fwConfigInterface->SetCustomMACAddressValue(inMACAddressValue);
	[self setCustomMACAddress1String:[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom1) encoding:NSUTF8StringEncoding]];
	[self setCustomMACAddress2String:[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom2) encoding:NSUTF8StringEncoding]];
	[self setCustomMACAddress3String:[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom3) encoding:NSUTF8StringEncoding]];
	[self setCustomMACAddress4String:[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Custom4) encoding:NSUTF8StringEncoding]];
}

- (uint32_t) customMACAddressValue
{
	return (_fwConfigInterface->GetMACAddressValue(FirmwareCfgMACAddrSetID_Custom1) - 1);
}

- (void) setIpv4Address_AP1_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4Address_AP1_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4Address_AP1_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4Address_AP1_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP1_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setIpv4Gateway_AP1_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4Gateway_AP1_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4Gateway_AP1_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4Gateway_AP1_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP1_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setIpv4PrimaryDNS_AP1_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4PrimaryDNS_AP1_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4PrimaryDNS_AP1_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4PrimaryDNS_AP1_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP1_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setIpv4SecondaryDNS_AP1_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 0);
}

- (void) setIpv4SecondaryDNS_AP1_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 1);
}

- (void) setIpv4SecondaryDNS_AP1_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 2);
}

- (void) setIpv4SecondaryDNS_AP1_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP1_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP1, 3);
}

- (void) setSubnetMask_AP1:(NSInteger)subnetMask
{
	_fwConfigInterface->SetSubnetMask(FirmwareCfgAPID_AP1, (uint8_t)subnetMask);
	
	NSString *subnetMaskString = [NSString stringWithCString:_fwConfigInterface->GetSubnetMaskString(FirmwareCfgAPID_AP1) encoding:NSUTF8StringEncoding];
	[self setSubnetMaskString_AP1:subnetMaskString];
}

- (NSInteger) subnetMask_AP1
{
	return (NSInteger)_fwConfigInterface->GetSubnetMask(FirmwareCfgAPID_AP1);
}

- (void) setIpv4Address_AP2_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4Address_AP2_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4Address_AP2_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4Address_AP2_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP2_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setIpv4Gateway_AP2_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4Gateway_AP2_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4Gateway_AP2_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4Gateway_AP2_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP2_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setIpv4PrimaryDNS_AP2_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4PrimaryDNS_AP2_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4PrimaryDNS_AP2_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4PrimaryDNS_AP2_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP2_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setIpv4SecondaryDNS_AP2_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 0);
}

- (void) setIpv4SecondaryDNS_AP2_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 1);
}

- (void) setIpv4SecondaryDNS_AP2_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 2);
}

- (void) setIpv4SecondaryDNS_AP2_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP2_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP2, 3);
}

- (void) setSubnetMask_AP2:(NSInteger)subnetMask
{
	_fwConfigInterface->SetSubnetMask(FirmwareCfgAPID_AP2, (uint8_t)subnetMask);
	
	NSString *subnetMaskString = [NSString stringWithCString:_fwConfigInterface->GetSubnetMaskString(FirmwareCfgAPID_AP2) encoding:NSUTF8StringEncoding];
	[self setSubnetMaskString_AP2:subnetMaskString];
}

- (NSInteger) subnetMask_AP2
{
	return (NSInteger)_fwConfigInterface->GetSubnetMask(FirmwareCfgAPID_AP2);
}

- (void) setIpv4Address_AP3_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4Address_AP3_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4Address_AP3_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4Address_AP3_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4AddressPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Address_AP3_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4AddressPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setIpv4Gateway_AP3_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4Gateway_AP3_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4Gateway_AP3_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4Gateway_AP3_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4GatewayPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4Gateway_AP3_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4GatewayPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setIpv4PrimaryDNS_AP3_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4PrimaryDNS_AP3_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4PrimaryDNS_AP3_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4PrimaryDNS_AP3_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4PrimaryDNS_AP3_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4PrimaryDNSPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setIpv4SecondaryDNS_AP3_1:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 0, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_1
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 0);
}

- (void) setIpv4SecondaryDNS_AP3_2:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 1, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_2
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 1);
}

- (void) setIpv4SecondaryDNS_AP3_3:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 2, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_3
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 2);
}

- (void) setIpv4SecondaryDNS_AP3_4:(NSInteger)addrValue
{
	_fwConfigInterface->SetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 3, (uint8_t)addrValue);
}

- (NSInteger) ipv4SecondaryDNS_AP3_4
{
	return (NSInteger)_fwConfigInterface->GetIPv4SecondaryDNSPart(FirmwareCfgAPID_AP3, 3);
}

- (void) setSubnetMask_AP3:(NSInteger)subnetMask
{
	_fwConfigInterface->SetSubnetMask(FirmwareCfgAPID_AP3, (uint8_t)subnetMask);
	
	NSString *subnetMaskString = [NSString stringWithCString:_fwConfigInterface->GetSubnetMaskString(FirmwareCfgAPID_AP3) encoding:NSUTF8StringEncoding];
	[self setSubnetMaskString_AP3:subnetMaskString];
}

- (NSInteger) subnetMask_AP3
{
	return (NSInteger)_fwConfigInterface->GetSubnetMask(FirmwareCfgAPID_AP3);
}

- (void) setConsoleType:(NSInteger)theType
{
	_fwConfigInterface->SetConsoleType((int)theType);
}

- (NSInteger) consoleType
{
	return (NSInteger)_fwConfigInterface->GetConsoleType();
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
		
		[theNickname getBytes:_fwConfigInterface->GetNicknameStringBuffer()
					maxLength:(sizeof(uint16_t) * characterRange.length)
				   usedLength:NULL
					 encoding:NSUTF16LittleEndianStringEncoding
					  options:NSStringEncodingConversionAllowLossy
						range:characterRange
			   remainingRange:NULL];
		
		_fwConfigInterface->SetNicknameStringLength((uint8_t)characterRange.length);
	}
	else
	{
		_fwConfigInterface->SetNicknameWithStringBuffer(NULL, 0);
	}
}

- (NSString *) nickname
{
	return [[[NSString alloc] initWithBytes:_fwConfigInterface->GetNicknameStringBuffer() length:(sizeof(uint16_t) * _fwConfigInterface->GetNicknameStringLength()) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
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
		
		[theMessage getBytes:_fwConfigInterface->GetMessageStringBuffer()
				   maxLength:(sizeof(uint16_t) * characterRange.length)
				  usedLength:NULL
					encoding:NSUTF16LittleEndianStringEncoding
					 options:NSStringEncodingConversionAllowLossy
					   range:characterRange
			  remainingRange:NULL];
		
		_fwConfigInterface->SetMessageStringLength((uint8_t)characterRange.length);
	}
	else
	{
		_fwConfigInterface->SetMessageWithStringBuffer(NULL, 0);
	}
}

- (NSString *) message
{
	return [[[NSString alloc] initWithBytes:_fwConfigInterface->GetMessageStringBuffer() length:(sizeof(uint16_t) * _fwConfigInterface->GetMessageStringLength()) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
}

- (void) setFavoriteColor:(NSInteger)colorID
{
	_fwConfigInterface->SetFavoriteColorByID((int)colorID);
}

- (NSInteger) favoriteColor
{
	return (NSInteger)_fwConfigInterface->GetFavoriteColorByID();
}

- (void) setBirthday:(NSDate *)theDate
{
	if (theDate != nil)
	{
		NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
		
		[dateFormatter setDateFormat:@"M"];
		NSInteger theMonth = [[dateFormatter stringFromDate:theDate] integerValue];
		
		[dateFormatter setDateFormat:@"d"];
		NSInteger theDay = [[dateFormatter stringFromDate:theDate] integerValue];
		
		[dateFormatter setDateFormat:@"Y"];
		NSInteger theYear = [[dateFormatter stringFromDate:theDate] integerValue];
		
		[dateFormatter release];
		
		_fwConfigInterface->SetBirthdayMonth((uint8_t)theMonth);
		_fwConfigInterface->SetBirthdayDay((uint8_t)theDay);
		_birth_year = theYear;
	}
	else
	{
		_fwConfigInterface->SetBirthdayMonth(1);
		_fwConfigInterface->SetBirthdayDay(1);
		_birth_year = 1970;
	}
}

- (NSDate *) birthday
{
	return [NSDate dateWithString:[NSString stringWithFormat:@"%ld-%ld-%ld 12:00:00 +0000", (unsigned long)_birth_year, (unsigned long)_fwConfigInterface->GetBirthdayMonth(), (unsigned long)_fwConfigInterface->GetBirthdayDay()]];
}

- (void) setLanguage:(NSInteger)languageID
{
	_fwConfigInterface->SetLanguageByID((int)languageID);
}

- (NSInteger) language
{
	return (NSInteger)_fwConfigInterface->GetLanguageByID();
}

- (void) setBacklightLevel:(NSInteger)level
{
	_fwConfigInterface->SetBacklightLevel((int)level);
}

- (NSInteger) backlightLevel
{
	return (NSInteger)_fwConfigInterface->GetBacklightLevel();
}

- (void) applySettings
{
	*_appliedFWConfig = _fwConfigInterface->GetFirmwareConfig();
	[self setFirmwareMACAddressString:[NSString stringWithCString:_fwConfigInterface->GetMACAddressString(FirmwareCfgMACAddrSetID_Firmware) encoding:NSUTF8StringEncoding]];
}

- (void) updateFirmwareConfigSessionValues
{
	NSNumber *wfcUserIDNumber = nil;
	NSDictionary *wfcUserIDDict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"FirmwareConfig_WFCUserID"];
	const FirmwareCfgMACAddrSetID selectionID = _fwConfigInterface->GetMACAddressSelection();
	
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
		macAddressValue = _fwConfigInterface->GetMACAddressValue(selectionID);
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
				macAddressValueString = [NSString stringWithCString:_fwConfigInterface->GetSelectedMACAddressString() encoding:NSUTF8StringEncoding];
			}
			
			wfcUserIDNumber = (NSNumber *)[wfcUserIDDict objectForKey:macAddressValueString];
		}
	}
	
	const uint64_t wfcUserIDValue = (wfcUserIDNumber == nil) ? 0 : (uint64_t)[wfcUserIDNumber unsignedLongLongValue];
	_fwConfigInterface->SetWFCUserID64(wfcUserIDValue);
	
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
	const uint32_t randomMACAddressValue = _fwConfigInterface->GenerateRandomMACValue();
	[self setFirmwareMACAddressPendingValue:randomMACAddressValue];
	
	return randomMACAddressValue;
}

- (uint32_t) generateRandomCustomMACAddress
{
	const uint32_t randomMACAddressValue = _fwConfigInterface->GenerateRandomMACValue();
	[self setCustomMACAddressValue:randomMACAddressValue];
	
	return randomMACAddressValue;
}

@end

FirmwareConfigInterface::FirmwareConfigInterface()
{
	_macAddressSelection = FirmwareCfgMACAddrSetID_Firmware;
	
	// First, get the default firmware config.
	_internalData = (FirmwareConfig *)malloc(sizeof(FirmwareConfig));
	NDS_GetDefaultFirmwareConfig(*_internalData);
	
	srand(time(NULL));
	
	// Generate a random firmware MAC address and its associated string.
	const uint32_t defaultFirmwareMACAddressValue = (uint32_t)_internalData->MACAddress[2] | ((uint32_t)_internalData->MACAddress[3] << 8) | ((uint32_t)_internalData->MACAddress[4] << 16) | ((uint32_t)_internalData->MACAddress[5] << 24);
	
	do
	{
		_firmwareMACAddressValue = (uint32_t)rand() & 0x00FFFFFF;
		_firmwareMACAddressValue = (_firmwareMACAddressValue << 8) | 0x000000BF;
	} while ( (_firmwareMACAddressValue == 0x000000BF) || (_firmwareMACAddressValue == 0xFFFFFFBF) || (_firmwareMACAddressValue == defaultFirmwareMACAddressValue) );
	
	_internalData->MACAddress[2] = (uint8_t)( _firmwareMACAddressValue        & 0x000000FF);
	_internalData->MACAddress[3] = (uint8_t)((_firmwareMACAddressValue >>  8) & 0x000000FF);
	_internalData->MACAddress[4] = (uint8_t)((_firmwareMACAddressValue >> 16) & 0x000000FF);
	_internalData->MACAddress[5] = (uint8_t)((_firmwareMACAddressValue >> 24) & 0x000000FF);
	
	memset(_macAddressString, '\0', sizeof(_macAddressString));
	FirmwareConfigInterface::WriteMACAddressStringToBuffer(_internalData->MACAddress[3], _internalData->MACAddress[4], _internalData->MACAddress[5], &_macAddressString[0]);
	
	// Generate a random custom MAC address set and their associated strings.
	do
	{
		_customMACAddressValue = (uint32_t)rand() & 0x00FFFFFF;
		_customMACAddressValue = (_customMACAddressValue << 8) | 0x000000BF;
	} while ( (_customMACAddressValue == 0x000000BF) || (_customMACAddressValue == 0xFFFFFFBF) || ((_customMACAddressValue & 0xF0FFFFFF) == (_firmwareMACAddressValue & 0xF0FFFFFF)) );
	
	const uint8_t customMAC4 = (_customMACAddressValue >>  8) & 0x000000FF;
	const uint8_t customMAC5 = (_customMACAddressValue >> 16) & 0x000000FF;
	const uint8_t customMAC6 = (_customMACAddressValue >> 24) & 0x000000F0;
	
	for (size_t i = 1; i <= 8; i++)
	{
		FirmwareConfigInterface::WriteMACAddressStringToBuffer(customMAC4, customMAC5, customMAC6 + (uint8_t)i, &_macAddressString[18*i]);
	}
	
	// Generate the WFC User ID string.
	memset(_wfcUserIDString, '\0', sizeof(_wfcUserIDString));
	const uint64_t wfcUserIDValue = (uint64_t)_internalData->WFCUserID[0]        |
	                               ((uint64_t)_internalData->WFCUserID[1] <<  8) |
	                               ((uint64_t)_internalData->WFCUserID[2] << 16) |
	                               ((uint64_t)_internalData->WFCUserID[3] << 24) |
	                               ((uint64_t)_internalData->WFCUserID[4] << 32) |
	                               ((uint64_t)_internalData->WFCUserID[5] << 40);
	FirmwareConfigInterface::WriteWFCUserIDStringToBuffer(wfcUserIDValue, _wfcUserIDString);
	
	// Generate the subnet mask strings.
	memset(_subnetMaskString, '\0', sizeof(_subnetMaskString));
	
	uint32_t subnetMaskValue = (_internalData->subnetMask_AP1 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP1));
	snprintf(&_subnetMaskString[0],  16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
	
	subnetMaskValue = (_internalData->subnetMask_AP2 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP2));
	snprintf(&_subnetMaskString[16], 16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
	
	subnetMaskValue = (_internalData->subnetMask_AP3 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP3));
	snprintf(&_subnetMaskString[32], 16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
}

FirmwareConfigInterface::~FirmwareConfigInterface()
{
	free(this->_internalData);
	this->_internalData = NULL;
}

void FirmwareConfigInterface::WriteMACAddressStringToBuffer(const uint8_t mac4, const uint8_t mac5, const uint8_t mac6, char *stringBuffer)
{
	if (stringBuffer == NULL)
	{
		return;
	}
	
	snprintf(stringBuffer, 18, "00:09:BF:%02X:%02X:%02X", mac4, mac5, mac6);
}

void FirmwareConfigInterface::WriteMACAddressStringToBuffer(const uint32_t macAddressValue, char *stringBuffer)
{
	if (stringBuffer == NULL)
	{
		return;
	}
	
	const uint8_t mac4 = (macAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (macAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (macAddressValue >> 24) & 0x000000FF;
	
	FirmwareConfigInterface::WriteMACAddressStringToBuffer(mac4, mac5, mac6, stringBuffer);
}

void FirmwareConfigInterface::WriteWFCUserIDStringToBuffer(const uint64_t wfcUserIDValue, char *stringBuffer)
{
	if (stringBuffer == NULL)
	{
		return;
	}
	
	const unsigned long long wfcUserIDValuePrint = (wfcUserIDValue & 0x000007FFFFFFFFFFULL) * 1000ULL;
	const unsigned long long wfcUserIDValuePrint1 =  wfcUserIDValuePrint / 1000000000000ULL;
	const unsigned long long wfcUserIDValuePrint2 = (wfcUserIDValuePrint / 100000000ULL) - (wfcUserIDValuePrint1 * 10000ULL);
	const unsigned long long wfcUserIDValuePrint3 = (wfcUserIDValuePrint / 10000ULL) - (wfcUserIDValuePrint1 * 100000000ULL) - (wfcUserIDValuePrint2 * 10000ULL);
	const unsigned long long wfcUserIDValuePrint4 =  wfcUserIDValuePrint - (wfcUserIDValuePrint1 * 1000000000000ULL) - (wfcUserIDValuePrint2 * 100000000ULL) - (wfcUserIDValuePrint3 * 10000ULL);
	snprintf(stringBuffer, 20, "%04llu-%04llu-%04llu-%04llu", wfcUserIDValuePrint1, wfcUserIDValuePrint2, wfcUserIDValuePrint3, wfcUserIDValuePrint4);
}

const FirmwareConfig& FirmwareConfigInterface::GetFirmwareConfig()
{
	return *this->_internalData;
}

uint32_t FirmwareConfigInterface::GenerateRandomMACValue()
{
	uint32_t randomMACAddressValue = 0;
	
	do
	{
		randomMACAddressValue = (uint32_t)rand() & 0x00FFFFFF;
		randomMACAddressValue = (randomMACAddressValue << 8) | 0x000000BF;
	} while ( (randomMACAddressValue == 0x000000BF) || (randomMACAddressValue == 0xFFFFFFBF) || ((randomMACAddressValue & 0xF0FFFFFF) == (this->_firmwareMACAddressValue & 0xF0FFFFFF)) || ((randomMACAddressValue & 0xF0FFFFFF) == (this->_customMACAddressValue & 0xF0FFFFFF)) );
	
	return randomMACAddressValue;
}

FirmwareCfgMACAddrSetID FirmwareConfigInterface::GetMACAddressSelection()
{
	return this->_macAddressSelection;
}

void FirmwareConfigInterface::SetMACAddressSelection(FirmwareCfgMACAddrSetID addressSetID)
{
	switch (addressSetID)
	{
		case FirmwareCfgMACAddrSetID_Firmware:
			this->_macAddressSelection = FirmwareCfgMACAddrSetID_Firmware;
			this->_internalData->MACAddress[0] = 0x00;
			this->_internalData->MACAddress[1] = 0x09;
			this->_internalData->MACAddress[2] = (uint8_t)( this->_firmwareMACAddressValue        & 0x000000FF);
			this->_internalData->MACAddress[3] = (uint8_t)((this->_firmwareMACAddressValue >>  8) & 0x000000FF);
			this->_internalData->MACAddress[4] = (uint8_t)((this->_firmwareMACAddressValue >> 16) & 0x000000FF);
			this->_internalData->MACAddress[5] = (uint8_t)((this->_firmwareMACAddressValue >> 24) & 0x000000FF);
			break;
			
		case FirmwareCfgMACAddrSetID_Custom1:
		case FirmwareCfgMACAddrSetID_Custom2:
		case FirmwareCfgMACAddrSetID_Custom3:
		case FirmwareCfgMACAddrSetID_Custom4:
		case FirmwareCfgMACAddrSetID_Custom5:
		case FirmwareCfgMACAddrSetID_Custom6:
		case FirmwareCfgMACAddrSetID_Custom7:
		case FirmwareCfgMACAddrSetID_Custom8:
		{
			this->_macAddressSelection = addressSetID;
			this->_internalData->MACAddress[0] = 0x00;
			this->_internalData->MACAddress[1] = 0x09;
			this->_internalData->MACAddress[2] = (uint8_t)( this->_customMACAddressValue        & 0x000000FF);
			this->_internalData->MACAddress[3] = (uint8_t)((this->_customMACAddressValue >>  8) & 0x000000FF);
			this->_internalData->MACAddress[4] = (uint8_t)((this->_customMACAddressValue >> 16) & 0x000000FF);
			this->_internalData->MACAddress[5] = (uint8_t)((this->_customMACAddressValue >> 24) & 0x000000F0);
			
			switch (addressSetID)
			{
				case FirmwareCfgMACAddrSetID_Custom1: this->_internalData->MACAddress[5] += 1; break;
				case FirmwareCfgMACAddrSetID_Custom2: this->_internalData->MACAddress[5] += 2; break;
				case FirmwareCfgMACAddrSetID_Custom3: this->_internalData->MACAddress[5] += 3; break;
				case FirmwareCfgMACAddrSetID_Custom4: this->_internalData->MACAddress[5] += 4; break;
				case FirmwareCfgMACAddrSetID_Custom5: this->_internalData->MACAddress[5] += 5; break;
				case FirmwareCfgMACAddrSetID_Custom6: this->_internalData->MACAddress[5] += 6; break;
				case FirmwareCfgMACAddrSetID_Custom7: this->_internalData->MACAddress[5] += 7; break;
				case FirmwareCfgMACAddrSetID_Custom8: this->_internalData->MACAddress[5] += 8; break;
				default: break;
			}
			
			break;
		}
			
		default:
			break;
	}
}

uint32_t FirmwareConfigInterface::GetMACAddressValue(FirmwareCfgMACAddrSetID addressSetID)
{
	switch (addressSetID)
	{
		case FirmwareCfgMACAddrSetID_Firmware: return this->_firmwareMACAddressValue;
		case FirmwareCfgMACAddrSetID_Custom1:  return this->_customMACAddressValue + 0x01000000;
		case FirmwareCfgMACAddrSetID_Custom2:  return this->_customMACAddressValue + 0x02000000;
		case FirmwareCfgMACAddrSetID_Custom3:  return this->_customMACAddressValue + 0x03000000;
		case FirmwareCfgMACAddrSetID_Custom4:  return this->_customMACAddressValue + 0x04000000;
		case FirmwareCfgMACAddrSetID_Custom5:  return this->_customMACAddressValue + 0x05000000;
		case FirmwareCfgMACAddrSetID_Custom6:  return this->_customMACAddressValue + 0x06000000;
		case FirmwareCfgMACAddrSetID_Custom7:  return this->_customMACAddressValue + 0x07000000;
		case FirmwareCfgMACAddrSetID_Custom8:  return this->_customMACAddressValue + 0x08000000;
		default: break;
	}
	
	return 0;
}

const char* FirmwareConfigInterface::GetMACAddressString(FirmwareCfgMACAddrSetID addressSetID)
{
	switch (addressSetID)
	{
		case FirmwareCfgMACAddrSetID_Firmware: return &this->_macAddressString[18*0];
		case FirmwareCfgMACAddrSetID_Custom1:  return &this->_macAddressString[18*1];
		case FirmwareCfgMACAddrSetID_Custom2:  return &this->_macAddressString[18*2];
		case FirmwareCfgMACAddrSetID_Custom3:  return &this->_macAddressString[18*3];
		case FirmwareCfgMACAddrSetID_Custom4:  return &this->_macAddressString[18*4];
		case FirmwareCfgMACAddrSetID_Custom5:  return &this->_macAddressString[18*5];
		case FirmwareCfgMACAddrSetID_Custom6:  return &this->_macAddressString[18*6];
		case FirmwareCfgMACAddrSetID_Custom7:  return &this->_macAddressString[18*7];
		case FirmwareCfgMACAddrSetID_Custom8:  return &this->_macAddressString[18*8];
		default: break;
	}
	
	return "";
}

uint32_t FirmwareConfigInterface::GetSelectedMACAddressValue()
{
	const uint32_t selectedMACAddressValue = (uint32_t)this->_internalData->MACAddress[2] | ((uint32_t)this->_internalData->MACAddress[3] << 8) | ((uint32_t)this->_internalData->MACAddress[4] << 16) | ((uint32_t)this->_internalData->MACAddress[5] << 24);
	return selectedMACAddressValue;
}

uint64_t FirmwareConfigInterface::GetSelectedWFCUserID64()
{
	const uint64_t selectedUserID64 = (uint64_t)this->_internalData->WFCUserID[0] |
	                                 ((uint64_t)this->_internalData->WFCUserID[1] <<  8) |
	                                 ((uint64_t)this->_internalData->WFCUserID[2] << 16) |
	                                 ((uint64_t)this->_internalData->WFCUserID[3] << 24) |
	                                 ((uint64_t)this->_internalData->WFCUserID[4] << 32) |
	                                 ((uint64_t)this->_internalData->WFCUserID[5] << 40);
	return selectedUserID64;
}

const char* FirmwareConfigInterface::GetSelectedMACAddressString()
{
	return this->GetMACAddressString(this->_macAddressSelection);
}

void FirmwareConfigInterface::SetFirmwareMACAddressValue(uint32_t macAddressValue)
{
	const uint8_t mac4 = (macAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (macAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (macAddressValue >> 24) & 0x000000FF;
	
	this->SetFirmwareMACAddressValue(mac4, mac5, mac6);
}

void FirmwareConfigInterface::SetFirmwareMACAddressValue(uint8_t mac4, uint8_t mac5, uint8_t mac6)
{
	this->_firmwareMACAddressValue = 0x000000BF | ((uint32_t)mac4 << 8) | ((uint32_t)mac5 << 16) | ((uint32_t)mac6 << 24);
	FirmwareConfigInterface::WriteMACAddressStringToBuffer(mac4, mac5, mac6, &this->_macAddressString[0]);
	
	if (this->_macAddressSelection == FirmwareCfgMACAddrSetID_Firmware)
	{
		this->SetMACAddressSelection(FirmwareCfgMACAddrSetID_Firmware);
	}
}

void FirmwareConfigInterface::SetCustomMACAddressValue(uint32_t macAddressValue)
{
	const uint8_t mac4 = (macAddressValue >>  8) & 0x000000FF;
	const uint8_t mac5 = (macAddressValue >> 16) & 0x000000FF;
	const uint8_t mac6 = (macAddressValue >> 24) & 0x000000F0;
	
	this->SetCustomMACAddressValue(mac4, mac5, mac6);
}

void FirmwareConfigInterface::SetCustomMACAddressValue(uint8_t mac4, uint8_t mac5, uint8_t mac6)
{
	mac6 &= 0xF0;
	this->_customMACAddressValue = 0x000000BF | ((uint32_t)mac4 << 8) | ((uint32_t)mac5 << 16) | ((uint32_t)mac6 << 24);
	
	for (size_t i = 1; i <= 8; i++)
	{
		FirmwareConfigInterface::WriteMACAddressStringToBuffer(mac4, mac5, mac6 + (uint8_t)i, &this->_macAddressString[18*i]);
	}
	
	if (this->_macAddressSelection != FirmwareCfgMACAddrSetID_Firmware)
	{
		this->SetMACAddressSelection(this->_macAddressSelection);
	}
}

uint8_t* FirmwareConfigInterface::GetWFCUserID()
{
	return this->_internalData->WFCUserID;
}

void FirmwareConfigInterface::SetWFCUserID(uint8_t *wfcUserID)
{
	const uint64_t wfcUserIDValue = (uint64_t)wfcUserID[0] |
	                               ((uint64_t)wfcUserID[1] <<  8) |
	                               ((uint64_t)wfcUserID[2] << 16) |
	                               ((uint64_t)wfcUserID[3] << 24) |
	                               ((uint64_t)wfcUserID[4] << 32) |
	                               ((uint64_t)wfcUserID[5] << 40);
	
	this->SetWFCUserID64(wfcUserIDValue);
}

uint64_t FirmwareConfigInterface::GetWFCUserID64()
{
	const uint64_t userID = (uint64_t)this->_internalData->WFCUserID[0] |
	                       ((uint64_t)this->_internalData->WFCUserID[1] <<  8) |
	                       ((uint64_t)this->_internalData->WFCUserID[2] << 16) |
	                       ((uint64_t)this->_internalData->WFCUserID[3] << 24) |
	                       ((uint64_t)this->_internalData->WFCUserID[4] << 32) |
	                       ((uint64_t)this->_internalData->WFCUserID[5] << 40);
	return userID;
}

void FirmwareConfigInterface::SetWFCUserID64(uint64_t wfcUserIDValue)
{
	if ( (wfcUserIDValue & 0x000007FFFFFFFFFFULL) != 0)
	{
		this->_internalData->WFCUserID[0] = (uint8_t)( wfcUserIDValue        & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[1] = (uint8_t)((wfcUserIDValue >>  8) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[2] = (uint8_t)((wfcUserIDValue >> 16) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[3] = (uint8_t)((wfcUserIDValue >> 24) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[4] = (uint8_t)((wfcUserIDValue >> 32) & 0x00000000000000FFULL);
		this->_internalData->WFCUserID[5] = (uint8_t)((wfcUserIDValue >> 40) & 0x00000000000000FFULL);
	}
	else
	{
		this->_internalData->WFCUserID[0] = 0;
		this->_internalData->WFCUserID[1] = 0;
		this->_internalData->WFCUserID[2] = 0;
		this->_internalData->WFCUserID[3] = 0;
		this->_internalData->WFCUserID[4] = 0;
		this->_internalData->WFCUserID[5] = 0;
	}
	
	FirmwareConfigInterface::WriteWFCUserIDStringToBuffer(wfcUserIDValue, this->_wfcUserIDString);
}

const char* FirmwareConfigInterface::GetWFCUserIDString()
{
	return this->_wfcUserIDString;
}

uint8_t FirmwareConfigInterface::GetIPv4AddressPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4Address_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4Address_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4Address_AP3[addrPart];
	}
	
	return 0;
}

void FirmwareConfigInterface::SetIPv4AddressPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4Address_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4Address_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4Address_AP3[addrPart] = addrValue; break;
	}
}

uint8_t FirmwareConfigInterface::GetIPv4GatewayPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4Gateway_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4Gateway_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4Gateway_AP3[addrPart];
	}
	
	return 0;
}

void FirmwareConfigInterface::SetIPv4GatewayPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4Gateway_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4Gateway_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4Gateway_AP3[addrPart] = addrValue; break;
	}
}

uint8_t FirmwareConfigInterface::GetIPv4PrimaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4PrimaryDNS_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4PrimaryDNS_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4PrimaryDNS_AP3[addrPart];
	}
	
	return 0;
}

void FirmwareConfigInterface::SetIPv4PrimaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4PrimaryDNS_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4PrimaryDNS_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4PrimaryDNS_AP3[addrPart] = addrValue; break;
	}
}

uint8_t FirmwareConfigInterface::GetIPv4SecondaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart)
{
	if (addrPart > 3)
	{
		return 0;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->ipv4SecondaryDNS_AP1[addrPart];
		case FirmwareCfgAPID_AP2: return this->_internalData->ipv4SecondaryDNS_AP2[addrPart];
		case FirmwareCfgAPID_AP3: return this->_internalData->ipv4SecondaryDNS_AP3[addrPart];
	}
	
	return 0;
}

void FirmwareConfigInterface::SetIPv4SecondaryDNSPart(FirmwareCfgAPID apid, uint8_t addrPart, uint8_t addrValue)
{
	if (addrPart > 3)
	{
		return;
	}
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: this->_internalData->ipv4SecondaryDNS_AP1[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP2: this->_internalData->ipv4SecondaryDNS_AP2[addrPart] = addrValue; break;
		case FirmwareCfgAPID_AP3: this->_internalData->ipv4SecondaryDNS_AP3[addrPart] = addrValue; break;
	}
}

uint8_t FirmwareConfigInterface::GetSubnetMask(FirmwareCfgAPID apid)
{
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return this->_internalData->subnetMask_AP1;
		case FirmwareCfgAPID_AP2: return this->_internalData->subnetMask_AP2;
		case FirmwareCfgAPID_AP3: return this->_internalData->subnetMask_AP3;
	}
	
	return 0;
}

const char* FirmwareConfigInterface::GetSubnetMaskString(FirmwareCfgAPID apid)
{
	switch (apid)
	{
		case FirmwareCfgAPID_AP1: return &this->_subnetMaskString[0];
		case FirmwareCfgAPID_AP2: return &this->_subnetMaskString[16];
		case FirmwareCfgAPID_AP3: return &this->_subnetMaskString[32];
	}
	
	return "";
}

void FirmwareConfigInterface::SetSubnetMask(FirmwareCfgAPID apid, uint8_t subnetMaskShift)
{
	if (subnetMaskShift > 28)
	{
		subnetMaskShift = 28;
	}
	
	int subnetMaskCharIndex = -1;
	uint32_t subnetMaskValue = 0;
	
	switch (apid)
	{
		case FirmwareCfgAPID_AP1:
			this->_internalData->subnetMask_AP1 = subnetMaskShift;
			subnetMaskCharIndex = 0;
			break;
			
		case FirmwareCfgAPID_AP2:
			this->_internalData->subnetMask_AP2 = subnetMaskShift;
			subnetMaskCharIndex = 16;
			break;
			
		case FirmwareCfgAPID_AP3:
			this->_internalData->subnetMask_AP3 = subnetMaskShift;
			subnetMaskCharIndex = 32;
			break;
	}
	
	if ( (subnetMaskCharIndex >= 0) && (subnetMaskCharIndex <= 32) )
	{
		subnetMaskValue = (subnetMaskShift == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMaskShift));
		memset(&this->_subnetMaskString[subnetMaskCharIndex], '\0', 16 * sizeof(char));
		snprintf(&this->_subnetMaskString[subnetMaskCharIndex], 16, "%d.%d.%d.%d", (subnetMaskValue >> 24) & 0x000000FF, (subnetMaskValue >> 16) & 0x000000FF, (subnetMaskValue >> 8) & 0x000000FF, subnetMaskValue & 0x000000FF);
	}
}

int FirmwareConfigInterface::GetConsoleType()
{
	return (int)this->_internalData->consoleType;
}

void FirmwareConfigInterface::SetConsoleType(int type)
{
	this->_internalData->consoleType = (uint8_t)type;
}

uint16_t* FirmwareConfigInterface::GetNicknameStringBuffer()
{
	return this->_internalData->nickname;
}

void FirmwareConfigInterface::SetNicknameWithStringBuffer(uint16_t *buffer, size_t charLength)
{
	size_t i = 0;
	
	if (buffer != NULL)
	{
		if (charLength > MAX_FW_NICKNAME_LENGTH)
		{
			charLength = MAX_FW_NICKNAME_LENGTH;
		}
		
		for (; i < charLength; i++)
		{
			this->_internalData->nickname[i] = buffer[i];
		}
	}
	else
	{
		charLength = 0;
	}
	
	for (; i < MAX_FW_NICKNAME_LENGTH+1; i++)
	{
		this->_internalData->nickname[i] = 0;
	}
	
	this->_internalData->nicknameLength = charLength;
}

size_t FirmwareConfigInterface::GetNicknameStringLength()
{
	return (size_t)this->_internalData->nicknameLength;
}

void FirmwareConfigInterface::SetNicknameStringLength(size_t charLength)
{
	if (charLength > MAX_FW_NICKNAME_LENGTH)
	{
		charLength = MAX_FW_NICKNAME_LENGTH;
	}
	
	this->_internalData->nicknameLength = charLength;
}

uint16_t* FirmwareConfigInterface::GetMessageStringBuffer()
{
	return this->_internalData->message;
}

void FirmwareConfigInterface::SetMessageWithStringBuffer(uint16_t *buffer, size_t charLength)
{
	size_t i = 0;
	
	if (buffer != NULL)
	{
		if (charLength > MAX_FW_MESSAGE_LENGTH)
		{
			charLength = MAX_FW_MESSAGE_LENGTH;
		}
		
		for (; i < charLength; i++)
		{
			this->_internalData->message[i] = buffer[i];
		}
	}
	else
	{
		charLength = 0;
	}
	
	for (; i < MAX_FW_MESSAGE_LENGTH+1; i++)
	{
		this->_internalData->message[i] = 0;
	}
	
	this->_internalData->messageLength = charLength;
}

size_t FirmwareConfigInterface::GetMessageStringLength()
{
	return (size_t)this->_internalData->messageLength;
}

void FirmwareConfigInterface::SetMessageStringLength(size_t charLength)
{
	if (charLength > MAX_FW_MESSAGE_LENGTH)
	{
		charLength = MAX_FW_MESSAGE_LENGTH;
	}
	
	this->_internalData->messageLength = charLength;
}

int FirmwareConfigInterface::GetFavoriteColorByID()
{
	return (int)this->_internalData->favoriteColor;
}

void FirmwareConfigInterface::SetFavoriteColorByID(int colorID)
{
	this->_internalData->favoriteColor = (uint8_t)colorID;
}

int FirmwareConfigInterface::GetBirthdayDay()
{
	return (int)this->_internalData->birthdayDay;
}

void FirmwareConfigInterface::SetBirthdayDay(int day)
{
	if (day < 1)
	{
		day = 1;
	}
	
	switch (this->_internalData->birthdayMonth)
	{
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
		{
			if (day > 31) day = 31;
			break;
		}
			
		case 4:
		case 6:
		case 9:
		case 11:
		{
			if (day > 30) day = 30;
			break;
		}
			
		case 2:
		{
			if (day > 29) day = 29;
			break;
		}
			
		default:
			break;
	}
	
	this->_internalData->birthdayDay = (uint8_t)day;
}

int FirmwareConfigInterface::GetBirthdayMonth()
{
	return (int)this->_internalData->birthdayMonth;
}

void FirmwareConfigInterface::SetBirthdayMonth(int month)
{
	if (month < 1)
	{
		month = 1;
	}
	else if (month > 12)
	{
		month = 12;
	}
	
	this->_internalData->birthdayMonth = (uint8_t)month;
}

int FirmwareConfigInterface::GetLanguageByID()
{
	return (int)this->_internalData->language;
}

void FirmwareConfigInterface::SetLanguageByID(int languageID)
{
	this->_internalData->language = (uint8_t)languageID;
}

int FirmwareConfigInterface::GetBacklightLevel()
{
	return (int)this->_internalData->backlightLevel;
}

void FirmwareConfigInterface::SetBacklightLevel(int level)
{
	this->_internalData->backlightLevel = (uint8_t)level;
}
