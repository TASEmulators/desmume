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

#import <Cocoa/Cocoa.h>


/********************************************************************************************
	CocoaDSFirmware - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for DeSmuME's firmware struct.

	If this object is instantiated through any init method other than initWithFirmwareData:,
	this object allocate memory for its own internal firmware struct. This memory is then
	freed upon the release of this object.

	If this object is instantiated using initWithFirmwareData:, or if a firmware struct is
	assigned through the data property, the firmware struct is not freed upon the release
	of this object. This is by design.
 
	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
@interface CocoaDSFirmware : NSObject
{
	struct NDS_fw_config_data *data;
	struct NDS_fw_config_data *internalData;
	NSUInteger birth_year;
	pthread_mutex_t mutex;
}

@property (assign) struct NDS_fw_config_data *data;
@property (assign) NSInteger consoleType;
@property (copy) NSString *nickname;
@property (copy) NSString *message;
@property (assign) NSInteger favoriteColor;
@property (assign) NSDate *birthday;
@property (assign) NSInteger language;

- (id) initWithDictionary:(NSDictionary *)dataDict;
- (id) initWithFirmwareData:(struct NDS_fw_config_data *)fwData;
- (void) update;
- (void) setDataWithDictionary:(NSDictionary *)fwDataDict;
- (NSDictionary *) dataDictionary;

@end
