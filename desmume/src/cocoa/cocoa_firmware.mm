/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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
#include "../NDSSystem.h"


@implementation CocoaDSFirmware

@synthesize data;
@dynamic consoleType;
@dynamic nickname;
@dynamic message;
@dynamic favoriteColor;
@dynamic birthday;
@dynamic language;

- (id)init
{
	return [self initWithDictionary:nil];
}

- (id) initWithDictionary:(NSDictionary *)fwDataDict
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	pthread_mutex_init(&mutex, NULL);
	
	// Allocate our own firmware struct since we weren't provided with one.
	internalData = (struct NDS_fw_config_data *)malloc(sizeof(struct NDS_fw_config_data));
	if (internalData == nil)
	{
		pthread_mutex_destroy(&mutex);
		[self release];
		return nil;
	}
	
	NDS_FillDefaultFirmwareConfigData(internalData);
	
	data = internalData;
	
	[self setDataWithDictionary:fwDataDict];
	
	return self;
}

- (id) initWithFirmwareData:(struct NDS_fw_config_data *)fwData
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	pthread_mutex_init(&mutex, NULL);
	
	if (fwData == nil)
	{
		pthread_mutex_destroy(&mutex);
		[self release];
		return nil;
	}
	
	// Set birth_year to the current year.
	NSDate *now = [[NSDate alloc] init];
	NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
	[dateFormatter setDateFormat:@"Y"];
	birth_year = [[dateFormatter stringFromDate:now] integerValue];
	[dateFormatter release];
	[now release];
	
	internalData = NULL;
	data = fwData;
	
	return self;
}

- (void)dealloc
{
	free(internalData);
	internalData = NULL;
		
	pthread_mutex_destroy(&mutex);
	
	[super dealloc];
}

- (void) setConsoleType:(NSInteger)theType
{
	pthread_mutex_lock(&mutex);
	data->ds_type = (NDS_CONSOLE_TYPE)theType;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) consoleType
{
	pthread_mutex_lock(&mutex);
	NSInteger theType = data->ds_type;
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
		
		[theNickname getBytes:&data->nickname[0]
					maxLength:(sizeof(UInt16) * characterRange.length)
				   usedLength:NULL
					 encoding:NSUTF16LittleEndianStringEncoding
					  options:NSStringEncodingConversionAllowLossy
						range:characterRange
			   remainingRange:NULL];
		
		data->nickname_len = (u8)characterRange.length;
	}
	else
	{
		memset(&data->nickname[0], 0, sizeof(data->nickname));
		data->nickname_len = 0;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSString *) nickname
{
	pthread_mutex_lock(&mutex);
	NSString *theNickname = [[[NSString alloc] initWithBytes:&data->nickname[0] length:(sizeof(UInt16) * data->nickname_len) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
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
		
		[theMessage getBytes:&data->message[0]
				   maxLength:(sizeof(UInt16) * characterRange.length)
				  usedLength:NULL
					encoding:NSUTF16LittleEndianStringEncoding
					 options:NSStringEncodingConversionAllowLossy
					   range:characterRange
			  remainingRange:NULL];
		
		data->message_len = (u8)characterRange.length;
	}
	else
	{
		memset(&data->message[0], 0, sizeof(data->message));
		data->message_len = 0;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSString *) message
{
	pthread_mutex_lock(&mutex);
	NSString *theMessage = [[[NSString alloc] initWithBytes:&data->message[0] length:(sizeof(UInt16) * data->message_len) encoding:NSUTF16LittleEndianStringEncoding] autorelease];
	pthread_mutex_unlock(&mutex);
	
	return theMessage;
}

- (void) setFavoriteColor:(NSInteger)colorID
{
	pthread_mutex_lock(&mutex);
	data->fav_colour = (u8)colorID;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) favoriteColor
{
	pthread_mutex_lock(&mutex);
	NSInteger theFavoriteColorID = data->fav_colour;
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
		
		data->birth_month = (u8)theMonth;
		data->birth_day = (u8)theDay;
		birth_year = theYear;
	}
	else
	{
		data->birth_month = 1;
		data->birth_day = 1;
		birth_year = 1970;
	}
	
	pthread_mutex_unlock(&mutex);
}

- (NSDate *) birthday
{
	pthread_mutex_lock(&mutex);
	NSDate *theBirthday = [NSDate dateWithString:[NSString stringWithFormat:@"%ld-%ld-%ld 12:00:00 +0000", (unsigned long)birth_year, (unsigned long)data->birth_month, (unsigned long)data->birth_day]];
	pthread_mutex_unlock(&mutex);
	
	return theBirthday;
}

- (void) setLanguage:(NSInteger)languageID
{
	pthread_mutex_lock(&mutex);
	data->language = (u8)languageID;
	pthread_mutex_unlock(&mutex);
}

- (NSInteger) language
{
	pthread_mutex_lock(&mutex);
	NSInteger theLanguageID = data->language;
	pthread_mutex_unlock(&mutex);
	
	return theLanguageID;
}

- (void) update
{
	// Write the attributes to the DS via the data struct.
	// We have make a copy of the struct and send that so that the firmware
	// changes get saved in CommonSettings.InternalFirmwareConf.
	pthread_mutex_lock(&mutex);
	struct NDS_fw_config_data newFirmwareData = *data;
	NDS_CreateDummyFirmware(&newFirmwareData);
	pthread_mutex_unlock(&mutex);
}

- (void) setDataWithDictionary:(NSDictionary *)dataDict
{
	if (dataDict == nil)
	{
		return;
	}
	
	[self setConsoleType:[(NSNumber *)[dataDict valueForKey:@"ConsoleType"] integerValue]];
	[self setNickname:(NSString *)[dataDict valueForKey:@"Nickname"]];
	[self setMessage:(NSString *)[dataDict valueForKey:@"Message"]];
	[self setFavoriteColor:[(NSNumber *)[dataDict valueForKey:@"FavoriteColor"] integerValue]];
	[self setBirthday:(NSDate *)[dataDict valueForKey:@"Birthday"]];
	[self setLanguage:[(NSNumber *)[dataDict valueForKey:@"Language"] integerValue]];
}

- (NSDictionary *) dataDictionary
{
	return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithInteger:[self consoleType]], @"ConsoleType",
			[self nickname], @"Nickname",
			[self message], @"Message",
			[NSNumber numberWithInteger:[self favoriteColor]], @"FavoriteColor",
			[self birthday], @"Birthday",
			[NSNumber numberWithInteger:[self language]], @"Language",
			nil];
}

@end
