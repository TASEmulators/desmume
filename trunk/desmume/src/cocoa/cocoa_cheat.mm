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

#import "cocoa_cheat.h"
#import "cocoa_core.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"

#include "../MMU.h"

@implementation CocoaDSCheatItem

static NSImage *iconInternalCheat = nil;
static NSImage *iconActionReplay = nil;
static NSImage *iconCodeBreaker = nil;

@dynamic data;
@synthesize willAdd;
@dynamic enabled;
@dynamic cheatType;
@dynamic cheatTypeIcon;
@dynamic isSupportedCheatType;
@dynamic freezeType;
@dynamic description;
@dynamic codeCount;
@dynamic code;
@dynamic memAddress;
@dynamic memAddressString;
@dynamic memAddressSixDigitString;
@dynamic bytes;
@dynamic value;
@synthesize workingCopy;
@synthesize parent;

- (id) init
{
	return [self initWithCheatData:nil];
}

- (id) initWithCheatData:(CHEATS_LIST *)cheatData
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	if (cheatData == NULL)
	{
		// Allocate our own cheat item struct since we weren't provided with one.
		internalData = (CHEATS_LIST *)malloc(sizeof(CHEATS_LIST));
		if (internalData == NULL)
		{
			[self release];
			return nil;
		}
		
		data = internalData;
		
		self.enabled = NO;
		self.cheatType = CHEAT_TYPE_INTERNAL;
		self.freezeType = 0;
		self.description = @"";
		self.code = @"";
		self.memAddress = 0x00000000;
		self.bytes = 1;
		self.value = 0;
	}
	else
	{
		internalData = NULL;
		data = cheatData;
	}
	
	if (iconInternalCheat == nil || iconActionReplay == nil || iconCodeBreaker == nil)
	{
		iconInternalCheat = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"AppIcon_DeSmuME" ofType:@"icns"]];
		iconActionReplay = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ActionReplay_128x128" ofType:@"png"]];
		iconCodeBreaker = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_CodeBreaker_128x128" ofType:@"png"]];
	}
	
	pthread_mutex_init(&mutexData, NULL);
	
	willAdd = NO;
	workingCopy = nil;
	parent = nil;
	
	return self;
}

- (void) dealloc
{
	[self destroyWorkingCopy];
	
	free(internalData);
	internalData = NULL;
	
	pthread_mutex_destroy(&mutexData);
	
	[super dealloc];
}

- (CHEATS_LIST *) data
{
	pthread_mutex_lock(&mutexData);
	CHEATS_LIST *returnData = data;
	pthread_mutex_unlock(&mutexData);
	
	return returnData;
}

- (void) setData:(CHEATS_LIST *)cheatData
{
	if (cheatData == NULL)
	{
		return;
	}
	
	pthread_mutex_lock(&mutexData);
	
	data = cheatData;
	
	pthread_mutex_unlock(&mutexData);
	
	[self update];
	
	if (workingCopy != nil)
	{
		pthread_mutex_lock(&mutexData);
		
		CHEATS_LIST *thisData = data;
		CHEATS_LIST *workingData = workingCopy.data;
		*workingData = *thisData;
		
		pthread_mutex_unlock(&mutexData);
		
		[workingCopy update];
	}
}

- (BOOL) retainData
{
	BOOL result = YES;
	
	if (internalData == NULL)
	{
		internalData = (CHEATS_LIST *)malloc(sizeof(CHEATS_LIST));
		if (internalData == NULL)
		{
			result = NO;
			return result;
		}
	}
	
	pthread_mutex_lock(&mutexData);
	*internalData = *data;
	data = internalData;
	pthread_mutex_unlock(&mutexData);
	
	return result;
}

- (BOOL) enabled
{
	return data->enabled;
}

- (void) setEnabled:(BOOL)theState
{
	data->enabled = theState;
	
	if (workingCopy != nil)
	{
		workingCopy.enabled = theState;
	}
}

- (NSString *) description
{
	return [NSString stringWithCString:(const char *)&data->description[0] encoding:NSUTF8StringEncoding];
}

- (void) setDescription:(NSString *)desc
{
	if (desc == nil)
	{
		memset(&data->description[0], 0, sizeof(data->description));
	}
	else
	{
		[desc getCString:&data->description[0] maxLength:sizeof(data->description) encoding:NSUTF8StringEncoding];
	}
	
	if (workingCopy != nil)
	{
		workingCopy.description = desc;
	}
}

- (char *) descriptionCString
{
	return &data->description[0];
}

- (NSInteger) cheatType
{
	return (NSInteger)data->type;
}

- (void) setCheatType:(NSInteger)theType
{
	data->type = (u8)theType;
	
	switch (theType)
	{
		case CHEAT_TYPE_INTERNAL:
			self.cheatTypeIcon = iconInternalCheat;
			self.isSupportedCheatType = YES;
			break;
			
		case CHEAT_TYPE_ACTION_REPLAY:
			self.cheatTypeIcon = iconActionReplay;
			self.isSupportedCheatType = YES;
			break;
			
		case CHEAT_TYPE_CODE_BREAKER:
			self.cheatTypeIcon = iconCodeBreaker;
			self.isSupportedCheatType = NO;
			break;
			
		default:
			break;
	}
	
	if (workingCopy != nil)
	{
		workingCopy.cheatType = theType;
	}
}

- (void) setCheatTypeIcon:(NSImage *)theIcon
{
	// Do nothing. This method exists for KVO compliance only.
}

- (NSImage *) cheatTypeIcon
{
	NSImage *theIcon = nil;
	
	switch (self.cheatType)
	{
		case CHEAT_TYPE_INTERNAL:
			theIcon = iconInternalCheat;
			break;
			
		case CHEAT_TYPE_ACTION_REPLAY:
			theIcon = iconActionReplay;
			break;
			
		case CHEAT_TYPE_CODE_BREAKER:
			theIcon = iconCodeBreaker;
			break;
			
		default:
			break;
	}
	
	return theIcon;
}

- (BOOL) isSupportedCheatType
{
	BOOL isSupported = YES;
	
	switch (self.cheatType)
	{
		case CHEAT_TYPE_INTERNAL:
		case CHEAT_TYPE_ACTION_REPLAY:
			isSupported = YES;
			break;
			
		case CHEAT_TYPE_CODE_BREAKER:
			isSupported = NO;
			break;
			
		default:
			break;
	}
	
	return isSupported;
}

- (void) setIsSupportedCheatType:(BOOL)isSupported
{
	// Do nothing. This method exists for KVO compliance only.
}

- (NSInteger) freezeType
{
	return (NSInteger)data->freezeType;
}

- (void) setFreezeType:(NSInteger)theType
{
	data->freezeType = (u8)theType;
	
	if (workingCopy != nil)
	{
		workingCopy.freezeType = theType;
	}
}

- (UInt8) bytes
{
	return (UInt8)(data->size + 1);
}

- (void) setBytes:(UInt8)byteSize
{
	data->size = (u8)(byteSize - 1);
	
	if (workingCopy != nil)
	{
		workingCopy.bytes = byteSize;
	}
}

- (NSUInteger) codeCount
{
	return (NSUInteger)data->num;
}

- (void) setCodeCount:(NSUInteger)count
{
	// Do nothing. This method exists for KVO compliance only.
}

- (NSString *) code
{
	NSString *codeLine = @"";
	NSString *cheatCodes = @"";
	
	NSUInteger numberCodes = self.codeCount;
	if (numberCodes > MAX_XX_CODE)
	{
		return nil;
	}
	
	for (NSUInteger i = 0; i < numberCodes; i++)
	{
		codeLine = [NSString stringWithFormat:@"%08X %08X\n", data->code[i][0], data->code[i][1]];
		cheatCodes = [cheatCodes stringByAppendingString:codeLine];
	}
	
	return cheatCodes;
}

- (void) setCode:(NSString *)theCode
{
	if (theCode == nil)
	{
		return;
	}
	
	size_t codeCStringSize = MAX_XX_CODE * 10 * 2 * sizeof(char);
	char *codeCString = (char *)calloc(codeCStringSize, 1);
	[theCode getCString:codeCString maxLength:codeCStringSize encoding:NSUTF8StringEncoding];
	
	CHEATS::XXCodeFromString(data, codeCString);
	
	free(codeCString);
	codeCString = NULL;
	
	self.codeCount = self.codeCount;
	self.bytes = self.bytes;
	
	if (workingCopy != nil)
	{
		workingCopy.code = theCode;
	}
}

- (UInt32) memAddress
{
	if (self.cheatType != CHEAT_TYPE_INTERNAL) // Needs to be the Internal Cheat type
	{
		return 0;
	}
	
	return (data->code[0][0] | 0x02000000);
}

- (void) setMemAddress:(UInt32)theAddress
{
	if (self.cheatType != CHEAT_TYPE_INTERNAL) // Needs to be the Internal Cheat type
	{
		return;
	}
	
	theAddress &= 0x00FFFFFF;
	theAddress |= 0x02000000;
	data->code[0][0] = theAddress;
	
	if (workingCopy != nil)
	{
		workingCopy.memAddress = theAddress;
	}
}

- (NSString *) memAddressString
{
	return [NSString stringWithFormat:@"0x%08X", (unsigned int)self.memAddress];
}

- (void) setMemAddressString:(NSString *)addressString
{
	if (self.cheatType != CHEAT_TYPE_INTERNAL) // Needs to be the Internal Cheat type
	{
		return;
	}
	
	u32 address = 0x00000000;
	[[NSScanner scannerWithString:addressString] scanHexInt:&address];
	self.memAddress = address;
	
	if (workingCopy != nil)
	{
		workingCopy.memAddressString = addressString;
	}
}

- (NSString *) memAddressSixDigitString
{
	return [NSString stringWithFormat:@"%06X", (unsigned int)(self.memAddress & 0x00FFFFFF)];
}

- (void) setMemAddressSixDigitString:(NSString *)addressString
{
	self.memAddressString = addressString;
}

- (SInt64) value
{
	if (self.cheatType != CHEAT_TYPE_INTERNAL) // Needs to be the Internal Cheat type
	{
		return 0;
	}
	
	return (data->code[0][1]);
}

- (void) setValue:(SInt64)theValue
{
	if (self.cheatType != CHEAT_TYPE_INTERNAL) // Needs to be the Internal Cheat type
	{
		return;
	}
	
	data->code[0][1] = (u32)theValue;
	
	if (workingCopy != nil)
	{
		workingCopy.value = theValue;
	}
}

- (void) update
{
	self.enabled = self.enabled;
	self.description = self.description;
	self.cheatType = self.cheatType;
	self.freezeType = self.freezeType;
	
	if (self.cheatType == CHEAT_TYPE_INTERNAL)
	{
		self.bytes = self.bytes;
		self.memAddress = self.memAddress;
		self.value = self.value;
	}
	else
	{
		self.code = self.code;
	}
}

- (CocoaDSCheatItem *) createWorkingCopy
{
	CocoaDSCheatItem *newWorkingCopy = nil;
	
	if (workingCopy != nil)
	{
		[workingCopy release];
	}
	
	newWorkingCopy = [[CocoaDSCheatItem alloc] initWithCheatData:self.data];
	[newWorkingCopy retainData];
	newWorkingCopy.parent = self;
	workingCopy = newWorkingCopy;
	
	return newWorkingCopy;
}

- (void) destroyWorkingCopy
{
	[workingCopy release];
	workingCopy = nil;
}

- (void) mergeFromWorkingCopy
{
	if (workingCopy == nil)
	{
		return;
	}
	
	CHEATS_LIST *thisData = self.data;
	CHEATS_LIST *workingData = workingCopy.data;
	
	pthread_mutex_lock(&mutexData);
	*thisData = *workingData;
	pthread_mutex_unlock(&mutexData);
	
	[self update];
}

- (void) mergeToParent
{
	if (parent == nil)
	{
		return;
	}
	
	CHEATS_LIST *thisData = self.data;
	CHEATS_LIST *parentData = parent.data;
	
	pthread_mutex_lock(&mutexData);
	*parentData = *thisData;
	pthread_mutex_unlock(&mutexData);
	
	[parent update];
}

- (void) setDataWithDictionary:(NSDictionary *)dataDict
{
	if (dataDict == nil)
	{
		return;
	}
	
	NSNumber *enabledNumber = (NSNumber *)[dataDict valueForKey:@"enabled"];
	if (enabledNumber != nil)
	{
		self.enabled = [enabledNumber boolValue];
	}
	
	NSString *descriptionString = (NSString *)[dataDict valueForKey:@"description"];
	if (descriptionString != nil)
	{
		self.description = descriptionString;
	}
	
	NSNumber *freezeTypeNumber = (NSNumber *)[dataDict valueForKey:@"freezeType"];
	if (freezeTypeNumber != nil)
	{
		self.freezeType = [freezeTypeNumber integerValue];
	}
	
	NSNumber *cheatTypeNumber = (NSNumber *)[dataDict valueForKey:@"cheatType"];
	if (cheatTypeNumber != nil)
	{
		self.cheatType = [cheatTypeNumber integerValue];
	}
	
	if (self.cheatType == CHEAT_TYPE_INTERNAL)
	{
		NSNumber *bytesNumber = (NSNumber *)[dataDict valueForKey:@"bytes"];
		if (bytesNumber != nil)
		{
			self.bytes = [bytesNumber unsignedIntegerValue];
		}
		
		NSNumber *memAddressNumber = (NSNumber *)[dataDict valueForKey:@"memAddress"];
		if (memAddressNumber != nil)
		{
			self.memAddress = [memAddressNumber unsignedIntegerValue];
		}
		
		NSNumber *valueNumber = (NSNumber *)[dataDict valueForKey:@"value"];
		if (valueNumber != nil)
		{
			self.value = [valueNumber integerValue];
		}
	}
	else
	{
		NSString *codeString = (NSString *)[dataDict valueForKey:@"code"];
		if (codeString != nil)
		{
			self.code = codeString;
		}
	}
}

- (NSDictionary *) dataDictionary
{
	return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithBool:self.enabled], @"enabled",
			[NSNumber numberWithInteger:self.cheatType], @"cheatType",
			self.description, @"description",
			[NSNumber numberWithInteger:self.freezeType], @"freezeType",
			[NSNumber numberWithUnsignedInteger:self.codeCount], @"codeCount",
			[NSNumber numberWithUnsignedInteger:self.bytes], @"bytes",
			[NSNumber numberWithUnsignedInt:self.memAddress], @"memAddress",
			self.memAddressString, @"memAddressString",
			[NSNumber numberWithInteger:self.value], @"value",
			self.code, @"code",
			[NSNumber numberWithBool:self.isSupportedCheatType], @"isSupportedCheatType",
			nil];
}

@end


@implementation CocoaDSCheatManager

@synthesize listData;
@synthesize list;
@synthesize cdsCore;
@synthesize untitledCount;
@synthesize dbTitle;
@synthesize dbDate;

- (id)init
{
	return [self initWithCore:nil fileURL:nil listData:nil];
}

- (id) initWithCore:(CocoaDSCore *)core
{
	return [self initWithCore:core fileURL:nil listData:nil];
}

- (id) initWithCore:(CocoaDSCore *)core fileURL:(NSURL *)fileURL
{
	return [self initWithCore:core fileURL:fileURL listData:nil];
}

- (id) initWithCore:(CocoaDSCore *)core listData:(CHEATS *)cheatList
{
	return [self initWithCore:core fileURL:nil listData:cheatList];
}

- (id) initWithCore:(CocoaDSCore *)core fileURL:(NSURL *)fileURL listData:(CHEATS *)cheatList
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	if (cheatList == nil)
	{
		CHEATS *newListData = new CHEATS();
		if (newListData == nil)
		{
			[self release];
			return nil;
		}
		
		listData = newListData;
	}
	else
	{
		listData = cheatList;
	}
	
	if (fileURL != nil)
	{
		listData->init((char *)[[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding]);
		list = [[CocoaDSCheatManager cheatListWithListObject:listData] retain];
	}
	else
	{
		list = [[NSMutableArray alloc] initWithCapacity:100];
		if (list == nil)
		{
			delete listData;
			[self release];
			return nil;
		}
	}
	
	cdsCore = [core retain];
	untitledCount = 0;
	dbTitle = nil;
	dbDate = nil;
	
	return self;
}

- (void)dealloc
{
	self.dbTitle = nil;
	self.dbDate = nil;
	self.cdsCore = nil;
	[list release];
	delete (CHEATS *)self.listData;
	
	[super dealloc];
}

- (BOOL) add:(CocoaDSCheatItem *)cheatItem
{
	BOOL result = NO;
	
	if (cheatItem == nil)
	{
		return result;
	}
	
	// Get the current pointer to the raw cheat list data. We will need it later
	// to check if the list got reallocated.
	CHEATS_LIST *cheatListData = self.listData->getListPtr();
	
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	
	switch (cheatItem.cheatType)
	{
		case CHEAT_TYPE_INTERNAL:
			result = self.listData->add(cheatItem.bytes - 1, cheatItem.memAddress, cheatItem.value, [cheatItem descriptionCString], cheatItem.enabled);
			break;
			
		case CHEAT_TYPE_ACTION_REPLAY:
		{
			char *cheatCodes = (char *)[cheatItem.code cStringUsingEncoding:NSUTF8StringEncoding];
			if (cheatCodes != nil)
			{
				result = self.listData->add_AR(cheatCodes, [cheatItem descriptionCString], cheatItem.enabled);
			}
			break;
		}
			
		case CHEAT_TYPE_CODE_BREAKER:
		{
			char *cheatCodes = (char *)[cheatItem.code cStringUsingEncoding:NSUTF8StringEncoding];
			if (cheatCodes != nil)
			{
				result = self.listData->add_CB(cheatCodes, [cheatItem descriptionCString], cheatItem.enabled);
			}
			break;
		}
			
		default:
			break;
	}
	
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	if (![self.list containsObject:cheatItem])
	{
		[self.list addObject:cheatItem];
	}
	
	// Adding a new item may cause the raw list data to get reallocated, which
	// will break the data pointers. So check for reallocation, and if it occurs,
	// reset the data pointers for each item.
	if (cheatListData != self.listData->getListPtr())
	{
		NSUInteger listCount = self.listData->getSize();
		for (NSUInteger i = 0; i < listCount; i++)
		{
			CocoaDSCheatItem *itemInList = (CocoaDSCheatItem *)[self.list objectAtIndex:i];
			itemInList.data = self.listData->getItemByIndex(i);
		}
	}
	else
	{
		cheatItem.data = self.listData->getItemByIndex(self.listData->getSize() - 1);
	}
	
	return result;
}

- (void) remove:(CocoaDSCheatItem *)cheatItem
{
	if (cheatItem == nil)
	{
		return;
	}
	
	NSUInteger selectionIndex = [self.list indexOfObject:cheatItem];
	if (selectionIndex == NSNotFound)
	{
		return;
	}
	
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	self.listData->remove(selectionIndex);
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	// Removing an item from the raw cheat list data shifts all higher elements
	// by one, so we need to do the same.
	NSUInteger listCount = self.listData->getSize();
	for (NSUInteger i = selectionIndex; i < listCount; i++)
	{
		[(CocoaDSCheatItem *)[self.list objectAtIndex:(i + 1)] setData:self.listData->getItemByIndex(i)];
	}
		
	cheatItem.data = nil;
	[self.list removeObject:cheatItem];
}

- (BOOL) update:(CocoaDSCheatItem *)cheatItem
{
	BOOL result = NO;
	
	if (cheatItem == nil)
	{
		return result;
	}
	
	NSUInteger selectionIndex = [self.list indexOfObject:cheatItem];
	if (selectionIndex == NSNotFound)
	{
		return result;
	}
	
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	
	switch (cheatItem.cheatType)
	{
		case CHEAT_TYPE_INTERNAL:
			result = self.listData->update(cheatItem.bytes - 1, cheatItem.memAddress, cheatItem.value, [cheatItem descriptionCString], cheatItem.enabled, selectionIndex);
			break;
			
		case CHEAT_TYPE_ACTION_REPLAY:
		{
			char *cheatCodes = (char *)[cheatItem.code cStringUsingEncoding:NSUTF8StringEncoding];
			if (cheatCodes != nil)
			{
				result = self.listData->update_AR(cheatCodes, [cheatItem descriptionCString], cheatItem.enabled, selectionIndex);
			}
			break;
		}
			
		case CHEAT_TYPE_CODE_BREAKER:
		{
			char *cheatCodes = (char *)[cheatItem.code cStringUsingEncoding:NSUTF8StringEncoding];
			if (cheatCodes != nil)
			{
				result = self.listData->update_CB(cheatCodes, [cheatItem descriptionCString], cheatItem.enabled, selectionIndex);
			}
			break;
		}
			
		default:
			break;
	}
		
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	[cheatItem update];
		
	return result;
}

- (BOOL) save
{
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	BOOL result = self.listData->save();
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	return result;
}

- (NSUInteger) activeCount
{
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	NSUInteger activeCheatsCount = self.listData->getActiveCount();
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	return activeCheatsCount;
}

- (NSMutableArray *) cheatListFromDatabase:(NSURL *)fileURL errorCode:(NSInteger *)error
{
	NSMutableArray *newDBList = nil;
	
	if (fileURL == nil)
	{
		return newDBList;
	}
	
	CHEATSEXPORT *exporter = new CHEATSEXPORT();
	
	BOOL result = exporter->load((char *)[[fileURL path] cStringUsingEncoding:NSUTF8StringEncoding]);
	if (!result)
	{
		if (error != nil)
		{
			*error = exporter->getErrorCode();
		}
	}
	else
	{
		self.dbTitle = [NSString stringWithCString:(const char *)exporter->gametitle encoding:NSUTF8StringEncoding];
		self.dbDate = [NSString stringWithCString:(const char *)exporter->date encoding:NSUTF8StringEncoding];
		newDBList = [CocoaDSCheatManager cheatListWithItemStructArray:exporter->getCheats() count:exporter->getCheatsNum()];
		if (newDBList != nil)
		{
			for (CocoaDSCheatItem *cheatItem in newDBList)
			{
				cheatItem.willAdd = NO;
				[cheatItem retainData];
			}
		}
	}

	delete exporter;
	exporter = nil;
	
	return newDBList;
}

- (void) applyInternalCheat:(CocoaDSCheatItem *)cheatItem
{
	if (cheatItem == nil)
	{
		return;
	}
	
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	[CocoaDSCheatManager applyInternalCheatWithItem:cheatItem];
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
}

+ (void) setMasterCheatList:(CocoaDSCheatManager *)cheatListManager
{
	cheats = cheatListManager.listData;
}

+ (void) applyInternalCheatWithItem:(CocoaDSCheatItem *)cheatItem
{
	[CocoaDSCheatManager applyInternalCheatWithAddress:cheatItem.memAddress value:cheatItem.value bytes:cheatItem.bytes];
}

+ (void) applyInternalCheatWithAddress:(UInt32)address value:(UInt32)value bytes:(NSUInteger)bytes
{
	address &= 0x00FFFFFF;
	address |= 0x02000000;
	
	switch (bytes)
	{
		case 1:
		{
			u8 oneByteValue = (u8)(value & 0x000000FF);
			_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(address, oneByteValue);
			break;
		}
			
		case 2:
		{
			u16 twoByteValue = (u16)(value & 0x0000FFFF);
			_MMU_write16<ARMCPU_ARM9,MMU_AT_DEBUG>(address, twoByteValue);
			break;
		}
			
		case 3:
		{
			u32 threeByteWithExtraValue = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(address);
			threeByteWithExtraValue &= 0xFF000000;
			threeByteWithExtraValue |= (value & 0x00FFFFFF);
			_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(address, threeByteWithExtraValue);
			break;
		}
			
		case 4:
			_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(address, value);
			break;
			
		default:
			break;
	}
}

+ (NSMutableArray *) cheatListWithListObject:(CHEATS *)cheatList
{
	if (cheatList == nil)
	{
		return nil;
	}
	
	NSMutableArray *newList = [NSMutableArray arrayWithCapacity:100];
	if (newList == nil)
	{
		return newList;
	}
	
	u32 itemCount = cheatList->getSize();
	for (u32 i = 0; i < itemCount; i++)
	{
		[newList addObject:[[[CocoaDSCheatItem alloc] initWithCheatData:cheatList->getItemByIndex(i)] autorelease]];
	}
	
	return newList;
}

+ (NSMutableArray *) cheatListWithItemStructArray:(CHEATS_LIST *)cheatItemArray count:(NSUInteger)itemCount
{
	if (cheatItemArray == nil)
	{
		return nil;
	}
	
	NSMutableArray *newList = [NSMutableArray arrayWithCapacity:100];
	if (newList == nil)
	{
		return newList;
	}
	
	for (NSUInteger i = 0; i < itemCount; i++)
	{
		[newList addObject:[[[CocoaDSCheatItem alloc] initWithCheatData:cheatItemArray + i] autorelease]];
	}
	
	return newList;
}

+ (NSMutableDictionary *) cheatItemWithType:(NSInteger)cheatTypeID description:(NSString *)description
{
	BOOL isSupported = YES;
	
	if (cheatTypeID == CHEAT_TYPE_CODE_BREAKER)
	{
		isSupported = NO;
	}
	
	return [NSMutableDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithBool:NO], @"enabled",
			[NSNumber numberWithInteger:cheatTypeID], @"cheatType",
			description, @"description",
			[NSNumber numberWithInteger:0], @"freezeType",
			[NSNumber numberWithUnsignedInteger:0], @"codeCount",
			[NSNumber numberWithUnsignedInteger:4], @"bytes",
			[NSNumber numberWithInteger:0], @"memAddress",
			@"0x00000000", @"memAddressString",
			[NSNumber numberWithInteger:0], @"value",
			@"", @"code",
			[NSNumber numberWithBool:isSupported], @"isSupportedCheatType",
			nil];
}

@end

@implementation CocoaDSCheatSearch

@synthesize listData;
@synthesize addressList;
@synthesize cdsCore;
@synthesize searchCount;

- (id)init
{
	return [self initWithCore:nil];
}

- (id) initWithCore:(CocoaDSCore *)core
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	CHEATSEARCH *newListData = new CHEATSEARCH();
	if (newListData == nil)
	{
		[self release];
		return nil;
	}
	
	listData = newListData;
	addressList = nil;
	cdsCore = [core retain];
	searchCount = 0;
	
	return self;
}

- (void)dealloc
{
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	self.listData->close();
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	[addressList release];
	self.cdsCore = nil;
	delete (CHEATSEARCH *)self.listData;
	
	[super dealloc];
}

- (NSUInteger) runExactValueSearch:(NSInteger)value byteSize:(UInt8)byteSize signType:(NSInteger)signType
{
	NSUInteger itemCount = 0;
	BOOL listExists = YES;
	
	if (searchCount == 0)
	{
		byteSize--;
		
		pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
		listExists = (NSUInteger)self.listData->start((u8)CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE, (u8)byteSize, (u8)signType);
		pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	}
	
	if (listExists)
	{
		pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
		itemCount = (NSUInteger)self.listData->search((u32)value);
		NSMutableArray *newAddressList = [[CocoaDSCheatSearch addressListWithListObject:self.listData maxItems:100] retain];
		pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
		
		[addressList release];
		addressList = newAddressList;
		searchCount++;
	}
	
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.searchDidFinish" object:self userInfo:nil];
	
	return itemCount;
}

- (void) runExactValueSearchOnThread:(id)object
{
	CocoaDSCheatSearchParams *searchParams = (CocoaDSCheatSearchParams *)object;
	[self runExactValueSearch:searchParams.value byteSize:searchParams.byteSize signType:searchParams.signType];
}

- (NSUInteger) runComparativeSearch:(NSInteger)typeID byteSize:(UInt8)byteSize signType:(NSInteger)signType
{
	NSUInteger itemCount = 0;
	BOOL listExists = YES;
	
	if (searchCount == 0)
	{
		byteSize--;
		
		pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
		listExists = (NSUInteger)self.listData->start((u8)CHEATSEARCH_SEARCHSTYLE_COMPARATIVE, (u8)byteSize, (u8)signType);
		pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
		
		addressList = nil;
	}
	else
	{
		pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
		itemCount = (NSUInteger)self.listData->search((u8)typeID);
		NSMutableArray *newAddressList = [[CocoaDSCheatSearch addressListWithListObject:self.listData maxItems:100] retain];
		pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
		
		[addressList release];
		addressList = newAddressList;
	}

	if (listExists)
	{
		searchCount++;
	}
	
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.searchDidFinish" object:self userInfo:nil];
	
	return itemCount;
}

- (void) runComparativeSearchOnThread:(id)object
{
	CocoaDSCheatSearchParams *searchParams = (CocoaDSCheatSearchParams *)object;
	[self runComparativeSearch:searchParams.comparativeSearchType byteSize:searchParams.byteSize signType:searchParams.signType];
}

- (void) reset
{
	pthread_mutex_lock(self.cdsCore.mutexCoreExecute);
	self.listData->close();
	pthread_mutex_unlock(self.cdsCore.mutexCoreExecute);
	
	searchCount = 0;
	[addressList release];
	addressList = nil;
}

+ (NSMutableArray *) addressListWithListObject:(CHEATSEARCH *)addressList maxItems:(NSUInteger)maxItemCount
{
	if (addressList == nil)
	{
		return nil;
	}
	
	if (maxItemCount == 0)
	{
		maxItemCount = 1024 * 1024 * 4;
	}
	
	NSMutableArray *newList = [NSMutableArray arrayWithCapacity:maxItemCount];
	if (newList == nil)
	{
		return newList;
	}
	
	NSMutableDictionary *newItem = nil;
	NSUInteger listCount = 0;
	u32 address;
	u32 value;
	
	addressList->getListReset();
	while (addressList->getList(&address, &value) && listCount < maxItemCount)
	{
		newItem = [NSMutableDictionary dictionaryWithObjectsAndKeys:
				   [NSString stringWithFormat:@"0x02%06X", address], @"addressString",
				   [NSNumber numberWithUnsignedInteger:value], @"value",
				   nil];
		
		[newList addObject:newItem];
		listCount++;
	}
	
	return newList;
}

@end

@implementation CocoaDSCheatSearchParams

@synthesize comparativeSearchType;
@synthesize value;
@synthesize byteSize;
@synthesize signType;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	comparativeSearchType = CHEATSEARCH_COMPARETYPE_EQUALS_TO;
	value = 1;
	byteSize = 4;
	signType = CHEATSEARCH_UNSIGNED;
	
	return self;
}

@end
