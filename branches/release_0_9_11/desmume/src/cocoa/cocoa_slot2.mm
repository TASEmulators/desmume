/*
	Copyright (C) 2014-2015 DeSmuME Team

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

#import "cocoa_slot2.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"


@implementation CocoaDSSlot2Device

@dynamic name;
@dynamic description;
@dynamic deviceID;
@dynamic type;
@synthesize enabled;


- (id) initWithDeviceData:(ISlot2Interface *)deviceData
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	device = deviceData;
	enabled = NO;
	
	return self;
}

- (void) dealloc
{
	[super dealloc];
}

- (NSString *) name
{
	const char *cDeviceName = device->info()->name();
	NSString *theName = (cDeviceName != NULL) ? [NSString stringWithCString:cDeviceName encoding:NSUTF8StringEncoding] : @"";
	
	return theName;
}

- (NSString *) description
{
	const char *cDeviceDescription = device->info()->descr();
	NSString *theDesc = (cDeviceDescription != NULL) ? [NSString stringWithCString:cDeviceDescription encoding:NSUTF8StringEncoding] : @"";
	
	return theDesc;
}

- (NSInteger) deviceID
{
	return (NSInteger)device->info()->id();
}

- (NDS_SLOT2_TYPE) type
{
	NDS_SLOT2_TYPE theType = NDS_SLOT2_NONE;
	slot2_getTypeByID(device->info()->id(), theType);
	return theType;
}

@end

@implementation CocoaDSSlot2Manager

@synthesize deviceList;
@dynamic currentDevice;
@synthesize slot2StatusText;
@dynamic mpcfFileSearchURL;
@dynamic gbaCartridgeURL;
@dynamic gbaSRamURL;
@dynamic doesGbaCartridgeSaveExist;

- (id) init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	deviceList = [[NSMutableArray alloc] initWithCapacity:32];
	slot2StatusText = NSSTRING_STATUS_SLOT2_LOADED_NONE;
	[self setGbaCartridgeURL:nil];
	
	if (FeedbackON == NULL)
	{
		FeedbackON = &OSXSendForceFeedbackState;
	}
	
	return self;
}

- (void) dealloc
{
	[deviceList release];
	
	[super dealloc];
}

- (void) setCurrentDevice:(CocoaDSSlot2Device *)theDevice
{
	NDS_SLOT2_TYPE theType = NDS_SLOT2_NONE;
	
	if (theDevice != nil)
	{
		theType = [theDevice type];
		[theDevice retain];
	}
	
	bool slotDidChange = slot2_Change(theType);
	if (slotDidChange || currentDevice == nil)
	{
		[currentDevice release];
		currentDevice = theDevice;
	}
	
	[self updateStatus];
}

- (CocoaDSSlot2Device *) currentDevice
{
	return currentDevice;
}

- (void) setMpcfFileSearchURL:(NSURL *)theURL
{
	if (theURL != nil)
	{
		BOOL isDirectory = NO;
		NSString *thePath = [theURL path];
		
		NSFileManager *fileManager = [[NSFileManager alloc] init];
		[fileManager fileExistsAtPath:thePath isDirectory:&isDirectory];
		
		CFlash_Mode = (isDirectory) ? ADDON_CFLASH_MODE_Path : ADDON_CFLASH_MODE_File;
		CFlash_Path = [thePath cStringUsingEncoding:NSUTF8StringEncoding];
		
		[fileManager release];
	}
	else
	{
		CFlash_Path.clear();
		CFlash_Mode = ADDON_CFLASH_MODE_RomPath;
	}
}

- (NSURL *) mpcfFileSearchURL
{
	return [NSURL fileURLWithPath:[NSString stringWithCString:CFlash_Path.c_str() encoding:NSUTF8StringEncoding]];
}

- (void) setGbaCartridgeURL:(NSURL *)fileURL
{
	if (fileURL != nil)
	{
		GBACartridge_RomPath = [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	}
	else
	{
		GBACartridge_RomPath.clear();
	}
}

- (NSURL *) gbaCartridgeURL
{
	return [NSURL fileURLWithPath:[NSString stringWithCString:GBACartridge_RomPath.c_str() encoding:NSUTF8StringEncoding]];
}

- (void) setGbaSRamURL:(NSURL *)fileURL
{
	if (fileURL != nil)
	{
		GBACartridge_SRAMPath = [[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding];
	}
	else
	{
		GBACartridge_SRAMPath.clear();
	}
}

- (NSURL *) gbaSRamURL;
{
	return [NSURL fileURLWithPath:[NSString stringWithCString:GBACartridge_SRAMPath.c_str() encoding:NSUTF8StringEncoding]];
}

- (BOOL) doesGbaCartridgeSaveExist
{
	return NO;
}

- (CocoaDSSlot2Device *) autoSelectedDevice
{
	return [self findDeviceByType:slot2_DetermineType()];
}

- (NSString *) autoSelectedDeviceName
{
	return [[self autoSelectedDevice] name];
}

- (CocoaDSSlot2Device *) findDeviceByType:(NDS_SLOT2_TYPE)theType
{
	CocoaDSSlot2Device *foundDevice = nil;
	
	for (CocoaDSSlot2Device *theDevice in deviceList)
	{
		if (theType == [theDevice type])
		{
			foundDevice = theDevice;
			return foundDevice;
		}
	}
	
	return foundDevice;
}

- (void) setDeviceByType:(NDS_SLOT2_TYPE)theType
{
	CocoaDSSlot2Device *theDevice = [self findDeviceByType:theType];
	if (theDevice != nil)
	{
		[self setCurrentDevice:theDevice];
	}
	else
	{
		// If no devices are found, just set it to NDS_SLOT2_NONE by default.
		slot2_Change(NDS_SLOT2_NONE);
	}
}

- (void) updateDeviceList
{
	BOOL didSelectDevice = NO;
	
	[deviceList removeAllObjects];
	
	for (size_t i = 0; i < NDS_SLOT2_COUNT; i++)
	{
		ISlot2Interface *theDevice = slot2_List[i];
		if (theDevice == NULL)
		{
			continue;
		}
		
		// Create a new device wrapper object and add it to the device list.
		CocoaDSSlot2Device *newCdsDevice = [[[CocoaDSSlot2Device alloc] initWithDeviceData:theDevice] autorelease];
		[deviceList addObject:newCdsDevice];
		
		// Only enable the SLOT-2 devices that are ready for end-user usage, and leave
		// the remaining devices disabled for the time being.
		const NDS_SLOT2_TYPE deviceType = [newCdsDevice type];
		
		struct Slot2DeviceProperties
		{
			NDS_SLOT2_TYPE typeID;
			BOOL isSupported;
		};
		
		static const Slot2DeviceProperties supportedDeviceTypesList[] = {
			{ NDS_SLOT2_NONE, YES },
			{ NDS_SLOT2_AUTO, YES },
			{ NDS_SLOT2_CFLASH, YES },
			{ NDS_SLOT2_RUMBLEPAK, YES },
			{ NDS_SLOT2_GBACART, YES },
			{ NDS_SLOT2_GUITARGRIP, YES },
			{ NDS_SLOT2_EXPMEMORY, YES }, 
			{ NDS_SLOT2_EASYPIANO, YES },
			{ NDS_SLOT2_PADDLE, YES },
			{ NDS_SLOT2_PASSME, YES }
		};
		
		for (size_t j = 0; j < NDS_SLOT2_COUNT; j++)
		{
			if (deviceType == supportedDeviceTypesList[j].typeID)
			{
				[newCdsDevice setEnabled:supportedDeviceTypesList[j].isSupported];
				break;
			}
		}
				
		// If the new device is the current device, select it.
		if (!didSelectDevice && [newCdsDevice type] == slot2_GetCurrentType())
		{
			[self setCurrentDevice:newCdsDevice];
			didSelectDevice = YES;
		}
	}
	
	if (!didSelectDevice)
	{
		slot2_Change(NDS_SLOT2_NONE);
	}
}

- (void) updateStatus
{
	const NDS_SLOT2_TYPE theType = ([self currentDevice] != nil) ? [currentDevice type] : NDS_SLOT2_NONE;
	
	switch (theType)
	{
		case NDS_SLOT2_NONE:
			[self setSlot2StatusText:NSSTRING_STATUS_SLOT2_LOADED_NONE];
			break;
			
		case NDS_SLOT2_AUTO:
			[self setSlot2StatusText:[NSString stringWithFormat:NSSTRING_STATUS_SLOT2_LOADED_AUTOMATIC, [self autoSelectedDeviceName]]];
			break;
			
		case NDS_SLOT2_CFLASH:
		{
			switch (CFlash_Mode)
			{
				case ADDON_CFLASH_MODE_Path:
					[self setSlot2StatusText:[NSString stringWithFormat:NSSTRING_STATUS_SLOT2_LOADED_MPCF_DIRECTORY, CFlash_Path.c_str()]];
					break;
					
				case ADDON_CFLASH_MODE_File:
					[self setSlot2StatusText:[NSString stringWithFormat:NSSTRING_STATUS_SLOT2_LOADED_MPCF_DISK_IMAGE, CFlash_Path.c_str()]];
					break;
					
				case ADDON_CFLASH_MODE_RomPath:
					[self setSlot2StatusText:NSSTRING_STATUS_SLOT2_LOADED_MPCF_WITH_ROM];
					break;
					
				default:
					break;
			}
			break;
		}
			
		case NDS_SLOT2_GBACART:
		{
			[self setSlot2StatusText:(GBACartridge_SRAMPath.empty()) ? NSSTRING_STATUS_SLOT2_LOADED_GBA_CART_NO_SRAM : [NSString stringWithFormat:NSSTRING_STATUS_SLOT2_LOADED_GBA_CART_WITH_SRAM, GBACartridge_SRAMPath.c_str()]];
			break;
		}
			
		case NDS_SLOT2_RUMBLEPAK:
		case NDS_SLOT2_GUITARGRIP:
		case NDS_SLOT2_EXPMEMORY:
		case NDS_SLOT2_EASYPIANO:
		case NDS_SLOT2_PADDLE:
		case NDS_SLOT2_PASSME:
			[self setSlot2StatusText:[NSString stringWithFormat:NSSTRING_STATUS_SLOT2_LOADED_GENERIC_DEVICE, [[self currentDevice] name]]];
			break;
			
		default:
			[self setSlot2StatusText:NSSTRING_STATUS_SLOT2_LOADED_UNKNOWN];
			break;
	}
}

@end

void OSXSendForceFeedbackState(bool enable)
{
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	
	NSDictionary *ffProperties = [NSDictionary dictionaryWithObjectsAndKeys:
								  [NSNumber numberWithBool:enable], @"ffState",
								  [NSNumber numberWithInteger:RUMBLE_ITERATIONS_RUMBLE_PAK], @"iterations",
								  nil];
	
	[[NSNotificationCenter defaultCenter] postNotificationName:@"org.desmume.DeSmuME.sendForceFeedback"
														object:nil
													  userInfo:ffProperties];
	
	[tempPool release];
}
