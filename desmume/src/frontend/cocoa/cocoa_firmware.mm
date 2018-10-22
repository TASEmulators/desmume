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

@synthesize internalData;

@dynamic MACAddressString;

@synthesize ipv4Address_AP1_1;
@synthesize ipv4Address_AP1_2;
@synthesize ipv4Address_AP1_3;
@synthesize ipv4Address_AP1_4;
@synthesize ipv4Gateway_AP1_1;
@synthesize ipv4Gateway_AP1_2;
@synthesize ipv4Gateway_AP1_3;
@synthesize ipv4Gateway_AP1_4;
@synthesize ipv4PrimaryDNS_AP1_1;
@synthesize ipv4PrimaryDNS_AP1_2;
@synthesize ipv4PrimaryDNS_AP1_3;
@synthesize ipv4PrimaryDNS_AP1_4;
@synthesize ipv4SecondaryDNS_AP1_1;
@synthesize ipv4SecondaryDNS_AP1_2;
@synthesize ipv4SecondaryDNS_AP1_3;
@synthesize ipv4SecondaryDNS_AP1_4;
@dynamic subnetMask_AP1;
@synthesize subnetMaskString_AP1;

@synthesize ipv4Address_AP2_1;
@synthesize ipv4Address_AP2_2;
@synthesize ipv4Address_AP2_3;
@synthesize ipv4Address_AP2_4;
@synthesize ipv4Gateway_AP2_1;
@synthesize ipv4Gateway_AP2_2;
@synthesize ipv4Gateway_AP2_3;
@synthesize ipv4Gateway_AP2_4;
@synthesize ipv4PrimaryDNS_AP2_1;
@synthesize ipv4PrimaryDNS_AP2_2;
@synthesize ipv4PrimaryDNS_AP2_3;
@synthesize ipv4PrimaryDNS_AP2_4;
@synthesize ipv4SecondaryDNS_AP2_1;
@synthesize ipv4SecondaryDNS_AP2_2;
@synthesize ipv4SecondaryDNS_AP2_3;
@synthesize ipv4SecondaryDNS_AP2_4;
@dynamic subnetMask_AP2;
@synthesize subnetMaskString_AP2;

@synthesize ipv4Address_AP3_1;
@synthesize ipv4Address_AP3_2;
@synthesize ipv4Address_AP3_3;
@synthesize ipv4Address_AP3_4;
@synthesize ipv4Gateway_AP3_1;
@synthesize ipv4Gateway_AP3_2;
@synthesize ipv4Gateway_AP3_3;
@synthesize ipv4Gateway_AP3_4;
@synthesize ipv4PrimaryDNS_AP3_1;
@synthesize ipv4PrimaryDNS_AP3_2;
@synthesize ipv4PrimaryDNS_AP3_3;
@synthesize ipv4PrimaryDNS_AP3_4;
@synthesize ipv4SecondaryDNS_AP3_1;
@synthesize ipv4SecondaryDNS_AP3_2;
@synthesize ipv4SecondaryDNS_AP3_3;
@synthesize ipv4SecondaryDNS_AP3_4;
@dynamic subnetMask_AP3;
@synthesize subnetMaskString_AP3;

@dynamic consoleType;
@dynamic nickname;
@dynamic message;
@dynamic favoriteColor;
@dynamic birthday;
@dynamic language;
@synthesize backlightLevel;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	internalData = (struct NDS_fw_config_data *)malloc(sizeof(struct NDS_fw_config_data));
	if (internalData == nil)
	{
		[self release];
		return nil;
	}
	
	NDS_FillDefaultFirmwareConfigData(internalData);
	
	_MACAddress[0] = 0x00;
	_MACAddress[1] = 0x09;
	_MACAddress[2] = 0xBF;
	_MACAddress[3] = 0x12;
	_MACAddress[4] = 0x34;
	_MACAddress[5] = 0x56;
	
	_birth_year = 1970;
	backlightLevel = 3;
	
	ipv4Address_AP1_1 = 0;
	ipv4Address_AP1_2 = 0;
	ipv4Address_AP1_3 = 0;
	ipv4Address_AP1_4 = 0;
	ipv4Gateway_AP1_1 = 0;
	ipv4Gateway_AP1_2 = 0;
	ipv4Gateway_AP1_3 = 0;
	ipv4Gateway_AP1_4 = 0;
	ipv4PrimaryDNS_AP1_1 = 0;
	ipv4PrimaryDNS_AP1_2 = 0;
	ipv4PrimaryDNS_AP1_3 = 0;
	ipv4PrimaryDNS_AP1_4 = 0;
	ipv4SecondaryDNS_AP1_1 = 0;
	ipv4SecondaryDNS_AP1_2 = 0;
	ipv4SecondaryDNS_AP1_3 = 0;
	ipv4SecondaryDNS_AP1_4 = 0;
	subnetMask_AP1 = 0;
	subnetMaskString_AP1 = @"0.0.0.0";
	
	ipv4Address_AP2_1 = 0;
	ipv4Address_AP2_2 = 0;
	ipv4Address_AP2_3 = 0;
	ipv4Address_AP2_4 = 0;
	ipv4Gateway_AP2_1 = 0;
	ipv4Gateway_AP2_2 = 0;
	ipv4Gateway_AP2_3 = 0;
	ipv4Gateway_AP2_4 = 0;
	ipv4PrimaryDNS_AP2_1 = 0;
	ipv4PrimaryDNS_AP2_2 = 0;
	ipv4PrimaryDNS_AP2_3 = 0;
	ipv4PrimaryDNS_AP2_4 = 0;
	ipv4SecondaryDNS_AP2_1 = 0;
	ipv4SecondaryDNS_AP2_2 = 0;
	ipv4SecondaryDNS_AP2_3 = 0;
	ipv4SecondaryDNS_AP2_4 = 0;
	subnetMask_AP2 = 0;
	subnetMaskString_AP2 = @"0.0.0.0";
	
	ipv4Address_AP3_1 = 0;
	ipv4Address_AP3_2 = 0;
	ipv4Address_AP3_3 = 0;
	ipv4Address_AP3_4 = 0;
	ipv4Gateway_AP3_1 = 0;
	ipv4Gateway_AP3_2 = 0;
	ipv4Gateway_AP3_3 = 0;
	ipv4Gateway_AP3_4 = 0;
	ipv4PrimaryDNS_AP3_1 = 0;
	ipv4PrimaryDNS_AP3_2 = 0;
	ipv4PrimaryDNS_AP3_3 = 0;
	ipv4PrimaryDNS_AP3_4 = 0;
	ipv4SecondaryDNS_AP3_1 = 0;
	ipv4SecondaryDNS_AP3_2 = 0;
	ipv4SecondaryDNS_AP3_3 = 0;
	ipv4SecondaryDNS_AP3_4 = 0;
	subnetMask_AP3 = 0;
	subnetMaskString_AP3 = @"0.0.0.0";
	
	pthread_mutex_init(&mutex, NULL);
	
	return self;
}

- (void)dealloc
{
	free(internalData);
	internalData = NULL;
		
	pthread_mutex_destroy(&mutex);
	
	[self setSubnetMaskString_AP1:nil];
	[self setSubnetMaskString_AP2:nil];
	[self setSubnetMaskString_AP3:nil];
	
	[super dealloc];
}

- (void) setMACAddress:(const uint8_t *)inMACAddress
{
	if (inMACAddress == NULL)
	{
		return;
	}
	
	pthread_mutex_lock(&mutex);
	_MACAddress[0] = 0x00;
	_MACAddress[1] = 0x09;
	_MACAddress[2] = 0xBF;
	_MACAddress[3] = inMACAddress[3];
	_MACAddress[4] = inMACAddress[4];
	_MACAddress[5] = inMACAddress[5];
	pthread_mutex_unlock(&mutex);
	
	[self setMACAddressString:NULL];
}

- (uint8_t *) MACAddress
{
	return _MACAddress;
}

- (void) setMACAddressString:(NSString *)theMACAddressString
{
	// Do nothing. This is here for KVO-compliance only.
}

- (NSString *) MACAddressString
{
	pthread_mutex_lock(&mutex);
	NSString *theMACAddressString = [NSString stringWithFormat:@"%02X:%02X:%02X:%02X:%02X:%02X",
									 _MACAddress[0], _MACAddress[1], _MACAddress[2], _MACAddress[3], _MACAddress[4], _MACAddress[5]];
	pthread_mutex_unlock(&mutex);
	
	return theMACAddressString;
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
	subnetMask_AP1 = subnetMask;
	const uint32_t subnetMaskValue = (subnetMask_AP1 == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMask_AP1));
	pthread_mutex_unlock(&mutex);
	
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
	const NSInteger subnetMask = subnetMask_AP1;
	pthread_mutex_unlock(&mutex);
	
	return subnetMask;
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
	subnetMask_AP2 = subnetMask;
	const uint32_t subnetMaskValue = (subnetMask_AP2 == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMask_AP2));
	pthread_mutex_unlock(&mutex);
	
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
	const NSInteger subnetMask = subnetMask_AP2;
	pthread_mutex_unlock(&mutex);
	
	return subnetMask;
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
	subnetMask_AP3 = subnetMask;
	const uint32_t subnetMaskValue = (subnetMask_AP3 == 0) ? 0 : (0xFFFFFFFF << (32 - subnetMask_AP3));
	pthread_mutex_unlock(&mutex);
	
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
	const NSInteger subnetMask = subnetMask_AP3;
	pthread_mutex_unlock(&mutex);
	
	return subnetMask;
}

- (void) setConsoleType:(NSInteger)theType
{
	pthread_mutex_lock(&mutex);
	internalData->ds_type = (NDS_CONSOLE_TYPE)theType;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) consoleType
{
	pthread_mutex_lock(&mutex);
	NSInteger theType = internalData->ds_type;
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
		
		[theNickname getBytes:&internalData->nickname[0]
					maxLength:(sizeof(UInt16) * characterRange.length)
				   usedLength:NULL
					 encoding:NSUTF16LittleEndianStringEncoding
					  options:NSStringEncodingConversionAllowLossy
						range:characterRange
			   remainingRange:NULL];
		
		internalData->nickname_len = (u8)characterRange.length;
	}
	else
	{
		memset(&internalData->nickname[0], 0, sizeof(internalData->nickname));
		internalData->nickname_len = 0;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSString *) nickname
{
	pthread_mutex_lock(&mutex);
	NSString *theNickname = [[[NSString alloc] initWithBytes:&internalData->nickname[0] length:(sizeof(UInt16) * internalData->nickname_len) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
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
		
		[theMessage getBytes:&internalData->message[0]
				   maxLength:(sizeof(UInt16) * characterRange.length)
				  usedLength:NULL
					encoding:NSUTF16LittleEndianStringEncoding
					 options:NSStringEncodingConversionAllowLossy
					   range:characterRange
			  remainingRange:NULL];
		
		internalData->message_len = (u8)characterRange.length;
	}
	else
	{
		memset(&internalData->message[0], 0, sizeof(internalData->message));
		internalData->message_len = 0;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSString *) message
{
	pthread_mutex_lock(&mutex);
	NSString *theMessage = [[[NSString alloc] initWithBytes:&internalData->message[0] length:(sizeof(UInt16) * internalData->message_len) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
	pthread_mutex_unlock(&mutex);
	
	return theMessage;
}

- (void) setFavoriteColor:(NSInteger)colorID
{
	pthread_mutex_lock(&mutex);
	internalData->fav_colour = (u8)colorID;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) favoriteColor
{
	pthread_mutex_lock(&mutex);
	NSInteger theFavoriteColorID = internalData->fav_colour;
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
		
		internalData->birth_month = (u8)theMonth;
		internalData->birth_day = (u8)theDay;
		_birth_year = theYear;
	}
	else
	{
		internalData->birth_month = 1;
		internalData->birth_day = 1;
		_birth_year = 1970;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSDate *) birthday
{
	pthread_mutex_lock(&mutex);
	NSDate *theBirthday = [NSDate dateWithString:[NSString stringWithFormat:@"%ld-%ld-%ld 12:00:00 +0000", (unsigned long)_birth_year, (unsigned long)internalData->birth_month, (unsigned long)internalData->birth_day]];
	pthread_mutex_unlock(&mutex);
	
	return theBirthday;
}

- (void) setLanguage:(NSInteger)languageID
{
	pthread_mutex_lock(&mutex);
	internalData->language = (u8)languageID;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) language
{
	pthread_mutex_lock(&mutex);
	NSInteger theLanguageID = internalData->language;
	pthread_mutex_unlock(&mutex);
	
	return theLanguageID;
}

// TODO: Remove this function after firmware.cpp is reworked.
// This is just a direct copy-paste of the function from there.
static u32 calc_CRC16( u32 start, const u8 *data, int count)
{
	int i,j;
	u32 crc = start & 0xffff;
	const u16 val[8] = { 0xC0C1,0xC181,0xC301,0xC601,0xCC01,0xD801,0xF001,0xA001 };
	for(i = 0; i < count; i++)
	{
		crc = crc ^ data[i];
		
		for(j = 0; j < 8; j++) {
			int do_bit = 0;
			
			if ( crc & 0x1)
				do_bit = 1;
			
			crc = crc >> 1;
			
			if ( do_bit) {
				crc = crc ^ (val[j] << (7-j));
			}
		}
	}
	return crc;
}

- (void) update
{
	// Write the attributes to the DS via the data struct.
	// We have make a copy of the struct and send that so that the firmware
	// changes get saved in CommonSettings.InternalFirmwareConf.
	pthread_mutex_lock(&mutex);
	
	struct NDS_fw_config_data newFirmwareData = *internalData;
	NDS_CreateDummyFirmware(&newFirmwareData);
	
	// WiFi Info
	MMU.fw.data[0x36] = _MACAddress[0];
	MMU.fw.data[0x37] = _MACAddress[1];
	MMU.fw.data[0x38] = _MACAddress[2];
	MMU.fw.data[0x39] = _MACAddress[3];
	MMU.fw.data[0x3A] = _MACAddress[4];
	MMU.fw.data[0x3B] = _MACAddress[5];
	*(u16 *)&MMU.fw.data[0x2A] = calc_CRC16(0, &MMU.fw.data[0x2C], 0x138);
	
	// Access Point Settings
	MMU.fw.data[0x3FA40] = 'S';
	MMU.fw.data[0x3FA41] = 'o';
	MMU.fw.data[0x3FA42] = 'f';
	MMU.fw.data[0x3FA43] = 't';
	MMU.fw.data[0x3FA44] = 'A';
	MMU.fw.data[0x3FA45] = 'P';
	MMU.fw.data[0x3FAC0] = ipv4Address_AP1_1;
	MMU.fw.data[0x3FAC1] = ipv4Address_AP1_2;
	MMU.fw.data[0x3FAC2] = ipv4Address_AP1_3;
	MMU.fw.data[0x3FAC3] = ipv4Address_AP1_4;
	MMU.fw.data[0x3FAC4] = ipv4Gateway_AP1_1;
	MMU.fw.data[0x3FAC5] = ipv4Gateway_AP1_2;
	MMU.fw.data[0x3FAC6] = ipv4Gateway_AP1_3;
	MMU.fw.data[0x3FAC7] = ipv4Gateway_AP1_4;
	MMU.fw.data[0x3FAC8] = ipv4PrimaryDNS_AP1_1;
	MMU.fw.data[0x3FAC9] = ipv4PrimaryDNS_AP1_2;
	MMU.fw.data[0x3FACA] = ipv4PrimaryDNS_AP1_3;
	MMU.fw.data[0x3FACB] = ipv4PrimaryDNS_AP1_4;
	MMU.fw.data[0x3FACC] = ipv4SecondaryDNS_AP1_1;
	MMU.fw.data[0x3FACD] = ipv4SecondaryDNS_AP1_2;
	MMU.fw.data[0x3FACE] = ipv4SecondaryDNS_AP1_3;
	MMU.fw.data[0x3FACF] = ipv4SecondaryDNS_AP1_4;
	MMU.fw.data[0x3FAD0] = subnetMask_AP1;
	MMU.fw.data[0x3FAE7] = 0;
	*(u16 *)&MMU.fw.data[0x3FAFE] = calc_CRC16(0, &MMU.fw.data[0x3FA00], 0xFE);
	
	MMU.fw.data[0x3FB40] = 'S';
	MMU.fw.data[0x3FB41] = 'o';
	MMU.fw.data[0x3FB42] = 'f';
	MMU.fw.data[0x3FB43] = 't';
	MMU.fw.data[0x3FB44] = 'A';
	MMU.fw.data[0x3FB45] = 'P';
	MMU.fw.data[0x3FBC0] = ipv4Address_AP2_1;
	MMU.fw.data[0x3FBC1] = ipv4Address_AP2_2;
	MMU.fw.data[0x3FBC2] = ipv4Address_AP2_3;
	MMU.fw.data[0x3FBC3] = ipv4Address_AP2_4;
	MMU.fw.data[0x3FBC4] = ipv4Gateway_AP2_1;
	MMU.fw.data[0x3FBC5] = ipv4Gateway_AP2_2;
	MMU.fw.data[0x3FBC6] = ipv4Gateway_AP2_3;
	MMU.fw.data[0x3FBC7] = ipv4Gateway_AP2_4;
	MMU.fw.data[0x3FBC8] = ipv4PrimaryDNS_AP2_1;
	MMU.fw.data[0x3FBC9] = ipv4PrimaryDNS_AP2_2;
	MMU.fw.data[0x3FBCA] = ipv4PrimaryDNS_AP2_3;
	MMU.fw.data[0x3FBCB] = ipv4PrimaryDNS_AP2_4;
	MMU.fw.data[0x3FBCC] = ipv4SecondaryDNS_AP2_1;
	MMU.fw.data[0x3FBCD] = ipv4SecondaryDNS_AP2_2;
	MMU.fw.data[0x3FBCE] = ipv4SecondaryDNS_AP2_3;
	MMU.fw.data[0x3FBCF] = ipv4SecondaryDNS_AP2_4;
	MMU.fw.data[0x3FBD0] = subnetMask_AP2;
	MMU.fw.data[0x3FBE7] = 0;
	*(u16 *)&MMU.fw.data[0x3FBFE] = calc_CRC16(0, &MMU.fw.data[0x3FB00], 0xFE);
	
	MMU.fw.data[0x3FC40] = 'S';
	MMU.fw.data[0x3FC41] = 'o';
	MMU.fw.data[0x3FC42] = 'f';
	MMU.fw.data[0x3FC43] = 't';
	MMU.fw.data[0x3FC44] = 'A';
	MMU.fw.data[0x3FC45] = 'P';
	MMU.fw.data[0x3FCC0] = ipv4Address_AP3_1;
	MMU.fw.data[0x3FCC1] = ipv4Address_AP3_2;
	MMU.fw.data[0x3FCC2] = ipv4Address_AP3_3;
	MMU.fw.data[0x3FCC3] = ipv4Address_AP3_4;
	MMU.fw.data[0x3FCC4] = ipv4Gateway_AP3_1;
	MMU.fw.data[0x3FCC5] = ipv4Gateway_AP3_2;
	MMU.fw.data[0x3FCC6] = ipv4Gateway_AP3_3;
	MMU.fw.data[0x3FCC7] = ipv4Gateway_AP3_4;
	MMU.fw.data[0x3FCC8] = ipv4PrimaryDNS_AP3_1;
	MMU.fw.data[0x3FCC9] = ipv4PrimaryDNS_AP3_2;
	MMU.fw.data[0x3FCCA] = ipv4PrimaryDNS_AP3_3;
	MMU.fw.data[0x3FCCB] = ipv4PrimaryDNS_AP3_4;
	MMU.fw.data[0x3FCCC] = ipv4SecondaryDNS_AP3_1;
	MMU.fw.data[0x3FCCD] = ipv4SecondaryDNS_AP3_2;
	MMU.fw.data[0x3FCCE] = ipv4SecondaryDNS_AP3_3;
	MMU.fw.data[0x3FCCF] = ipv4SecondaryDNS_AP3_4;
	MMU.fw.data[0x3FCD0] = subnetMask_AP3;
	MMU.fw.data[0x3FCE7] = 0;
	*(u16 *)&MMU.fw.data[0x3FCFE] = calc_CRC16(0, &MMU.fw.data[0x3FC00], 0xFE);
	
	pthread_mutex_unlock(&mutex);
}

@end
