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

#import <Cocoa/Cocoa.h>
#include <pthread.h>

struct FirmwareConfig;

/********************************************************************************************
	CocoaDSFirmware - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for DeSmuME's firmware struct.
 
	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
@interface CocoaDSFirmware : NSObject
{
	FirmwareConfig *_internalData;
	NSUInteger _birth_year;
	
	NSString *MACAddressString;
	NSString *subnetMaskString_AP1;
	NSString *subnetMaskString_AP2;
	NSString *subnetMaskString_AP3;
	
	pthread_mutex_t mutex;
}

// WiFi Info
@property (assign) uint32_t MACAddressValue;
@property (retain) NSString *MACAddressString;

// Access Point Settings
@property (assign) NSInteger ipv4Address_AP1_1;
@property (assign) NSInteger ipv4Address_AP1_2;
@property (assign) NSInteger ipv4Address_AP1_3;
@property (assign) NSInteger ipv4Address_AP1_4;
@property (assign) NSInteger ipv4Gateway_AP1_1;
@property (assign) NSInteger ipv4Gateway_AP1_2;
@property (assign) NSInteger ipv4Gateway_AP1_3;
@property (assign) NSInteger ipv4Gateway_AP1_4;
@property (assign) NSInteger ipv4PrimaryDNS_AP1_1;
@property (assign) NSInteger ipv4PrimaryDNS_AP1_2;
@property (assign) NSInteger ipv4PrimaryDNS_AP1_3;
@property (assign) NSInteger ipv4PrimaryDNS_AP1_4;
@property (assign) NSInteger ipv4SecondaryDNS_AP1_1;
@property (assign) NSInteger ipv4SecondaryDNS_AP1_2;
@property (assign) NSInteger ipv4SecondaryDNS_AP1_3;
@property (assign) NSInteger ipv4SecondaryDNS_AP1_4;
@property (assign) NSInteger subnetMask_AP1;
@property (retain) NSString *subnetMaskString_AP1;

@property (assign) NSInteger ipv4Address_AP2_1;
@property (assign) NSInteger ipv4Address_AP2_2;
@property (assign) NSInteger ipv4Address_AP2_3;
@property (assign) NSInteger ipv4Address_AP2_4;
@property (assign) NSInteger ipv4Gateway_AP2_1;
@property (assign) NSInteger ipv4Gateway_AP2_2;
@property (assign) NSInteger ipv4Gateway_AP2_3;
@property (assign) NSInteger ipv4Gateway_AP2_4;
@property (assign) NSInteger ipv4PrimaryDNS_AP2_1;
@property (assign) NSInteger ipv4PrimaryDNS_AP2_2;
@property (assign) NSInteger ipv4PrimaryDNS_AP2_3;
@property (assign) NSInteger ipv4PrimaryDNS_AP2_4;
@property (assign) NSInteger ipv4SecondaryDNS_AP2_1;
@property (assign) NSInteger ipv4SecondaryDNS_AP2_2;
@property (assign) NSInteger ipv4SecondaryDNS_AP2_3;
@property (assign) NSInteger ipv4SecondaryDNS_AP2_4;
@property (assign) NSInteger subnetMask_AP2;
@property (retain) NSString *subnetMaskString_AP2;

@property (assign) NSInteger ipv4Address_AP3_1;
@property (assign) NSInteger ipv4Address_AP3_2;
@property (assign) NSInteger ipv4Address_AP3_3;
@property (assign) NSInteger ipv4Address_AP3_4;
@property (assign) NSInteger ipv4Gateway_AP3_1;
@property (assign) NSInteger ipv4Gateway_AP3_2;
@property (assign) NSInteger ipv4Gateway_AP3_3;
@property (assign) NSInteger ipv4Gateway_AP3_4;
@property (assign) NSInteger ipv4PrimaryDNS_AP3_1;
@property (assign) NSInteger ipv4PrimaryDNS_AP3_2;
@property (assign) NSInteger ipv4PrimaryDNS_AP3_3;
@property (assign) NSInteger ipv4PrimaryDNS_AP3_4;
@property (assign) NSInteger ipv4SecondaryDNS_AP3_1;
@property (assign) NSInteger ipv4SecondaryDNS_AP3_2;
@property (assign) NSInteger ipv4SecondaryDNS_AP3_3;
@property (assign) NSInteger ipv4SecondaryDNS_AP3_4;
@property (assign) NSInteger subnetMask_AP3;
@property (retain) NSString *subnetMaskString_AP3;

// User Settings
@property (assign) NSInteger consoleType;
@property (copy) NSString *nickname;
@property (copy) NSString *message;
@property (assign) NSInteger favoriteColor;
@property (assign) NSDate *birthday;
@property (assign) NSInteger language;
@property (assign) NSInteger backlightLevel;

- (void) update;
- (uint32_t) generateRandomMACAddress;

@end
