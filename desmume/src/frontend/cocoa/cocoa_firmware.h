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

#import <Cocoa/Cocoa.h>

class ClientExecutionControl;
class ClientFirmwareControl;
struct FirmwareConfig;

/********************************************************************************************
	CocoaDSFirmware - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for DeSmuME's firmware struct.
 
	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
@interface CocoaDSFirmware : NSObject
{
	ClientExecutionControl *execControl;
	ClientFirmwareControl *_fwControl;
	FirmwareConfig *_appliedFWConfig;
	NSUInteger _birth_year;
	
	NSString *firmwareMACAddressString;
	NSString *firmwareMACAddressPendingString;
	NSString *customMACAddress1String;
	NSString *customMACAddress2String;
	NSString *customMACAddress3String;
	NSString *customMACAddress4String;
	
	NSString *subnetMaskString_AP1;
	NSString *subnetMaskString_AP2;
	NSString *subnetMaskString_AP3;
}

@property (assign, nonatomic) ClientExecutionControl *execControl;

// WiFi Info
@property (assign, nonatomic) NSInteger addressSelection;
@property (assign, nonatomic) uint32_t firmwareMACAddressPendingValue;
@property (readonly, nonatomic) uint32_t firmwareMACAddressValue;
@property (retain) NSString *firmwareMACAddressPendingString;
@property (retain) NSString *firmwareMACAddressString;
@property (assign, nonatomic) uint32_t customMACAddressValue;
@property (retain) NSString *customMACAddress1String;
@property (retain) NSString *customMACAddress2String;
@property (retain) NSString *customMACAddress3String;
@property (retain) NSString *customMACAddress4String;

// Access Point Settings
@property (assign, nonatomic) NSInteger ipv4Address_AP1_1;
@property (assign, nonatomic) NSInteger ipv4Address_AP1_2;
@property (assign, nonatomic) NSInteger ipv4Address_AP1_3;
@property (assign, nonatomic) NSInteger ipv4Address_AP1_4;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP1_1;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP1_2;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP1_3;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP1_4;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP1_1;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP1_2;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP1_3;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP1_4;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP1_1;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP1_2;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP1_3;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP1_4;
@property (assign, nonatomic) NSInteger subnetMask_AP1;
@property (retain) NSString *subnetMaskString_AP1;

@property (assign, nonatomic) NSInteger ipv4Address_AP2_1;
@property (assign, nonatomic) NSInteger ipv4Address_AP2_2;
@property (assign, nonatomic) NSInteger ipv4Address_AP2_3;
@property (assign, nonatomic) NSInteger ipv4Address_AP2_4;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP2_1;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP2_2;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP2_3;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP2_4;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP2_1;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP2_2;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP2_3;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP2_4;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP2_1;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP2_2;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP2_3;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP2_4;
@property (assign, nonatomic) NSInteger subnetMask_AP2;
@property (retain) NSString *subnetMaskString_AP2;

@property (assign, nonatomic) NSInteger ipv4Address_AP3_1;
@property (assign, nonatomic) NSInteger ipv4Address_AP3_2;
@property (assign, nonatomic) NSInteger ipv4Address_AP3_3;
@property (assign, nonatomic) NSInteger ipv4Address_AP3_4;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP3_1;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP3_2;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP3_3;
@property (assign, nonatomic) NSInteger ipv4Gateway_AP3_4;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP3_1;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP3_2;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP3_3;
@property (assign, nonatomic) NSInteger ipv4PrimaryDNS_AP3_4;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP3_1;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP3_2;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP3_3;
@property (assign, nonatomic) NSInteger ipv4SecondaryDNS_AP3_4;
@property (assign, nonatomic) NSInteger subnetMask_AP3;
@property (retain) NSString *subnetMaskString_AP3;

// User Settings
@property (assign, nonatomic) NSInteger consoleType;
@property (copy, nonatomic) NSString *nickname;
@property (copy, nonatomic) NSString *message;
@property (assign, nonatomic) NSInteger favoriteColor;
@property (assign, nonatomic) NSDate *birthday;
@property (assign, nonatomic) NSInteger language;
@property (assign, nonatomic) NSInteger backlightLevel;

- (void) applySettings;
- (void) updateFirmwareConfigSessionValues;
- (uint32_t) generateRandomFirmwareMACAddress;
- (uint32_t) generateRandomCustomMACAddress;
- (void) writeUserDefaultWFCUserID;

@end
