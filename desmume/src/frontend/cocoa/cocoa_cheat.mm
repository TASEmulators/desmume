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

#import "cocoa_cheat.h"
#import "cocoa_globals.h"
#import "cocoa_util.h"

#include "ClientCheatManager.h"
#include "../../NDSSystem.h"
#undef BOOL


@implementation CocoaDSCheatItem

static NSImage *iconDirectory = nil;
static NSImage *iconInternalCheat = nil;
static NSImage *iconActionReplay = nil;
static NSImage *iconCodeBreaker = nil;

@synthesize _internalData;
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

- (id)init
{
	return [self initWithCheatItem:NULL];
}

- (id) initWithCocoaCheatItem:(CocoaDSCheatItem *)cdsCheatItem
{
	return [self initWithCheatItem:[cdsCheatItem clientData]];
}

- (id) initWithCheatItem:(ClientCheatItem *)cheatItem
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	if (cheatItem == NULL)
	{
		_internalData = new ClientCheatItem;
		_didAllocateInternalData = YES;
	}
	else
	{
		_internalData = cheatItem;
		_didAllocateInternalData = NO;
	}
	
	_disableWorkingCopyUpdate = NO;
	willAdd = NO;
	workingCopy = nil;
	parent = nil;
	_isMemAddressAlreadyUpdating = NO;
	
	return self;
}

- (id) initWithCheatData:(const CHEATS_LIST *)cheatData
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	_internalData = new ClientCheatItem;
	_didAllocateInternalData = YES;
	
	if (cheatData != NULL)
	{
		_internalData->Init(*cheatData);
	}
	
	willAdd = NO;
	workingCopy = nil;
	parent = nil;
	_isMemAddressAlreadyUpdating = NO;
	
	return self;
}

- (void) dealloc
{
	[self destroyWorkingCopy];
	
	if (_didAllocateInternalData)
	{
		delete _internalData;
		_internalData = NULL;
	}
	
	[super dealloc];
}

- (BOOL) enabled
{
	return _internalData->IsEnabled() ? YES : NO;
}

- (void) setEnabled:(BOOL)theState
{
	_internalData->SetEnabled((theState) ? true : false);
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setEnabled:theState];
	}
}

- (NSString *) description
{
	return [NSString stringWithCString:_internalData->GetName() encoding:NSUTF8StringEncoding];
}

- (void) setDescription:(NSString *)desc
{
	if (desc == nil)
	{
		_internalData->SetName(NULL);
	}
	else
	{
		_internalData->SetName([desc cStringUsingEncoding:NSUTF8StringEncoding]);
	}
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setDescription:desc];
	}
}

- (char *) descriptionCString
{
	return (char *)_internalData->GetName();
}

- (NSInteger) cheatType
{
	return (NSInteger)_internalData->GetType();
}

- (void) setCheatType:(NSInteger)theType
{
	_internalData->SetType((CheatType)theType);
	
	switch (theType)
	{
		case CheatType_Internal:
			[self setCheatTypeIcon:iconInternalCheat];
			[self setIsSupportedCheatType:YES];
			[self setMemAddress:[self memAddress]];
			[self setValue:[self value]];
			[self setBytes:[self bytes]];
			break;
			
		case CheatType_ActionReplay:
			[self setCheatTypeIcon:iconActionReplay];
			[self setIsSupportedCheatType:YES];
			[self setCode:[self code]];
			break;
			
		case CheatType_CodeBreaker:
			[self setCheatTypeIcon:iconCodeBreaker];
			[self setIsSupportedCheatType:NO];
			[self setCode:[self code]];
			break;
			
		default:
			break;
	}
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setCheatType:theType];
	}
}

- (void) setCheatTypeIcon:(NSImage *)theIcon
{
	// Do nothing. This method exists for KVO compliance only.
}

- (NSImage *) cheatTypeIcon
{
	NSImage *theIcon = nil;
	
	switch ([self cheatType])
	{
		case CheatType_Internal:
			theIcon = iconInternalCheat;
			break;
			
		case CheatType_ActionReplay:
			theIcon = iconActionReplay;
			break;
			
		case CheatType_CodeBreaker:
			theIcon = iconCodeBreaker;
			break;
			
		default:
			break;
	}
	
	return theIcon;
}

- (BOOL) isSupportedCheatType
{
	return (_internalData->IsSupportedType()) ? YES : NO;
}

- (void) setIsSupportedCheatType:(BOOL)isSupported
{
	// Do nothing. This method exists for KVO compliance only.
}

- (NSInteger) freezeType
{
	return (NSInteger)_internalData->GetFreezeType();
}

- (void) setFreezeType:(NSInteger)theType
{
	_internalData->SetFreezeType((CheatFreezeType)theType);
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setFreezeType:theType];
	}
}

- (UInt8) bytes
{
	return _internalData->GetValueLength();
}

- (void) setBytes:(UInt8)byteSize
{
	_internalData->SetValueLength(byteSize);
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setBytes:byteSize];
	}
}

- (NSUInteger) codeCount
{
	return (NSUInteger)_internalData->GetCodeCount();
}

- (void) setCodeCount:(NSUInteger)count
{
	// Do nothing. This method exists for KVO compliance only.
}

- (NSString *) code
{
	return [NSString stringWithCString:_internalData->GetRawCodeString() encoding:NSUTF8StringEncoding];
}

- (void) setCode:(NSString *)theCode
{
	if (theCode == nil)
	{
		return;
	}
	
	_internalData->SetRawCodeString([theCode cStringUsingEncoding:NSUTF8StringEncoding], true);
	
	[self setCodeCount:[self codeCount]];
	[self setBytes:[self bytes]];
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setCode:theCode];
	}
}

- (UInt32) memAddress
{
	return _internalData->GetAddress();
}

- (void) setMemAddress:(UInt32)theAddress
{
	theAddress &= 0x00FFFFFF;
	theAddress |= 0x02000000;
	
	_internalData->SetAddress(theAddress);
	
	if (!_isMemAddressAlreadyUpdating)
	{
		_isMemAddressAlreadyUpdating = YES;
		NSString *addrString = [NSString stringWithCString:_internalData->GetAddressSixDigitString() encoding:NSUTF8StringEncoding];
		[self setMemAddressSixDigitString:addrString];
		_isMemAddressAlreadyUpdating = NO;
	}
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setMemAddress:theAddress];
	}
}

- (NSString *) memAddressString
{
	return [NSString stringWithCString:_internalData->GetAddressString() encoding:NSUTF8StringEncoding];
}

- (void) setMemAddressString:(NSString *)addressString
{
	if (!_isMemAddressAlreadyUpdating)
	{
		_isMemAddressAlreadyUpdating = YES;
		u32 address = 0x00000000;
		[[NSScanner scannerWithString:addressString] scanHexInt:&address];
		[self setMemAddress:address];
		_isMemAddressAlreadyUpdating = NO;
	}
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setMemAddressString:addressString];
	}
}

- (NSString *) memAddressSixDigitString
{
	return [NSString stringWithCString:_internalData->GetAddressSixDigitString() encoding:NSUTF8StringEncoding];
}

- (void) setMemAddressSixDigitString:(NSString *)addressString
{
	[self setMemAddressString:addressString];
}

- (SInt64) value
{
	return _internalData->GetValue();
}

- (void) setValue:(SInt64)theValue
{
	_internalData->SetValue((u32)theValue);
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setValue:theValue];
	}
}

- (void) update
{
	[self setEnabled:[self enabled]];
	[self setDescription:[self description]];
	[self setCheatType:[self cheatType]];
	[self setFreezeType:[self freezeType]];
	
	if ([self cheatType] == CheatType_Internal)
	{
		[self setMemAddressSixDigitString:[self memAddressSixDigitString]];
		[self setValue:[self value]];
		[self setBytes:[self bytes]];
	}
	else
	{
		[self setCode:[self code]];
	}
}

- (void) copyFrom:(CocoaDSCheatItem *)cdsCheatItem
{
	if (cdsCheatItem == nil)
	{
		return;
	}
	
	if (cdsCheatItem == workingCopy)
	{
		_disableWorkingCopyUpdate = YES;
	}
	
	[self setEnabled:[cdsCheatItem enabled]];
	[self setDescription:[cdsCheatItem description]];
	[self setCheatType:[cdsCheatItem cheatType]];
	[self setFreezeType:[cdsCheatItem freezeType]];
	
	if ([self cheatType] == CheatType_Internal)
	{
		[self setMemAddress:[cdsCheatItem memAddress]];
		[self setValue:[cdsCheatItem value]];
		[self setBytes:[cdsCheatItem bytes]];
	}
	else
	{
		[self setCode:[cdsCheatItem code]];
	}
	
	_disableWorkingCopyUpdate = NO;
}

- (CocoaDSCheatItem *) createWorkingCopy
{
	CocoaDSCheatItem *newWorkingCopy = nil;
	
	if (workingCopy != nil)
	{
		[workingCopy release];
	}
	
	newWorkingCopy = [[CocoaDSCheatItem alloc] init];
	ClientCheatItem *workingCheat = [newWorkingCopy clientData];
	workingCheat->Init(*[self clientData]);
	
	[newWorkingCopy setParent:self];
	workingCopy = newWorkingCopy;
	
	return newWorkingCopy;
}

- (void) destroyWorkingCopy
{
	[workingCopy release];
	workingCopy = nil;
}

- (void) mergeToParent
{
	if (parent == nil)
	{
		return;
	}
	
	[parent copyFrom:self];
}

+ (void) setIconDirectory:(NSImage *)iconImage
{
	iconDirectory = iconImage;
}

+ (NSImage *) iconDirectory
{
	return iconDirectory;
}

+ (void) setIconInternalCheat:(NSImage *)iconImage
{
	iconInternalCheat = iconImage;
}

+ (NSImage *) iconInternalCheat
{
	return iconInternalCheat;
}

+ (void) setIconActionReplay:(NSImage *)iconImage
{
	iconActionReplay = iconImage;
}

+ (NSImage *) iconActionReplay
{
	return iconActionReplay;
}

+ (void) setIconCodeBreaker:(NSImage *)iconImage
{
	iconCodeBreaker = iconImage;
}

+ (NSImage *) iconCodeBreaker
{
	return iconCodeBreaker;
}

@end

@implementation CocoaDSCheatDBEntry

@dynamic name;
@dynamic comment;
@dynamic icon;
@dynamic entryCount;
@dynamic codeString;
@dynamic isDirectory;
@dynamic isCheatItem;
@dynamic willAdd;
@synthesize needSetMixedState;
@synthesize parent;
@synthesize child;

- (id)init
{
	return [self initWithDBEntry:NULL];
}

- (id) initWithDBEntry:(const CheatDBEntry *)dbEntry
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	if (dbEntry == NULL)
	{
		[self release];
		self = nil;
		return self;
	}
	
	if (dbEntry->codeLength == NULL)
	{
		const NSUInteger entryCount = dbEntry->child.size();
		child = [[NSMutableArray alloc] initWithCapacity:entryCount];
		if (child == nil)
		{
			[self release];
			self = nil;
			return self;
		}
		
		for (NSUInteger i = 0; i < entryCount; i++)
		{
			const CheatDBEntry &childEntry = dbEntry->child[i];
			
			CocoaDSCheatDBEntry *newCocoaEntry = [[CocoaDSCheatDBEntry alloc] initWithDBEntry:&childEntry];
			[newCocoaEntry setParent:self];
			[child addObject:newCocoaEntry];
		}
	}
	else
	{
		child = nil;
	}
	
	_dbEntry = (CheatDBEntry *)dbEntry;
	codeString = nil;
	willAdd = NO;
	needSetMixedState = NO;
	parent = nil;
	
	return self;
}

- (void)dealloc
{
	[child release];
	child = nil;
	
	[codeString release];
	codeString = nil;
	
	[super dealloc];
}

- (NSString *) name
{
	if (_dbEntry->name != NULL)
	{
		return [NSString stringWithCString:_dbEntry->name encoding:NSUTF8StringEncoding];
	}
	
	return @"";
}

- (NSString *) comment
{
	if (_dbEntry->note != NULL)
	{
		return [NSString stringWithCString:_dbEntry->note encoding:NSUTF8StringEncoding];
	}
	
	return @"";
}

- (NSImage *) icon
{
	if (_dbEntry->codeLength == NULL)
	{
		return [CocoaDSCheatItem iconDirectory];
	}
	
	return [CocoaDSCheatItem iconActionReplay];
}

- (NSInteger) entryCount
{
	return (NSInteger)_dbEntry->child.size();
}

- (NSString *) codeString
{
	if ( (codeString == nil) && [self isCheatItem] )
	{
		char codeStr[8+8+1+1+1] = {0};
		const size_t codeCount = *_dbEntry->codeLength / 2;
		
		u32 code0 = _dbEntry->codeData[0];
		u32 code1 = _dbEntry->codeData[1];
		snprintf(codeStr, sizeof(codeStr), "%08X %08X", code0, code1);
		std::string code = codeStr;
		
		for (size_t i = 1; i < codeCount; i++)
		{
			code0 = _dbEntry->codeData[(i * 2) + 0];
			code1 = _dbEntry->codeData[(i * 2) + 1];
			snprintf(codeStr, sizeof(codeStr), "\n%08X %08X", code0, code1);
			code += codeStr;
		}
		
		codeString = [[NSString alloc] initWithCString:code.c_str() encoding:NSUTF8StringEncoding];
	}
	
	return codeString;
}

- (BOOL) isDirectory
{
	return (_dbEntry->codeLength == NULL) ? YES : NO;
}

- (BOOL) isCheatItem
{
	return (_dbEntry->codeLength != NULL) ? YES : NO;
}

- (void) setWillAdd:(NSInteger)theState
{
	if ((theState == GUI_STATE_MIXED) && ![self needSetMixedState])
	{
		theState = GUI_STATE_ON;
	}
	
	if (willAdd == theState)
	{
		return;
	}
	else
	{
		willAdd = theState;
	}
	
	if (theState == GUI_STATE_MIXED)
	{
		if (parent != nil)
		{
			[parent willChangeValueForKey:@"willAdd"];
			[parent setNeedSetMixedState:YES];
			[parent setWillAdd:GUI_STATE_MIXED];
			[parent setNeedSetMixedState:NO];
			[parent didChangeValueForKey:@"willAdd"];
		}
		
		return;
	}
	
	if (_dbEntry->codeLength == NULL)
	{
		for (CocoaDSCheatDBEntry *childEntry in child)
		{
			[childEntry willChangeValueForKey:@"willAdd"];
			[childEntry setWillAdd:theState];
			[childEntry didChangeValueForKey:@"willAdd"];
		}
	}
	
	if (parent != nil)
	{
		NSInteger firstEntryState = [(CocoaDSCheatDBEntry *)[[parent child] objectAtIndex:0] willAdd];
		BOOL isMixedStateFound = (firstEntryState == GUI_STATE_MIXED);
		
		if (!isMixedStateFound)
		{
			for (CocoaDSCheatDBEntry *childEntry in [parent child])
			{
				const NSInteger childEntryState = [childEntry willAdd];
				
				isMixedStateFound = (firstEntryState != childEntryState) || (childEntryState == GUI_STATE_MIXED);
				if (isMixedStateFound)
				{
					[parent willChangeValueForKey:@"willAdd"];
					[parent setNeedSetMixedState:YES];
					[parent setWillAdd:GUI_STATE_MIXED];
					[parent setNeedSetMixedState:NO];
					[parent didChangeValueForKey:@"willAdd"];
					break;
				}
			}
		}
		
		if (!isMixedStateFound)
		{
			[parent willChangeValueForKey:@"willAdd"];
			[parent setWillAdd:firstEntryState];
			[parent didChangeValueForKey:@"willAdd"];
		}
	}
}

- (NSInteger) willAdd
{
	return willAdd;
}

- (ClientCheatItem *) newClientItem
{
	ClientCheatItem *newItem = NULL;
	if (![self isCheatItem])
	{
		return newItem;
	}
	
	newItem = new ClientCheatItem;
	newItem->SetType(CheatType_ActionReplay); // Default to Action Replay for now
	newItem->SetFreezeType(CheatFreezeType_Normal);
	newItem->SetEnabled(false);
	newItem->SetComments(_dbEntry->note);
	
	CheatDBEntry *entryParent = _dbEntry->parent;
	
	// TODO: Replace this flattening out of names/comments with separated names/comments and tags.
	std::string descString = "";
	std::string itemString = "";
	
	if ( ((_dbEntry->name != NULL) && (*_dbEntry->name != '\0')) ||
	     ((_dbEntry->note != NULL) && (*_dbEntry->note != '\0')) )
	{
		if ( (_dbEntry->name != NULL) && (*_dbEntry->name != '\0') )
		{
			itemString += _dbEntry->name;
		}
		
		if ( (_dbEntry->note != NULL) && (*_dbEntry->note != '\0') )
		{
			if ( (_dbEntry->name != NULL) && (*_dbEntry->name != '\0') )
			{
				itemString += " | ";
			}
			
			itemString += _dbEntry->note;
		}
	}
	else
	{
		itemString = "No description.";
	}
	
	// If this entry is root or child of root, then don't add the directory
	// name or comments to the new cheat item's description.
	if ( (entryParent == NULL) || (entryParent->parent == NULL) )
	{
		descString = itemString;
	}
	else
	{
		if ( (entryParent->name != NULL) && (*entryParent->name != '\0') )
		{
			descString += entryParent->name;
		}
		
		if ( (entryParent->note != NULL) && (*entryParent->note != '\0') )
		{
			if ( (entryParent->name != NULL) && (*entryParent->name != '\0') )
			{
				descString += " ";
			}
			
			descString += "[";
			descString += entryParent->note;
			descString += "]";
		}
		
		descString += ": ";
		descString += itemString;
	}
	
	newItem->SetName(descString.c_str());
	newItem->SetRawCodeString([[self codeString] cStringUsingEncoding:NSUTF8StringEncoding], true);
	
	return newItem;
}

@end

@implementation CocoaDSCheatDBGame

@synthesize index;
@dynamic title;
@dynamic serial;
@dynamic crc;
@dynamic crcString;
@dynamic dataSize;
@dynamic isDataLoaded;
@dynamic cheatItemCount;
@synthesize entryRoot;

- (id)init
{
	return [self initWithGameEntry:NULL];
}

- (id) initWithGameEntry:(const CheatDBGame *)gameEntry
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	_dbGame = (CheatDBGame *)gameEntry;
	entryRoot = nil;
	index = 0;
	
	return self;
}

- (void)dealloc
{
	[entryRoot release];
	entryRoot = nil;
	
	[super dealloc];
}

- (NSString *) title
{
	return [NSString stringWithCString:_dbGame->GetTitle() encoding:NSUTF8StringEncoding];
}

- (NSString *) serial
{
	return [NSString stringWithCString:_dbGame->GetSerial() encoding:NSUTF8StringEncoding];
}

- (NSUInteger) crc
{
	return (NSUInteger)_dbGame->GetCRC();
}

- (NSString *) crcString
{
	const u32 crc = _dbGame->GetCRC();
	return [NSString stringWithFormat:@"%08lX", (unsigned long)crc];
}

- (NSInteger) dataSize
{
	return (NSInteger)_dbGame->GetRawDataSize();
}

- (NSString *) dataSizeString
{
	const u32 dataSize = _dbGame->GetRawDataSize();
	const float dataSizeKB = (float)dataSize / 1024.0f;
	
	if (dataSize > ((1024 * 100) - 1))
	{
		return [NSString stringWithFormat:@"%1.1f KB", dataSizeKB];
	}
	else if (dataSize > ((1024 * 10) - 1))
	{
		return [NSString stringWithFormat:@"%1.2f KB", dataSizeKB];
	}
	
	return [NSString stringWithFormat:@"%lu bytes", (unsigned long)dataSize];
}

- (BOOL) isDataLoaded
{
	return (_dbGame->IsEntryDataLoaded()) ? YES : NO;
}

- (NSInteger) cheatItemCount
{
	return (NSInteger)_dbGame->GetCheatItemCount();
}

- (CocoaDSCheatDBEntry *) loadEntryDataFromFilePtr:(FILE *)fp isEncrypted:(BOOL)isEncrypted
{
	[entryRoot release];
	entryRoot = nil;
	
	u8 *entryData = _dbGame->LoadEntryData(fp, (isEncrypted) ? true : false);
	if (entryData == NULL)
	{
		return entryRoot;
	}
	
	entryRoot = [[CocoaDSCheatDBEntry alloc] initWithDBEntry:&_dbGame->GetEntryRoot()];
	return entryRoot;
}

@end

@implementation CocoaDSCheatDatabase

@synthesize lastFileURL;
@dynamic description;
@dynamic formatString;
@dynamic isEncrypted;
@synthesize gameList;

- (id)init
{
	return [self initWithFileURL:nil error:NULL];
}

- (id) initWithFileURL:(NSURL *)fileURL error:(CheatSystemError *)errorCode
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	if (fileURL == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	lastFileURL = [fileURL copy];
	
	CheatDBFile *newDBFile = new CheatDBFile();
	if (newDBFile == NULL)
	{
		[lastFileURL release];
		[self release];
		self = nil;
		return self;
	}
	
	CheatSystemError error = newDBFile->OpenFile([CocoaDSUtil cPathFromFileURL:fileURL]);
	if (error != CheatSystemError_NoError)
	{
		delete newDBFile;
		
		if (errorCode != nil)
		{
			*errorCode = error;
		}
		
		[lastFileURL release];
		[self release];
		self = nil;
		return self;
	}
	
	_dbGameList = new CheatDBGameList;
	if (_dbGameList == NULL)
	{
		delete newDBFile;
		
		if (errorCode == nil)
		{
			*errorCode = error;
		}
		
		[lastFileURL release];
		[self release];
		self = nil;
		return self;
	}
	
	newDBFile->LoadGameList(NULL, 0, *_dbGameList);
	const size_t gameCount = _dbGameList->size();
	
	gameList = [[NSMutableArray alloc] initWithCapacity:gameCount];
	if (gameList == nil)
	{
		delete newDBFile;
		
		if (errorCode == nil)
		{
			*errorCode = error;
		}
		
		[lastFileURL release];
		[self release];
		self = nil;
		return self;
	}
	
	for (size_t i = 0; i < gameCount; i++)
	{
		const CheatDBGame &dbGame = (*_dbGameList)[i];
		CocoaDSCheatDBGame *newCocoaDBGame = [[[CocoaDSCheatDBGame alloc] initWithGameEntry:&dbGame] autorelease];
		
		[gameList addObject:newCocoaDBGame];
		[newCocoaDBGame setIndex:[gameList indexOfObject:newCocoaDBGame]];
	}
	
	_dbFile = newDBFile;
	
	return self;
}

- (void)dealloc
{
	[gameList release];
	[lastFileURL release];
	
	delete _dbGameList;
	_dbGameList = NULL;
	
	delete _dbFile;
	_dbFile = NULL;
	
	[super dealloc];
}

- (NSString *) description
{
	return [NSString stringWithCString:_dbFile->GetDescription() encoding:NSUTF8StringEncoding];
}

- (NSString *) formatString
{
	return [NSString stringWithCString:_dbFile->GetFormatString() encoding:NSUTF8StringEncoding];
}

- (BOOL) isEncrypted
{
	return (_dbFile->IsEncrypted()) ? YES : NO;
}

- (CocoaDSCheatDBGame *) getGameEntryUsingCode:(const char *)gameCode crc:(NSUInteger)crc
{
	if (gameCode == nil)
	{
		return nil;
	}
	
	const char gameCodeTerminated[5] = {
		gameCode[0],
		gameCode[1],
		gameCode[2],
		gameCode[3],
		'\0'
	};
	
	NSString *gameCodeString = [NSString stringWithCString:gameCodeTerminated encoding:NSUTF8StringEncoding];
	
	for (CocoaDSCheatDBGame *dbGame in gameList)
	{
		if ( ([dbGame crc] == crc) && [[dbGame serial] isEqualToString:gameCodeString] )
		{
			return dbGame;
		}
	}
	
	return nil;
}

- (CocoaDSCheatDBEntry *) loadGameEntry:(CocoaDSCheatDBGame *)dbGame
{
	CocoaDSCheatDBEntry *entryRoot = nil;
	
	if (dbGame == nil)
	{
		return entryRoot;
	}
	else if ([dbGame isDataLoaded])
	{
		entryRoot = [dbGame entryRoot];
	}
	else
	{
		entryRoot = [dbGame loadEntryDataFromFilePtr:_dbFile->GetFilePtr() isEncrypted:_dbFile->IsEncrypted()];
	}
	
	return entryRoot;
}

@end

@implementation CocoaDSCheatManager

@synthesize _internalCheatManager;
@synthesize sessionList;
@dynamic currentGameCode;
@dynamic currentGameCRC;
@dynamic itemTotalCount;
@dynamic itemActiveCount;
@synthesize searchResultsList;
@dynamic searchCount;
@dynamic searchDidStart;
@synthesize rwlockCoreExecute;

- (id)init
{
	return [self initWithFileURL:nil];
}

- (id) initWithFileURL:(NSURL *)fileURL
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	rwlockCoreExecute = NULL;
	_internalCheatManager = new ClientCheatManager;
	
	sessionList = [[NSMutableArray alloc] initWithCapacity:100];
	if (sessionList == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	searchResultsList = [[NSMutableArray alloc] initWithCapacity:100];
	if (searchResultsList == nil)
	{
		[sessionList release];
		[self release];
		self = nil;
		return self;
	}
	
	if (fileURL != nil)
	{
		_internalCheatManager->SessionListLoadFromFile([CocoaDSUtil cPathFromFileURL:fileURL]);
	}
	
	return self;
}

- (void)dealloc
{
	[sessionList release];
	sessionList = nil;
	
	[searchResultsList release];
	searchResultsList = nil;
	
	delete _internalCheatManager;
	_internalCheatManager = NULL;
	
	[super dealloc];
}

- (NSString *) currentGameCode
{
	const char gameCodeTerminated[5] = {
		gameInfo.header.gameCode[0],
		gameInfo.header.gameCode[1],
		gameInfo.header.gameCode[2],
		gameInfo.header.gameCode[3],
		'\0'
	};
	
	return [NSString stringWithCString:gameCodeTerminated encoding:NSUTF8StringEncoding];
}

- (NSUInteger) currentGameCRC
{
	return (NSUInteger)gameInfo.crcForCheatsDb;
}

- (NSUInteger) itemTotalCount
{
	return (NSUInteger)_internalCheatManager->GetTotalCheatCount();
}

- (NSUInteger) itemActiveCount
{
	return (NSUInteger)_internalCheatManager->GetActiveCheatCount();
}

- (CocoaDSCheatItem *) newItem
{
	CocoaDSCheatItem *newCocoaItem = nil;
	
	[self willChangeValueForKey:@"itemTotalCount"];
	[self willChangeValueForKey:@"itemActiveCount"];
	
	ClientCheatItem *newItem = _internalCheatManager->NewItem();
	
	[self didChangeValueForKey:@"itemTotalCount"];
	[self didChangeValueForKey:@"itemActiveCount"];
	
	if (newItem == NULL)
	{
		return newCocoaItem;
	}
	
	newCocoaItem = [[[CocoaDSCheatItem alloc] initWithCheatItem:newItem] autorelease];
	if (newCocoaItem == nil)
	{
		_internalCheatManager->RemoveItem(newItem);
		return newCocoaItem;
	}
	
	[sessionList addObject:newCocoaItem];
	
	return newCocoaItem;
}

- (CocoaDSCheatItem *) addExistingItem:(ClientCheatItem *)cheatItem
{
	CocoaDSCheatItem *addedCocoaItem = nil;
	if (cheatItem == nil)
	{
		return addedCocoaItem;
	}
	
	addedCocoaItem = [[[CocoaDSCheatItem alloc] initWithCheatItem:cheatItem] autorelease];
	return [self addExistingCocoaItem:addedCocoaItem];
}

- (CocoaDSCheatItem *) addExistingCocoaItem:(CocoaDSCheatItem *)cocoaCheatItem
{
	CocoaDSCheatItem *addedCocoaItem = nil;
	if ( (cocoaCheatItem == nil) || ([cocoaCheatItem clientData] == NULL) )
	{
		return addedCocoaItem;
	}
	
	[self willChangeValueForKey:@"itemTotalCount"];
	[self willChangeValueForKey:@"itemActiveCount"];
	
	ClientCheatItem *addedItem = _internalCheatManager->AddExistingItemNoDuplicate([cocoaCheatItem clientData]);
	
	[self didChangeValueForKey:@"itemTotalCount"];
	[self didChangeValueForKey:@"itemActiveCount"];
	
	if (addedItem == NULL)
	{
		return addedCocoaItem;
	}
	
	addedCocoaItem = cocoaCheatItem;
	[sessionList addObject:addedCocoaItem];
	
	return addedCocoaItem;
}

- (void) remove:(CocoaDSCheatItem *)cocoaCheatItem
{
	if ((cocoaCheatItem == NULL) || ![sessionList containsObject:cocoaCheatItem])
	{
		return;
	}
	
	NSUInteger selectionIndex = [sessionList indexOfObject:cocoaCheatItem];
	if (selectionIndex == NSNotFound)
	{
		return;
	}
	
	[self removeAtIndex:selectionIndex];
}

- (void) removeAtIndex:(NSUInteger)itemIndex
{
	[self willChangeValueForKey:@"itemTotalCount"];
	[self willChangeValueForKey:@"itemActiveCount"];
	
	_internalCheatManager->RemoveItemAtIndex((size_t)itemIndex);
	
	[self didChangeValueForKey:@"itemTotalCount"];
	[self didChangeValueForKey:@"itemActiveCount"];
	
	[sessionList removeObjectAtIndex:itemIndex];
}

- (BOOL) update:(CocoaDSCheatItem *)cocoaCheatItem
{
	BOOL result = NO;
	
	if (cocoaCheatItem == nil)
	{
		return result;
	}
	
	_internalCheatManager->ModifyItem([cocoaCheatItem clientData], [cocoaCheatItem clientData]);
	[cocoaCheatItem update];
	
	result = YES;
	return result;
}

- (BOOL) save
{
	const char *lastFilePath = _internalCheatManager->GetSessionListLastFilePath();
	const CheatSystemError error = _internalCheatManager->SessionListSaveToFile(lastFilePath);
	
	return (error == CheatSystemError_NoError) ? YES : NO;
}

- (void) directWriteInternalCheat:(CocoaDSCheatItem *)cocoaCheatItem
{
	if (cocoaCheatItem == nil)
	{
		return;
	}
	
	_internalCheatManager->DirectWriteInternalCheatItem([cocoaCheatItem clientData]);
}

- (void) loadFromMaster
{
	CHEATS *masterCheats = ClientCheatManager::GetMaster();
	if (masterCheats != NULL)
	{
		[self willChangeValueForKey:@"itemTotalCount"];
		[self willChangeValueForKey:@"itemActiveCount"];
		
		_internalCheatManager->LoadFromMaster();
		
		[self didChangeValueForKey:@"itemTotalCount"];
		[self didChangeValueForKey:@"itemActiveCount"];
		
		[sessionList removeAllObjects];
		
		ClientCheatList *itemList = _internalCheatManager->GetSessionList();
		const size_t itemCount = _internalCheatManager->GetTotalCheatCount();
		for (size_t i = 0; i < itemCount; i++)
		{
			CocoaDSCheatItem *cheatItem = [[CocoaDSCheatItem alloc] initWithCheatItem:itemList->GetItemAtIndex(i)];
			if (cheatItem != nil)
			{
				[sessionList addObject:[cheatItem autorelease]];
			}
		}
	}
}

- (void) applyToMaster
{
	_internalCheatManager->ApplyToMaster();
}

- (NSUInteger) databaseAddSelectedInEntry:(CocoaDSCheatDBEntry *)theEntry
{
	NSUInteger willAddCount = 0;
	if (theEntry == nil)
	{
		return willAddCount;
	}
	
	NSMutableArray *entryChildren = [theEntry child];
	if (entryChildren == nil)
	{
		return willAddCount;
	}
	
	for (CocoaDSCheatDBEntry *entry in entryChildren)
	{
		if ([entry isDirectory])
		{
			willAddCount += [self databaseAddSelectedInEntry:entry];
		}
		else if ([entry willAdd])
		{
			ClientCheatItem *newCheatItem = [entry newClientItem];
			CocoaDSCheatItem *newCocoaCheatItem = [self addExistingItem:newCheatItem];
			if (newCocoaCheatItem != nil)
			{
				willAddCount++;
			}
		}
	}
	
	return willAddCount;
}

- (NSUInteger) runExactValueSearch:(NSInteger)value byteSize:(UInt8)byteSize signType:(NSInteger)signType
{
	[self willChangeValueForKey:@"searchDidStart"];
	[self willChangeValueForKey:@"searchCount"];
	
	pthread_rwlock_rdlock(rwlockCoreExecute);
	size_t itemCount = _internalCheatManager->SearchExactValue((u32)value, (u8)byteSize, false);
	pthread_rwlock_unlock(rwlockCoreExecute);
	
	[self didChangeValueForKey:@"searchDidStart"];
	[self didChangeValueForKey:@"searchCount"];
	
	[searchResultsList removeAllObjects];
	
	if (itemCount > 0)
	{
		const DesmumeCheatSearchResultsList &dsSearchResults = _internalCheatManager->GetSearchResults();
		const size_t resultCount = dsSearchResults.size();
		
		NSMutableDictionary *newItem = nil;
		
		for (size_t i = 0; (i < resultCount) && (i < 100); i++)
		{
			newItem = [NSMutableDictionary dictionaryWithObjectsAndKeys:
					   [NSString stringWithFormat:@"0x02%06X", dsSearchResults[i].address], @"addressString",
					   [NSNumber numberWithUnsignedInteger:dsSearchResults[i].value], @"value",
					   nil];
			
			[searchResultsList addObject:newItem];
		}
	}
	
	return itemCount;
}

- (NSUInteger) runComparativeSearch:(NSInteger)typeID byteSize:(UInt8)byteSize signType:(NSInteger)signType
{
	const bool wasSearchAlreadyStarted = _internalCheatManager->SearchDidStart();
	
	[self willChangeValueForKey:@"searchDidStart"];
	[self willChangeValueForKey:@"searchCount"];
	
	pthread_rwlock_rdlock(rwlockCoreExecute);
	size_t itemCount = _internalCheatManager->SearchComparative((CheatSearchCompareStyle)typeID, (u8)byteSize, false);
	pthread_rwlock_unlock(rwlockCoreExecute);
	
	[self didChangeValueForKey:@"searchDidStart"];
	[self didChangeValueForKey:@"searchCount"];
	
	[searchResultsList removeAllObjects];
	
	if (!wasSearchAlreadyStarted && _internalCheatManager->SearchDidStart())
	{
		// Do nothing.
	}
	else if (itemCount > 0)
	{
		const DesmumeCheatSearchResultsList &dsSearchResults = _internalCheatManager->GetSearchResults();
		const size_t resultCount = dsSearchResults.size();
		
		NSMutableDictionary *newItem = nil;
		
		for (size_t i = 0; (i < resultCount) && (i < 100); i++)
		{
			newItem = [NSMutableDictionary dictionaryWithObjectsAndKeys:
					   [NSString stringWithFormat:@"0x02%06X", dsSearchResults[i].address], @"addressString",
					   [NSNumber numberWithUnsignedInteger:dsSearchResults[i].value], @"value",
					   nil];
			
			[searchResultsList addObject:newItem];
		}
	}
	
	return itemCount;
}

- (NSUInteger) searchCount
{
	return (NSUInteger)_internalCheatManager->GetSearchResultCount();;
}

- (BOOL) searchDidStart
{
	return (_internalCheatManager->SearchDidStart()) ? YES : NO;
}

- (void) searchReset
{
	[searchResultsList removeAllObjects];
	
	[self willChangeValueForKey:@"searchDidStart"];
	[self willChangeValueForKey:@"searchCount"];
	
	_internalCheatManager->SearchReset();
	
	[self didChangeValueForKey:@"searchDidStart"];
	[self didChangeValueForKey:@"searchCount"];
	
}

@end
