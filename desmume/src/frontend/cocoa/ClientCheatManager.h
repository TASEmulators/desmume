/*
	Copyright (C) 2025 DeSmuME team

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

#ifndef _CLIENT_CHEAT_MANAGER_H_
#define _CLIENT_CHEAT_MANAGER_H_

#include <stdint.h>
#include <string>
#include "../../cheatSystem.h"
#undef BOOL


enum CheatType
{
	CheatType_Internal     = 0,
	CheatType_ActionReplay = 1,
	CheatType_CodeBreaker  = 2
};

enum CheatFreezeType
{
	CheatFreezeType_Normal      = 0,
	CheatFreezeType_CanDecrease = 1,
	CheatFreezeType_CanIncrease = 2
};

enum CheatSearchStyle
{
	CheatSearchStyle_ExactValue  = 0,
	CheatSearchStyle_Comparative = 1
};

enum CheatSearchCompareStyle
{
	CheatSearchCompareStyle_GreaterThan = 0,
	CheatSearchCompareStyle_LesserThan  = 1,
	CheatSearchCompareStyle_Equals      = 2,
	CheatSearchCompareStyle_NotEquals   = 3
};

class ClientCheatManager;

union DesmumeCheatSearchItem
{
	uint64_t data;
	
	struct
	{
		uint32_t address;
		uint32_t value;
	};
};
typedef union DesmumeCheatSearchItem DesmumeCheatSearchItem;

typedef std::vector<DesmumeCheatSearchItem> DesmumeCheatSearchResultsList;

struct InternalCheatParam
{
	uint32_t address;
	uint32_t value;
	uint8_t valueLength;
};
typedef struct InternalCheatParam InternalCheatParam;

class ClientCheatItem
{
protected:
	ClientCheatManager *_cheatManager;
	
	bool _isEnabled;
	bool _willAddFromDB;
	
	CheatType _cheatType;
	std::string _nameString;
	std::string _commentString;
	
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
	
	void SetCheatManager(ClientCheatManager *cheatManager);
	ClientCheatManager* GetCheatManager() const;
	
	void SetEnabled(bool theState);
	bool IsEnabled() const;
	
	void SetWillAddFromDB(bool theState);
	bool WillAddFromDB() const;
	
	CheatType GetType() const;
	void SetType(CheatType requestedType);
	bool IsSupportedType() const;
	
	const char* GetName() const;
	void SetName(const char *nameString);
	
	const char* GetComments() const;
	void SetComments(const char *commentString);
	
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
	const std::string& GetCleanCodeCppString() const;
	uint32_t GetCodeCount() const;
	
	void ClientToDesmumeCheatItem(CHEATS_LIST *outCheatItem) const;
};

class ClientCheatList
{
private:
	ClientCheatItem* __AddItem(const ClientCheatItem *srcItem, const bool willCopy, const bool allowDuplicates);
	
protected:
	std::vector<ClientCheatItem *> *_list;
	
public:
	ClientCheatList();
	~ClientCheatList();
	
	CheatSystemError LoadFromFile(const char *filePath);
	CheatSystemError SaveToFile(const char *filePath);
	
	bool IsItemDuplicate(const ClientCheatItem *srcItem);
	
	ClientCheatItem* AddNew();
	ClientCheatItem* AddNewItemCopy(const ClientCheatItem *srcItem);
	ClientCheatItem* AddNewItemCopyNoDuplicate(const ClientCheatItem *srcItem);
	ClientCheatItem* AddExistingItemNoDuplicate(const ClientCheatItem *srcItem);
	
	bool Remove(ClientCheatItem *targetItem);
	bool RemoveAtIndex(size_t index);
	void RemoveAll();
	
	bool Update(const ClientCheatItem &srcItem, ClientCheatItem *targetItem);
	bool UpdateAtIndex(const ClientCheatItem &srcItem, size_t index);
	
	size_t GetTotalCheatCount() const;
	size_t GetActiveCheatCount() const;
	std::vector<ClientCheatItem *>* GetCheatList() const;
	size_t GetIndexOfItem(const ClientCheatItem *cheatItem) const;
	ClientCheatItem* GetItemAtIndex(size_t index) const;
	
	void ReplaceFromEngine(const CHEATS *engineCheatList);
	void CopyListToEngine(const bool willApplyOnlyEnabledItems, CHEATS *engineCheatList);
};

class ClientCheatSearcher
{
protected:
	CHEATSEARCH *_desmumeSearcher;
	uint8_t _searchValueLength;
	size_t _resultsCount;
	bool _didSearchStart;
	DesmumeCheatSearchResultsList _resultsList;
	
public:
	ClientCheatSearcher();
	~ClientCheatSearcher();
	
	bool DidStart() const;
	void Reset();
	size_t SearchExactValue(uint32_t value, uint8_t valueLength, bool performSignedSearch);
	size_t SearchComparative(CheatSearchCompareStyle compareStyle, uint8_t valueLength, bool performSignedSearch);
	const DesmumeCheatSearchResultsList& RefreshResults();
	const DesmumeCheatSearchResultsList& GetResults();
	size_t GetResultCount() const;
};

class ClientCheatDatabase
{
protected:
	ClientCheatList *_list;
	std::string _title;
	std::string _description;
	std::string _lastFilePath;
	
public:
	ClientCheatDatabase();
	~ClientCheatDatabase();
	
	ClientCheatList* GetList() const;
	ClientCheatList* LoadFromFile(const char *dbFilePath);
	
	const char* GetTitle() const;
	const char* GetDescription() const;
};

class ClientCheatManager
{
protected:
	ClientCheatList *_currentSessionList;
	ClientCheatDatabase *_currentDatabase;
	ClientCheatSearcher *_currentSearcher;
	
	ClientCheatItem *_selectedItem;
	size_t _selectedItemIndex;
	uint32_t _untitledCount;
	std::string _currentSessionLastFilePath;
	
	std::vector<InternalCheatParam> _pendingInternalCheatWriteList;
	
	bool _masterNeedsUpdate;
	
public:
	ClientCheatManager();
	virtual ~ClientCheatManager();
	
	static CHEATS* GetMaster();
	static void SetMaster(const CHEATS *masterCheats);
	
	ClientCheatList* GetSessionList() const;
	const char* GetSessionListLastFilePath() const;
	
	virtual CheatSystemError SessionListLoadFromFile(const char *filePath);
	virtual CheatSystemError SessionListSaveToFile(const char *filePath);
	
	ClientCheatItem* SetSelectedItemByIndex(size_t index);
	
	ClientCheatItem* NewItem();
	ClientCheatItem* AddExistingItemNoDuplicate(const ClientCheatItem *theItem);
	
	void RemoveItem(ClientCheatItem *theItem);
	void RemoveItemAtIndex(size_t index);
	void RemoveSelectedItem();
	
	void ModifyItem(const ClientCheatItem *srcItem, ClientCheatItem *targetItem);
	void ModifyItemAtIndex(const ClientCheatItem *srcItem, size_t index);
	
	size_t GetTotalCheatCount() const;
	size_t GetActiveCheatCount() const;
	
	void LoadFromMaster();
	void ApplyToMaster();
	void MasterNeedsUpdate();
	
	ClientCheatList* GetDatabaseList() const;
	ClientCheatList* DatabaseListLoadFromFile(const char *dbFilePath);
	const char* GetDatabaseTitle() const;
	const char* GetDatabaseDescription() const;
	
	bool SearchDidStart() const;
	void SearchReset();
	size_t SearchExactValue(uint32_t value, uint8_t valueLength, bool performSignedSearch);
	size_t SearchComparative(CheatSearchCompareStyle compareStyle, uint8_t valueLength, bool performSignedSearch);
	const DesmumeCheatSearchResultsList& SearchResultsRefresh();
	const DesmumeCheatSearchResultsList& GetSearchResults();
	size_t GetSearchResultCount() const;
	
	void DirectWriteInternalCheatAtIndex(size_t index);
	void DirectWriteInternalCheatItem(const ClientCheatItem *cheatItem);
	void DirectWriteInternalCheat(uint32_t targetAddress, uint32_t newValue32, size_t newValueLength);
	void ApplyPendingInternalCheatWrites();
};

#endif // _CLIENT_CHEAT_MANAGER_H_
