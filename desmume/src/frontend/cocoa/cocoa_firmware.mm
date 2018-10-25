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
#include "../../NDSSystem.h"
#include "../../firmware.h"
#include "../../MMU.h"
#undef BOOL


@implementation CocoaDSFirmware

@dynamic MACAddressValue;
@synthesize MACAddressString;

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
	
	_internalData = (FirmwareConfig *)malloc(sizeof(FirmwareConfig));
	if (_internalData == nil)
	{
		[self release];
		return nil;
	}
	
	NDS_GetDefaultFirmwareConfig(*_internalData);
	
	_birth_year = 1970;
	
	MACAddressString = [[NSString stringWithFormat:@"%02X:%02X:%02X:%02X:%02X:%02X",
						 _internalData->MACAddress[0], _internalData->MACAddress[1], _internalData->MACAddress[2],
						 _internalData->MACAddress[3], _internalData->MACAddress[4], _internalData->MACAddress[5]] retain];
	
	uint32_t subnetMaskValue = (_internalData->subnetMask_AP1 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP1));
	subnetMaskString_AP1 = [[NSString stringWithFormat:@"%d.%d.%d.%d",
							 (subnetMaskValue >> 24) & 0x000000FF,
							 (subnetMaskValue >> 16) & 0x000000FF,
							 (subnetMaskValue >>  8) & 0x000000FF,
							 (subnetMaskValue >>  0) & 0x000000FF] retain];
	
	subnetMaskValue = (_internalData->subnetMask_AP2 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP2));
	subnetMaskString_AP2 = [[NSString stringWithFormat:@"%d.%d.%d.%d",
							 (subnetMaskValue >> 24) & 0x000000FF,
							 (subnetMaskValue >> 16) & 0x000000FF,
							 (subnetMaskValue >>  8) & 0x000000FF,
							 (subnetMaskValue >>  0) & 0x000000FF] retain];
	
	subnetMaskValue = (_internalData->subnetMask_AP3 == 0) ? 0 : (0xFFFFFFFF << (32 - _internalData->subnetMask_AP3));
	subnetMaskString_AP3 = [[NSString stringWithFormat:@"%d.%d.%d.%d",
							 (subnetMaskValue >> 24) & 0x000000FF,
							 (subnetMaskValue >> 16) & 0x000000FF,
							 (subnetMaskValue >>  8) & 0x000000FF,
							 (subnetMaskValue >>  0) & 0x000000FF] retain];
	
	pthread_mutex_init(&mutex, NULL);
	
	return self;
}

- (void)dealloc
{
	free(_internalData);
	_internalData = NULL;
		
	pthread_mutex_destroy(&mutex);
	
	[self setMACAddressString:nil];
	[self setSubnetMaskString_AP1:nil];
	[self setSubnetMaskString_AP2:nil];
	[self setSubnetMaskString_AP3:nil];
	
	[super dealloc];
}

- (void) setMACAddressValue:(uint32_t)inMACAddressValue
{
	pthread_mutex_lock(&mutex);
	_internalData->MACAddress[0] = 0x00;
	_internalData->MACAddress[1] = 0x09;
	_internalData->MACAddress[2] = 0xBF;
	_internalData->MACAddress[3] = (inMACAddressValue >>  8) & 0xFF;
	_internalData->MACAddress[4] = (inMACAddressValue >> 16) & 0xFF;
	_internalData->MACAddress[5] = (inMACAddressValue >> 24) & 0xFF;
	pthread_mutex_unlock(&mutex);
	
	NSString *theMACAddressString = [NSString stringWithFormat:@"%02X:%02X:%02X:%02X:%02X:%02X",
									 _internalData->MACAddress[0], _internalData->MACAddress[1], _internalData->MACAddress[2],
									 _internalData->MACAddress[3], _internalData->MACAddress[4], _internalData->MACAddress[5]];
	
	[self setMACAddressString:theMACAddressString];
}

- (uint32_t) MACAddressValue
{
	uint32_t outMACAddressValue = 0;
	outMACAddressValue |= (_internalData->MACAddress[2] <<  0);
	outMACAddressValue |= (_internalData->MACAddress[3] <<  8);
	outMACAddressValue |= (_internalData->MACAddress[4] << 16);
	outMACAddressValue |= (_internalData->MACAddress[5] << 24);
	
	return outMACAddressValue;
}

- (void) setIpv4Address_AP1_1:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP1[0] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP1_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP1[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP1_2:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP1[1] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP1_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP1[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP1_3:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP1[2] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP1_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP1[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP1_4:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP1[3] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP1_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP1[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Gateway_AP1_1:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP1[0] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP1_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP1[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP1_2:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP1[1] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP1_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP1[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP1_3:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP1[2] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP1_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP1[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP1_4:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP1[3] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP1_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP1[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4PrimaryDNS_AP1_1:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP1[0] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP1_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP1[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP1_2:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP1[1] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP1_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP1[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP1_3:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP1[2] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP1_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP1[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP1_4:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP1[3] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP1_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP1[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP1_1:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP1[0] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP1_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP1[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP1_2:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP1[1] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP1_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP1[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP1_3:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP1[2] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP1_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP1[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP1_4:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP1[3] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP1_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP1[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setSubnetMask_AP1:(NSInteger)subnetMask
{
	if (subnetMask > 28)
	{
		subnetMask = 28;
	}
	else if (subnetMask < 0)
	{
		subnetMask = 0;
	}
	
	pthread_mutex_lock(&mutex);
	_internalData->subnetMask_AP1 = subnetMask;
	pthread_mutex_unlock(&mutex);
	
	const uint32_t subnetMaskValue = (subnetMask == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMask));
	NSString *subnetMaskString = [NSString stringWithFormat:@"%d.%d.%d.%d",
								  (subnetMaskValue >> 24) & 0x000000FF,
								  (subnetMaskValue >> 16) & 0x000000FF,
								  (subnetMaskValue >>  8) & 0x000000FF,
								  (subnetMaskValue >>  0) & 0x000000FF];
	
	[self setSubnetMaskString_AP1:subnetMaskString];
}

- (NSInteger) subnetMask_AP1
{
	pthread_mutex_lock(&mutex);
	const NSInteger subnetMask = _internalData->subnetMask_AP1;
	pthread_mutex_unlock(&mutex);
	
	return subnetMask;
}

- (void) setIpv4Address_AP2_1:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP2[0] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP2_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP2[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP2_2:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP2[1] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP2_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP2[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP2_3:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP2[2] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP2_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP2[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP2_4:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP2[3] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP2_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP2[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Gateway_AP2_1:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP2[0] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP2_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP2[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP2_2:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP2[1] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP2_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP2[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP2_3:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP2[2] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP2_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP2[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP2_4:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP2[3] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP2_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP2[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4PrimaryDNS_AP2_1:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP2[0] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP2_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP2[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP2_2:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP2[1] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP2_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP2[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP2_3:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP2[2] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP2_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP2[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP2_4:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP2[3] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP2_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP2[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP2_1:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP2[0] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP2_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP2[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP2_2:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP2[1] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP2_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP2[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP2_3:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP2[2] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP2_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP2[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP2_4:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP2[3] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP2_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP2[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setSubnetMask_AP2:(NSInteger)subnetMask
{
	if (subnetMask > 28)
	{
		subnetMask = 28;
	}
	else if (subnetMask < 0)
	{
		subnetMask = 0;
	}
	
	pthread_mutex_lock(&mutex);
	_internalData->subnetMask_AP2 = subnetMask;
	pthread_mutex_unlock(&mutex);
	
	const uint32_t subnetMaskValue = (subnetMask == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMask));
	NSString *subnetMaskString = [NSString stringWithFormat:@"%d.%d.%d.%d",
								  (subnetMaskValue >> 24) & 0x000000FF,
								  (subnetMaskValue >> 16) & 0x000000FF,
								  (subnetMaskValue >>  8) & 0x000000FF,
								  (subnetMaskValue >>  0) & 0x000000FF];
	
	[self setSubnetMaskString_AP2:subnetMaskString];
}

- (NSInteger) subnetMask_AP2
{
	pthread_mutex_lock(&mutex);
	const NSInteger subnetMask = _internalData->subnetMask_AP2;
	pthread_mutex_unlock(&mutex);
	
	return subnetMask;
}

- (void) setIpv4Address_AP3_1:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP3[0] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP3_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP3[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP3_2:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP3[1] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP3_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP3[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP3_3:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP3[2] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP3_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP3[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Address_AP3_4:(NSInteger)ipv4AddressPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Address_AP3[3] = ipv4AddressPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Address_AP3_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4AddressPart = _internalData->ipv4Address_AP3[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4AddressPart;
}

- (void) setIpv4Gateway_AP3_1:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP3[0] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP3_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP3[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP3_2:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP3[1] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP3_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP3[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP3_3:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP3[2] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP3_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP3[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4Gateway_AP3_4:(NSInteger)ipv4GatewayPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4Gateway_AP3[3] = ipv4GatewayPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4Gateway_AP3_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4GatewayPart = _internalData->ipv4Gateway_AP3[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4GatewayPart;
}

- (void) setIpv4PrimaryDNS_AP3_1:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP3[0] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP3_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP3[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP3_2:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP3[1] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP3_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP3[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP3_3:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP3[2] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP3_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP3[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4PrimaryDNS_AP3_4:(NSInteger)ipv4PrimaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4PrimaryDNS_AP3[3] = ipv4PrimaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4PrimaryDNS_AP3_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4PrimaryDNSPart = _internalData->ipv4PrimaryDNS_AP3[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4PrimaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP3_1:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP3[0] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP3_1
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP3[0];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP3_2:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP3[1] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP3_2
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP3[1];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP3_3:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP3[2] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP3_3
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP3[2];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setIpv4SecondaryDNS_AP3_4:(NSInteger)ipv4SecondaryDNSPart
{
	pthread_mutex_lock(&mutex);
	_internalData->ipv4SecondaryDNS_AP3[3] = ipv4SecondaryDNSPart;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) ipv4SecondaryDNS_AP3_4
{
	pthread_mutex_lock(&mutex);
	const NSInteger ipv4SecondaryDNSPart = _internalData->ipv4SecondaryDNS_AP3[3];
	pthread_mutex_unlock(&mutex);
	
	return ipv4SecondaryDNSPart;
}

- (void) setSubnetMask_AP3:(NSInteger)subnetMask
{
	if (subnetMask > 28)
	{
		subnetMask = 28;
	}
	else if (subnetMask < 0)
	{
		subnetMask = 0;
	}
	
	pthread_mutex_lock(&mutex);
	_internalData->subnetMask_AP3 = subnetMask;
	pthread_mutex_unlock(&mutex);
	
	const uint32_t subnetMaskValue = (subnetMask == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMask));
	NSString *subnetMaskString = [NSString stringWithFormat:@"%d.%d.%d.%d",
								  (subnetMaskValue >> 24) & 0x000000FF,
								  (subnetMaskValue >> 16) & 0x000000FF,
								  (subnetMaskValue >>  8) & 0x000000FF,
								  (subnetMaskValue >>  0) & 0x000000FF];
	
	[self setSubnetMaskString_AP3:subnetMaskString];
}

- (NSInteger) subnetMask_AP3
{
	pthread_mutex_lock(&mutex);
	const NSInteger subnetMask = _internalData->subnetMask_AP3;
	pthread_mutex_unlock(&mutex);
	
	return subnetMask;
}

- (void) setConsoleType:(NSInteger)theType
{
	pthread_mutex_lock(&mutex);
	_internalData->consoleType = (NDS_CONSOLE_TYPE)theType;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) consoleType
{
	pthread_mutex_lock(&mutex);
	NSInteger theType = _internalData->consoleType;
	pthread_mutex_unlock(&mutex);
	
	return theType;
}

- (void) setNickname:(NSString *)theNickname
{
	pthread_mutex_lock(&mutex);
	
	if (theNickname != nil)
	{
		NSRange characterRange = {0, [theNickname length]};
		
		if (characterRange.length > MAX_FW_NICKNAME_LENGTH)
		{
			characterRange.length = MAX_FW_NICKNAME_LENGTH;
		}
		
		[theNickname getBytes:&_internalData->nickname[0]
					maxLength:(sizeof(UInt16) * characterRange.length)
				   usedLength:NULL
					 encoding:NSUTF16LittleEndianStringEncoding
					  options:NSStringEncodingConversionAllowLossy
						range:characterRange
			   remainingRange:NULL];
		
		_internalData->nicknameLength = (u8)characterRange.length;
	}
	else
	{
		memset(&_internalData->nickname[0], 0, sizeof(_internalData->nickname));
		_internalData->nicknameLength = 0;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSString *) nickname
{
	pthread_mutex_lock(&mutex);
	NSString *theNickname = [[[NSString alloc] initWithBytes:&_internalData->nickname[0] length:(sizeof(UInt16) * _internalData->nicknameLength) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
	pthread_mutex_unlock(&mutex);
	
	return theNickname;
}

- (void) setMessage:(NSString *)theMessage
{
	pthread_mutex_lock(&mutex);
	
	if (theMessage != nil)
	{
		NSRange characterRange = {0, [theMessage length]};
		
		if (characterRange.length > MAX_FW_MESSAGE_LENGTH)
		{
			characterRange.length = MAX_FW_MESSAGE_LENGTH;
		}
		
		[theMessage getBytes:&_internalData->message[0]
				   maxLength:(sizeof(UInt16) * characterRange.length)
				  usedLength:NULL
					encoding:NSUTF16LittleEndianStringEncoding
					 options:NSStringEncodingConversionAllowLossy
					   range:characterRange
			  remainingRange:NULL];
		
		_internalData->messageLength = (u8)characterRange.length;
	}
	else
	{
		memset(&_internalData->message[0], 0, sizeof(_internalData->message));
		_internalData->messageLength = 0;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSString *) message
{
	pthread_mutex_lock(&mutex);
	NSString *theMessage = [[[NSString alloc] initWithBytes:&_internalData->message[0] length:(sizeof(UInt16) * _internalData->messageLength) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
	pthread_mutex_unlock(&mutex);
	
	return theMessage;
}

- (void) setFavoriteColor:(NSInteger)colorID
{
	pthread_mutex_lock(&mutex);
	_internalData->favoriteColor = (u8)colorID;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) favoriteColor
{
	pthread_mutex_lock(&mutex);
	NSInteger theFavoriteColorID = _internalData->favoriteColor;
	pthread_mutex_unlock(&mutex);
	
	return theFavoriteColorID;
}

- (void) setBirthday:(NSDate *)theDate
{
	pthread_mutex_lock(&mutex);
	
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
		
		_internalData->birthdayMonth = (u8)theMonth;
		_internalData->birthdayDay = (u8)theDay;
		_birth_year = theYear;
	}
	else
	{
		_internalData->birthdayMonth = 1;
		_internalData->birthdayDay = 1;
		_birth_year = 1970;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSDate *) birthday
{
	pthread_mutex_lock(&mutex);
	NSDate *theBirthday = [NSDate dateWithString:[NSString stringWithFormat:@"%ld-%ld-%ld 12:00:00 +0000", (unsigned long)_birth_year, (unsigned long)_internalData->birthdayMonth, (unsigned long)_internalData->birthdayDay]];
	pthread_mutex_unlock(&mutex);
	
	return theBirthday;
}

- (void) setLanguage:(NSInteger)languageID
{
	pthread_mutex_lock(&mutex);
	_internalData->language = (u8)languageID;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) language
{
	pthread_mutex_lock(&mutex);
	NSInteger theLanguageID = _internalData->language;
	pthread_mutex_unlock(&mutex);
	
	return theLanguageID;
}

- (void) setBacklightLevel:(NSInteger)level
{
	pthread_mutex_lock(&mutex);
	_internalData->backlightLevel = (u8)level;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) backlightLevel
{
	pthread_mutex_lock(&mutex);
	NSInteger level = _internalData->backlightLevel;
	pthread_mutex_unlock(&mutex);
	
	return level;
}

- (void) update
{
	pthread_mutex_lock(&mutex);
	NDS_InitFirmwareWithConfig(*_internalData);
	pthread_mutex_unlock(&mutex);
}

- (uint32_t) generateRandomMACAddress
{
	uint32_t randomMACAddressValue = 0;
	
	do
	{
		randomMACAddressValue = (uint32_t)random() & 0x00FFFFFF;
	} while (randomMACAddressValue == 0);
	
	randomMACAddressValue = (randomMACAddressValue << 8) | 0xBF;
	
	uint8_t newMACAddress[6];
	newMACAddress[0] = 0x00;
	newMACAddress[1] = 0x09;
	newMACAddress[2] = (randomMACAddressValue >>  0) & 0x000000FF;
	newMACAddress[3] = (randomMACAddressValue >>  8) & 0x000000FF;
	newMACAddress[4] = (randomMACAddressValue >> 16) & 0x000000FF;
	newMACAddress[5] = (randomMACAddressValue >> 24) & 0x000000FF;
	
	[self setMACAddressValue:randomMACAddressValue];
	
	return randomMACAddressValue;
}

@end
