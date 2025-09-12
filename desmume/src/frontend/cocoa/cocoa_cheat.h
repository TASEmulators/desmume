/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2025 DeSmuME team

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

#include <string>
#include <vector>
#include "../../cheatSystem.h"
#undef BOOL

#import <Cocoa/Cocoa.h>


class ClientCheatItem;
class ClientCheatManager;

@interface CocoaDSCheatItem : NSObject
{
	ClientCheatItem *_internalData;
	BOOL _didAllocateInternalData;
	BOOL _disableWorkingCopyUpdate;
	BOOL willAdd;
	
	CocoaDSCheatItem *workingCopy;
	CocoaDSCheatItem *parent;
	
	BOOL _isMemAddressAlreadyUpdating;
}

@property (readonly, nonatomic, getter=clientData) ClientCheatItem *_internalData;
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

- (id) initWithCocoaCheatItem:(CocoaDSCheatItem *)cdsCheatItem;
- (id) initWithCheatItem:(ClientCheatItem *)cheatItem;
- (id) initWithCheatData:(const CHEATS_LIST *)cheatData;
- (char *) descriptionCString;
- (void) update;
- (CocoaDSCheatItem *) createWorkingCopy;
- (void) destroyWorkingCopy;
- (void) mergeToParent;

+ (void) setIconDirectory:(NSImage *)iconImage;
+ (NSImage *) iconDirectory;
+ (void) setIconInternalCheat:(NSImage *)iconImage;
+ (NSImage *) iconInternalCheat;
+ (void) setIconActionReplay:(NSImage *)iconImage;
+ (NSImage *) iconActionReplay;
+ (void) setIconCodeBreaker:(NSImage *)iconImage;
+ (NSImage *) iconCodeBreaker;

@end

@interface CocoaDSCheatDBEntry : NSObject
{
	CheatDBEntry *_dbEntry;
	
	CocoaDSCheatDBEntry *parent;
	NSMutableArray *child;
	NSInteger willAdd;
	NSString *codeString;
	
	BOOL needSetMixedState;
}

@property (readonly, nonatomic) NSString *name;
@property (readonly, nonatomic) NSString *comment;
@property (readonly, nonatomic) NSImage *icon;
@property (readonly, nonatomic) NSInteger entryCount;
@property (readonly, nonatomic) NSString *codeString;
@property (readonly, nonatomic) BOOL isDirectory;
@property (readonly, nonatomic) BOOL isCheatItem;
@property (assign) NSInteger willAdd;
@property (assign) BOOL needSetMixedState;
@property (assign) CocoaDSCheatDBEntry *parent;
@property (readonly, nonatomic) NSMutableArray *child;

- (id) initWithDBEntry:(const CheatDBEntry *)dbEntry;
- (ClientCheatItem *) newClientItem;

@end

@interface CocoaDSCheatDBGame : NSObject
{
	CheatDBGame *_dbGame;
	CocoaDSCheatDBEntry *entryRoot;
	NSUInteger index;
}

@property (assign) NSUInteger index;
@property (readonly, nonatomic) NSString *title;
@property (readonly, nonatomic) NSString *serial;
@property (readonly, nonatomic) NSUInteger crc;
@property (readonly, nonatomic) NSString *crcString;
@property (readonly, nonatomic) NSInteger dataSize;
@property (readonly, nonatomic) NSString *dataSizeString;
@property (readonly, nonatomic) BOOL isDataLoaded;
@property (readonly, nonatomic) NSInteger cheatItemCount;
@property (readonly, nonatomic) CocoaDSCheatDBEntry *entryRoot;

- (id) initWithGameEntry:(const CheatDBGame *)gameEntry;
- (CocoaDSCheatDBEntry *) loadEntryDataFromFilePtr:(FILE *)fp isEncrypted:(BOOL)isEncrypted;

@end

@interface CocoaDSCheatDatabase : NSObject
{
	CheatDBFile *_dbFile;
	CheatDBGameList *_dbGameList;
	NSMutableArray *gameList;
	
	NSURL *lastFileURL;
}

@property (readonly) NSURL *lastFileURL;
@property (readonly, nonatomic) NSString *description;
@property (readonly, nonatomic) NSString *formatString;
@property (readonly, nonatomic) BOOL isEncrypted;
@property (readonly) NSMutableArray *gameList;

- (id) initWithFileURL:(NSURL *)fileURL error:(CheatSystemError *)errorCode;
- (CocoaDSCheatDBGame *) getGameEntryUsingCode:(const char *)gameCode crc:(NSUInteger)crc;
- (CocoaDSCheatDBEntry *) loadGameEntry:(CocoaDSCheatDBGame *)dbGame;

@end

@interface CocoaDSCheatManager : NSObject
{
	ClientCheatManager *_internalCheatManager;
	NSMutableArray *sessionList;
	NSMutableArray *searchResultsList;
	
	pthread_rwlock_t *rwlockCoreExecute;
}

@property (readonly, nonatomic, getter=internalManager) ClientCheatManager *_internalCheatManager;
@property (readonly) NSMutableArray *sessionList;
@property (readonly, nonatomic) NSString *currentGameCode;
@property (readonly, nonatomic) NSUInteger currentGameCRC;
@property (readonly, nonatomic) NSUInteger itemTotalCount;
@property (readonly, nonatomic) NSUInteger itemActiveCount;
@property (readonly) NSMutableArray *searchResultsList;
@property (readonly, nonatomic) BOOL searchDidStart;
@property (readonly, nonatomic) NSUInteger searchCount;
@property (assign, nonatomic) pthread_rwlock_t *rwlockCoreExecute;

- (id) initWithFileURL:(NSURL *)fileURL;

- (CocoaDSCheatItem *) newItem;
- (CocoaDSCheatItem *) addExistingItem:(ClientCheatItem *)cheatItem;
- (CocoaDSCheatItem *) addExistingCocoaItem:(CocoaDSCheatItem *)cocoaCheatItem;
- (void) remove:(CocoaDSCheatItem *)cocoaCheatItem;
- (void) removeAtIndex:(NSUInteger)itemIndex;
- (BOOL) update:(CocoaDSCheatItem *)cocoaCheatItem;
- (BOOL) save;
- (void) directWriteInternalCheat:(CocoaDSCheatItem *)cocoaCheatItem;
- (void) loadFromMaster;
- (void) applyToMaster;

- (NSUInteger) databaseAddSelectedInEntry:(CocoaDSCheatDBEntry *)theEntry;

- (NSUInteger) runExactValueSearch:(NSInteger)value byteSize:(UInt8)byteSize signType:(NSInteger)signType;
- (NSUInteger) runComparativeSearch:(NSInteger)typeID byteSize:(UInt8)byteSize signType:(NSInteger)signType;
- (void) searchReset;

@end
