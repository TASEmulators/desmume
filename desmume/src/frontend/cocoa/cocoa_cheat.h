/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2023 DeSmuME team

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
#import <Cocoa/Cocoa.h>
#undef BOOL

class CHEATS;
class CHEATS_LIST;
class CHEATSEARCH;

enum CheatType
{
	CheatType_Internal = 0,
	CheatType_ActionReplay = 1,
	CheatType_CodeBreaker = 2
};

enum CheatFreezeType
{
	CheatFreezeType_Normal = 0,
	CheatFreezeType_CanDecrease = 1,
	CheatFreezeType_CanIncrease = 2
};

enum CheatSystemError
{
	CheatSystemError_NoError           = 0,
	CheatSystemError_FileOpenFailed    = 1,
	CheatSystemError_FileFormatInvalid = 2,
	CheatSystemError_CheatNotFound     = 3,
	CheatSystemError_ExportError       = 4,
	CheatSystemError_FileSaveFailed    = 5
};

class ClientCheatItem
{
protected:
	bool _isEnabled;
	bool _willAddFromDB;
	
	CheatType _cheatType;
	std::string _descriptionString;
	void *_clientData;
	
	// Internal cheat type parameters
	CheatFreezeType _freezeType;
	char _addressString[10+1];
	uint32_t _address;
	uint32_t _value;
	uint8_t _valueLength;
	
	// Action Replay parameters
	uint32_t _codeCount;
	std::string _rawCodeString;
	std::string _cleanCodeString;
	
	void _ConvertInternalToActionReplay();
	void _ConvertActionReplayToInternal();
	
public:
	ClientCheatItem();
	~ClientCheatItem();
	
	void Init(const CHEATS_LIST &inCheatItem);
	void Init(const ClientCheatItem &inCheatItem);
	
	void SetEnabled(bool theState);
	bool IsEnabled() const;
	
	void SetWillAddFromDB(bool theState);
	bool WillAddFromDB() const;
	
	CheatType GetType() const;
	void SetType(CheatType requestedType);
	bool IsSupportedType() const;
	
	const char* GetDescription() const;
	void SetDescription(const char *descriptionString);
	
	CheatFreezeType GetFreezeType() const;
	void SetFreezeType(CheatFreezeType theFreezeType);
	
	uint32_t GetAddress() const;
	void SetAddress(uint32_t theAddress);
	const char* GetAddressString() const;
	const char* GetAddressSixDigitString() const;
	void SetAddressSixDigitString(const char *sixDigitString);
	
	uint32_t GetValue() const;
	void SetValue(uint32_t theValue);
	uint8_t GetValueLength() const;
	void SetValueLength(uint8_t byteLength);
	
	void SetRawCodeString(const char *rawString, const bool willSaveValidatedRawString);
	const char* GetRawCodeString() const;
	const char* GetCleanCodeString() const;
	uint32_t GetCodeCount() const;
	
	void ClientToDesmumeCheatItem(CHEATS_LIST *outCheatItem) const;
};

class ClientCheatList
{
protected:
	std::vector<ClientCheatItem *> *_list;
	bool _engineNeedsUpdate;
	
public:
	ClientCheatList();
	~ClientCheatList();
	
	CheatSystemError LoadFromFile(const char *filePath);
	CheatSystemError SaveToFile(const char *filePath);
	
	ClientCheatItem* AddNew();
	ClientCheatItem* Add(ClientCheatItem *srcItem);
	ClientCheatItem* AddNoDuplicate(ClientCheatItem *srcItem);
	void Remove(ClientCheatItem *targetItem);
	void RemoveAtIndex(size_t index);
	void RemoveAll();
	void Update(const ClientCheatItem &srcItem, ClientCheatItem *targetItem);
	ClientCheatItem* UpdateAtIndex(const ClientCheatItem &srcItem, size_t index);
	
	size_t GetTotalCheatCount() const;
	size_t GetActiveCheatCount() const;
	std::vector<ClientCheatItem *>* GetCheatList() const;
	ClientCheatItem* GetItemAtIndex(size_t index) const;
	
	void ReplaceFromEngine(const CHEATS *engineCheatList);
	void CopyListToEngine(const bool willApplyOnlyEnabledItems, CHEATS *engineCheatList);
	void ApplyListToEngine();
	
	static CHEATS* GetMasterCheatList();
};

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
	ClientCheatItem *_internalData;
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

- (id) initWithCheatItem:(ClientCheatItem *)cheatItem;
- (id) initWithCocoaCheatItem:(CocoaDSCheatItem *)cdsCheatItem;
- (id) initWithCheatData:(const CHEATS_LIST *)cheatData;
- (char *) descriptionCString;
- (void) update;
- (CocoaDSCheatItem *) createWorkingCopy;
- (void) destroyWorkingCopy;
- (void) mergeToParent;

+ (void) setIconInternalCheat:(NSImage *)iconImage;
+ (NSImage *) iconInternalCheat;
+ (void) setIconActionReplay:(NSImage *)iconImage;
+ (NSImage *) iconActionReplay;
+ (void) setIconCodeBreaker:(NSImage *)iconImage;
+ (NSImage *) iconCodeBreaker;

@end

/********************************************************************************************
	CocoaDSCheatManager - OBJECTIVE-C CLASS

	This is an Objective-C wrapper class for DeSmuME's cheat list class.

	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
@interface CocoaDSCheatManager : NSObject
{
	ClientCheatList *_clientListData;
	NSMutableArray *list;
	
	NSUInteger untitledCount;
	NSString *dbTitle;
	NSString *dbDate;
	NSURL *lastFileURL;
}

@property (readonly, nonatomic, getter=clientListData) ClientCheatList *_clientListData;
@property (readonly) NSMutableArray *list;
@property (assign) NSUInteger untitledCount;
@property (copy) NSString *dbTitle;
@property (copy) NSString *dbDate;
@property (retain) NSURL *lastFileURL;

- (id) initWithFileURL:(NSURL *)fileURL;

- (BOOL) add:(CocoaDSCheatItem *)cheatItem;
- (void) remove:(CocoaDSCheatItem *)cheatItem;
- (BOOL) update:(CocoaDSCheatItem *)cheatItem;
- (BOOL) save;
- (NSUInteger) activeCount;
- (NSMutableArray *) cheatListFromDatabase:(NSURL *)fileURL errorCode:(NSInteger *)error;
- (void) applyInternalCheat:(CocoaDSCheatItem *)cheatItem;
- (void) loadFromEngine;
- (void) applyListToEngine;

+ (void) applyInternalCheatWithItem:(CocoaDSCheatItem *)cheatItem;
+ (void) applyInternalCheatWithAddress:(UInt32)address value:(UInt32)value bytes:(NSUInteger)bytes;
+ (NSMutableArray *) cheatListWithListObject:(CHEATS *)cheatList;
+ (NSMutableArray *) cheatListWithClientListObject:(ClientCheatList *)cheatList;
+ (NSMutableArray *) cheatListWithItemStructArray:(CHEATS_LIST *)cheatItemArray count:(NSUInteger)itemCount;

@end

@interface CocoaDSCheatSearch : NSObject
{
	CHEATSEARCH *listData;
	NSMutableArray *addressList;
	
	pthread_rwlock_t *rwlockCoreExecute;
	BOOL isUsingDummyRWlock;
	
	NSUInteger searchCount;
}

@property (readonly) CHEATSEARCH *listData;
@property (readonly) NSMutableArray *addressList;
@property (assign) pthread_rwlock_t *rwlockCoreExecute;
@property (readonly) NSUInteger searchCount;

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
