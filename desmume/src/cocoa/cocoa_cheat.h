/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2013 DeSmuME team

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
#include "../cheatSystem.h"
#undef BOOL

@class CocoaDSCore;


/********************************************************************************************
	CocoaDSCheatItem - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for DeSmuME's cheat item struct.
 
	The cheat item data is not freed upon release of this object. This is by design.

	Thread Safety:
		Assume that all methods are not thread-safe. This was done for performance
		reasons. The caller of this class' methods is expected to handle thread safety.
 ********************************************************************************************/
@interface CocoaDSCheatItem : NSObject
{
	CHEATS_LIST *data;
	CHEATS_LIST *internalData;
	BOOL willAdd;
	pthread_mutex_t mutexData;
	
	CocoaDSCheatItem *workingCopy;
	CocoaDSCheatItem *parent;
}

@property (assign) CHEATS_LIST *data;
@property (assign) BOOL willAdd;
@property (assign, nonatomic) BOOL enabled;
@property (assign, nonatomic) NSInteger cheatType;
@property (assign, nonatomic) NSImage *cheatTypeIcon;
@property (assign, nonatomic) BOOL isSupportedCheatType;
@property (assign, nonatomic) NSInteger freezeType;
@property (assign, nonatomic) NSString *description;
@property (assign, nonatomic) NSUInteger codeCount;
@property (assign, nonatomic) NSString *code;
@property (assign, nonatomic) UInt8 bytes;
@property (assign, nonatomic) UInt32 memAddress;
@property (assign, nonatomic) NSString *memAddressString;
@property (assign, nonatomic) NSString *memAddressSixDigitString;
@property (assign, nonatomic) SInt64 value;
@property (readonly) CocoaDSCheatItem *workingCopy;
@property (assign) CocoaDSCheatItem *parent;

- (id) initWithCheatData:(CHEATS_LIST *)cheatData;
- (BOOL) retainData;
- (char *) descriptionCString;
- (void) update;
- (CocoaDSCheatItem *) createWorkingCopy;
- (void) destroyWorkingCopy;
- (void) mergeFromWorkingCopy;
- (void) mergeToParent;
- (void) setDataWithDictionary:(NSDictionary *)dataDict;
- (NSDictionary *) dataDictionary;

@end

/********************************************************************************************
	CocoaDSCheatManager - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for DeSmuME's cheat list class.

	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
@interface CocoaDSCheatManager : NSObject
{
	CHEATS *listData;
	NSMutableArray *list;
	
	CocoaDSCore *cdsCore;
	NSUInteger untitledCount;
	NSString *dbTitle;
	NSString *dbDate;
}

@property (readonly) CHEATS *listData;
@property (readonly) NSMutableArray *list;
@property (retain) CocoaDSCore *cdsCore;
@property (assign) NSUInteger untitledCount;
@property (copy) NSString *dbTitle;
@property (copy) NSString *dbDate;

- (id) initWithCore:(CocoaDSCore *)core;
- (id) initWithCore:(CocoaDSCore *)core fileURL:(NSURL *)fileURL;
- (id) initWithCore:(CocoaDSCore *)core listData:(CHEATS *)cheatList;
- (id) initWithCore:(CocoaDSCore *)core fileURL:(NSURL *)fileURL listData:(CHEATS *)cheatList;

- (BOOL) add:(CocoaDSCheatItem *)cheatItem;
- (void) remove:(CocoaDSCheatItem *)cheatItem;
- (BOOL) update:(CocoaDSCheatItem *)cheatItem;
- (BOOL) save;
- (NSUInteger) activeCount;
- (NSMutableArray *) cheatListFromDatabase:(NSURL *)fileURL errorCode:(NSInteger *)error;
- (void) applyInternalCheat:(CocoaDSCheatItem *)cheatItem;

+ (void) setMasterCheatList:(CocoaDSCheatManager *)cheatListManager;
+ (void) applyInternalCheatWithItem:(CocoaDSCheatItem *)cheatItem;
+ (void) applyInternalCheatWithAddress:(UInt32)address value:(UInt32)value bytes:(NSUInteger)bytes;
+ (NSMutableArray *) cheatListWithListObject:(CHEATS *)cheatList;
+ (NSMutableArray *) cheatListWithItemStructArray:(CHEATS_LIST *)cheatItemArray count:(NSUInteger)itemCount;
+ (NSMutableDictionary *) cheatItemWithType:(NSInteger)cheatTypeID description:(NSString *)description;

@end

@interface CocoaDSCheatSearch : NSObject
{
	CHEATSEARCH *listData;
	NSMutableArray *addressList;
	
	CocoaDSCore *cdsCore;
	NSUInteger searchCount;
}

@property (readonly) CHEATSEARCH *listData;
@property (readonly) NSMutableArray *addressList;
@property (retain) CocoaDSCore *cdsCore;
@property (readonly) NSUInteger searchCount;

- (id) initWithCore:(CocoaDSCore *)core;

- (NSUInteger) runExactValueSearch:(NSInteger)value byteSize:(UInt8)byteSize signType:(NSInteger)signType;
- (void) runExactValueSearchOnThread:(id)object;
- (NSUInteger) runComparativeSearch:(NSInteger)typeID byteSize:(UInt8)byteSize signType:(NSInteger)signType;
- (void) runComparativeSearchOnThread:(id)object;
- (void) reset;

+ (NSMutableArray *) addressListWithListObject:(CHEATSEARCH *)addressList maxItems:(NSUInteger)maxItemCount;

@end

@interface CocoaDSCheatSearchParams : NSObject
{
	NSInteger comparativeSearchType;
	NSInteger value;
	UInt8 byteSize;
	NSInteger signType;
}

@property (assign) NSInteger comparativeSearchType;
@property (assign) NSInteger value;
@property (assign) UInt8 byteSize;
@property (assign) NSInteger signType;

@end
