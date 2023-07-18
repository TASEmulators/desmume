/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2023 DeSmuME team

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

#include "../../cheatSystem.h"
#include "../../MMU.h"
#undef BOOL


size_t CheatConvertRawCodeToCleanCode(const char *inRawCodeString, const size_t requestedCodeCount, std::string &outCleanCodeString)
{
	size_t cleanCodeLength = 0;
	if ( (inRawCodeString == NULL) ||
		 (requestedCodeCount == 0) )
	{
		return cleanCodeLength;
	}
	
	char *cleanCodeWorkingBuffer = (char *)malloc((requestedCodeCount * 16) + 1);
	memset(cleanCodeWorkingBuffer, 0, (requestedCodeCount * 16) + 1);
	
	size_t rawCodeLength = strlen(inRawCodeString);
	// remove wrong chars
	for (size_t i = 0; (i < rawCodeLength) && (cleanCodeLength < (requestedCodeCount * 16)); i++)
	{
		char c = inRawCodeString[i];
		//apparently 100% of pokemon codes were typed with the letter O in place of zero in some places
		//so let's try to adjust for that here
		static const char *AR_Valid = "Oo0123456789ABCDEFabcdef";
		if (strchr(AR_Valid, c))
		{
			if ( (c == 'o') || (c == 'O') )
			{
				c = '0';
			}
			
			cleanCodeWorkingBuffer[cleanCodeLength++] = c;
		}
	}
	
	if ( (cleanCodeLength % 16) != 0)
	{
		// Code lines must always come in 8+8, where the first 8 characters
		// are used for the target address, and the second 8 characters are
		// used for the 32-bit value written to the target address. Therefore,
		// if the string length isn't divisible by 16, then it is considered
		// invalid.
		cleanCodeLength = 0;
		free(cleanCodeWorkingBuffer);
		return cleanCodeLength;
	}
	
	outCleanCodeString = cleanCodeWorkingBuffer;
	free(cleanCodeWorkingBuffer);
	
	return (cleanCodeLength / 16);
}

size_t CheatConvertCleanCodeToRawCode(const char *inCleanCodeString, std::string &outRawCodeString)
{
	if (inCleanCodeString == NULL)
	{
		return 0;
	}
	
	// Clean code strings are assumed to be already validated, so we're not
	// going to bother with any more validation here.
	
	const size_t cleanCodeLength = strlen(inCleanCodeString);
	const size_t codeCount = cleanCodeLength / 16;
	const size_t rawCodeLength = codeCount * (16 + 1 + 1);
	
	char *rawCodeWorkingBuffer = (char *)malloc(rawCodeLength);
	memset(rawCodeWorkingBuffer, 0, rawCodeLength);
	
	for (size_t i = 0; i < codeCount; i++)
	{
		const size_t c = i * 16;
		const size_t r = i * (16 + 1 + 1);
		
		rawCodeWorkingBuffer[r + 0] = inCleanCodeString[c + 0];
		rawCodeWorkingBuffer[r + 1] = inCleanCodeString[c + 1];
		rawCodeWorkingBuffer[r + 2] = inCleanCodeString[c + 2];
		rawCodeWorkingBuffer[r + 3] = inCleanCodeString[c + 3];
		rawCodeWorkingBuffer[r + 4] = inCleanCodeString[c + 4];
		rawCodeWorkingBuffer[r + 5] = inCleanCodeString[c + 5];
		rawCodeWorkingBuffer[r + 6] = inCleanCodeString[c + 6];
		rawCodeWorkingBuffer[r + 7] = inCleanCodeString[c + 7];
		rawCodeWorkingBuffer[r + 8] = ' ';
		rawCodeWorkingBuffer[r + 9] = inCleanCodeString[c + 8];
		rawCodeWorkingBuffer[r +10] = inCleanCodeString[c + 9];
		rawCodeWorkingBuffer[r +11] = inCleanCodeString[c +10];
		rawCodeWorkingBuffer[r +12] = inCleanCodeString[c +11];
		rawCodeWorkingBuffer[r +13] = inCleanCodeString[c +12];
		rawCodeWorkingBuffer[r +14] = inCleanCodeString[c +13];
		rawCodeWorkingBuffer[r +15] = inCleanCodeString[c +14];
		rawCodeWorkingBuffer[r +16] = inCleanCodeString[c +15];
		rawCodeWorkingBuffer[r +17] = '\n';
	}
	
	rawCodeWorkingBuffer[rawCodeLength - 1] = '\0';
	outRawCodeString = rawCodeWorkingBuffer;
	
	return codeCount;
}

bool IsCheatItemDuplicate(const ClientCheatItem *first, const ClientCheatItem *second)
{
	bool isDuplicate = false;
	
	if ( (first == NULL) || (second == NULL) )
	{
		return isDuplicate;
	}
	
	if (first == second)
	{
		isDuplicate = true;
		return isDuplicate;
	}
	
	const CheatType compareType = first->GetType();
	
	switch (compareType)
	{
		case CheatType_Internal:
		{
			if ( (first->GetAddress()     == second->GetAddress()) &&
				 (first->GetValueLength() == second->GetValueLength()) &&
				 (first->GetValue()       == second->GetValue()) )
			{
				isDuplicate = true;
			}
			break;
		}
			
		case CheatType_ActionReplay:
		{
			if ( (first->GetCodeCount()          == second->GetCodeCount()) &&
				 (first->GetCleanCodeCppString() == second->GetCleanCodeCppString()) )
			{
				isDuplicate = true;
			}
			break;
		}
			
		case CheatType_CodeBreaker:
		default:
			break;
	}
	
	return isDuplicate;
}

ClientCheatItem::ClientCheatItem()
{
	_cheatManager = NULL;
	
	_isEnabled = false;
	_willAddFromDB = false;
	
	_cheatType = CheatType_Internal;
	_descriptionMajorString = "No description.";
	_descriptionMinorString = "";
	_freezeType = CheatFreezeType_Normal;
	_address = 0x02000000;
	strncpy(_addressString, "0x02000000", sizeof(_addressString));
	_valueLength = 1;
	_value = 0;
	
	_codeCount = 1;
	_rawCodeString = "02000000 00000000";
	_cleanCodeString = "0200000000000000";
}

ClientCheatItem::~ClientCheatItem()
{
	
}

void ClientCheatItem::Init(const CHEATS_LIST &inCheatItem)
{
	char workingCodeBuffer[32];
	
	this->_isEnabled = (inCheatItem.enabled) ? true : false;
	
	this->_cheatType = (CheatType)inCheatItem.type;
	this->_descriptionMajorString = inCheatItem.description;
	this->_descriptionMinorString = "";
	
	this->_freezeType = (CheatFreezeType)inCheatItem.freezeType;
	this->_valueLength = inCheatItem.size + 1; // CHEATS_LIST.size value range is [1...4], but starts counting from 0.
	this->_address = inCheatItem.code[0][0];
	this->_addressString[0] = '0';
	this->_addressString[1] = 'x';
	snprintf(this->_addressString + 2, sizeof(this->_addressString) - 2, "%08X", this->_address);
	this->_value = inCheatItem.code[0][1];
	
	snprintf(workingCodeBuffer, 18, "%08X %08X", this->_address, this->_value);
	this->_rawCodeString = workingCodeBuffer;
	snprintf(workingCodeBuffer, 17, "%08X%08X", this->_address, this->_value);
	this->_cleanCodeString = workingCodeBuffer;
	
	if (this->_cheatType == CheatType_Internal)
	{
		this->_codeCount = 1;
	}
	else if (this->_cheatType == CheatType_ActionReplay)
	{
		this->_codeCount = inCheatItem.num;
		
		for (size_t i = 1; i < this->_codeCount; i++)
		{
			snprintf(workingCodeBuffer, 19, "\n%08X %08X", inCheatItem.code[i][0], inCheatItem.code[i][1]);
			this->_rawCodeString += workingCodeBuffer;
			snprintf(workingCodeBuffer, 17, "%08X%08X", inCheatItem.code[i][0], inCheatItem.code[i][1]);
			this->_cleanCodeString += workingCodeBuffer;
		}
	}
}

void ClientCheatItem::Init(const ClientCheatItem &inCheatItem)
{
	this->SetEnabled(inCheatItem.IsEnabled());
	this->SetMajorDescription(inCheatItem.GetMajorDescription());
	this->SetType(inCheatItem.GetType());
	this->SetFreezeType(inCheatItem.GetFreezeType());
	
	if (this->GetType() == CheatType_Internal)
	{
		this->SetValueLength(inCheatItem.GetValueLength());
		this->SetAddress(inCheatItem.GetAddress());
		this->SetValue(inCheatItem.GetValue());
	}
	else
	{
		this->SetRawCodeString(inCheatItem.GetRawCodeString(), false);
	}
}

void ClientCheatItem::SetCheatManager(ClientCheatManager *cheatManager)
{
	this->_cheatManager = cheatManager;
}

ClientCheatManager* ClientCheatItem::GetCheatManager() const
{
	return this->_cheatManager;
}

void ClientCheatItem::SetEnabled(bool theState)
{
	if ( (this->_isEnabled != theState) && (this->_cheatManager != NULL) )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_isEnabled = theState;
}

bool ClientCheatItem::IsEnabled() const
{
	return this->_isEnabled;
}

void ClientCheatItem::SetWillAddFromDB(bool theState)
{
	this->_willAddFromDB = theState;
}

bool ClientCheatItem::WillAddFromDB() const
{
	return this->_willAddFromDB;
}

CheatType ClientCheatItem::GetType() const
{
	return this->_cheatType;
}

void ClientCheatItem::SetType(CheatType requestedType)
{
	switch (requestedType)
	{
		case CheatType_Internal:
		case CheatType_ActionReplay:
		case CheatType_CodeBreaker:
			// Do nothing.
			break;
			
		default:
			// Bail if the cheat type is invalid.
			return;
	}
	
	if ( (this->_cheatType != requestedType) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_cheatType = requestedType;
}

bool ClientCheatItem::IsSupportedType() const
{
	return (this->_cheatType != CheatType_CodeBreaker);
}

const char* ClientCheatItem::GetMajorDescription() const
{
	return this->_descriptionMajorString.c_str();
}

void ClientCheatItem::SetMajorDescription(const char *descriptionString)
{
	if (descriptionString == NULL)
	{
		this->_descriptionMajorString = "";
	}
	else
	{
		this->_descriptionMajorString = descriptionString;
	}
}

CheatFreezeType ClientCheatItem::GetFreezeType() const
{
	return this->_freezeType;
}

void ClientCheatItem::SetFreezeType(CheatFreezeType theFreezeType)
{
	switch (theFreezeType)
	{
		case CheatFreezeType_Normal:
		case CheatFreezeType_CanDecrease:
		case CheatFreezeType_CanIncrease:
			// Do nothing.
			break;
			
		default:
			// Bail if the freeze type is invalid.
			return;
	}
	
	if ( (this->_freezeType != theFreezeType) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_freezeType = theFreezeType;
}

uint32_t ClientCheatItem::GetAddress() const
{
	if (this->_cheatType != CheatType_Internal)
	{
		return 0;
	}
	
	return this->_address;
}

void ClientCheatItem::SetAddress(uint32_t theAddress)
{
	if ( (this->_address != theAddress) && (this->_cheatType == CheatType_Internal) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_address = theAddress;
	
	this->_addressString[0] = '0';
	this->_addressString[1] = 'x';
	snprintf(this->_addressString + 2, 9, "%08X", theAddress);
	this->_addressString[10] = '\0';
	
	if (this->_cheatType == CheatType_Internal)
	{
		this->_ConvertInternalToActionReplay();
	}
}

const char* ClientCheatItem::GetAddressString() const
{
	return this->_addressString;
}

const char* ClientCheatItem::GetAddressSixDigitString() const
{
	return (this->_addressString + 4);
}

void ClientCheatItem::SetAddressSixDigitString(const char *sixDigitString)
{
	this->_addressString[0] = '0';
	this->_addressString[1] = 'x';
	this->_addressString[2] = '0';
	this->_addressString[3] = '2';
	
	if (sixDigitString == NULL)
	{
		this->_addressString[4] = '0';
		this->_addressString[5] = '0';
		this->_addressString[6] = '0';
		this->_addressString[7] = '0';
		this->_addressString[8] = '0';
		this->_addressString[9] = '0';
	}
	else
	{
		this->_addressString[4] = sixDigitString[0];
		this->_addressString[5] = sixDigitString[1];
		this->_addressString[6] = sixDigitString[2];
		this->_addressString[7] = sixDigitString[3];
		this->_addressString[8] = sixDigitString[4];
		this->_addressString[9] = sixDigitString[5];
	}
	
	this->_addressString[10] = '\0';
	
	u32 theAddress = 0;
	sscanf(this->_addressString + 2, "%x", &theAddress);
	
	if ( (this->_address != theAddress) && (this->_cheatType == CheatType_Internal) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_address = theAddress;
	
	if (this->_cheatType == CheatType_Internal)
	{
		this->_ConvertInternalToActionReplay();
	}
}

uint32_t ClientCheatItem::GetValue() const
{
	return this->_value;
}

void ClientCheatItem::SetValue(uint32_t theValue)
{
	if ( (this->_value != theValue) && (this->_cheatType == CheatType_Internal) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_value = theValue;
	
	if (this->_cheatType == CheatType_Internal)
	{
		this->_ConvertInternalToActionReplay();
	}
}

uint8_t ClientCheatItem::GetValueLength() const
{
	return this->_valueLength;
}

void ClientCheatItem::SetValueLength(uint8_t byteLength)
{
	if ( (this->_valueLength != byteLength) && (this->_cheatType == CheatType_Internal) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	this->_valueLength = byteLength;
	
	if (this->_cheatType == CheatType_Internal)
	{
		this->_ConvertInternalToActionReplay();
	}
}

void ClientCheatItem::SetRawCodeString(const char *rawString, const bool willSaveValidatedRawString)
{
	std::string newCleanCodeString;
	
	size_t cleanCodeCount = CheatConvertRawCodeToCleanCode(rawString, 1024, this->_cleanCodeString);
	if (cleanCodeCount == 0)
	{
		return;
	}
	
	this->_codeCount = (uint32_t)cleanCodeCount;
	
	if (willSaveValidatedRawString)
	{
		// Using the validated clean code string, the raw code string will be
		// rewritten using the following format for each line:
		// XXXXXXXX XXXXXXXX\n
		CheatConvertCleanCodeToRawCode(this->_cleanCodeString.c_str(), this->_rawCodeString);
	}
	else
	{
		// The passed in raw code string will be saved, regardless of any syntax
		// errors, flaws, or formatting issues that it may contain.
		this->_rawCodeString = rawString;
	}
	
	if ( (this->_cheatType == CheatType_ActionReplay) && (this->_cheatManager != NULL) && this->_isEnabled )
	{
		this->_cheatManager->MasterNeedsUpdate();
	}
	
	if (this->_cheatType == CheatType_ActionReplay)
	{
		this->_ConvertActionReplayToInternal();
	}
}

const char* ClientCheatItem::GetRawCodeString() const
{
	return this->_rawCodeString.c_str();
}

const char* ClientCheatItem::GetCleanCodeString() const
{
	return this->_cleanCodeString.c_str();
}

const std::string& ClientCheatItem::GetCleanCodeCppString() const
{
	return this->_cleanCodeString;
}

uint32_t ClientCheatItem::GetCodeCount() const
{
	return this->_codeCount;
}

void ClientCheatItem::_ConvertInternalToActionReplay()
{
	char workingCodeBuffer[16+1+1];
	
	u32 truncatedValue = this->_value;
	
	switch (this->_valueLength)
	{
		case 1:
			truncatedValue &= 0x000000FF;
			break;
			
		case 2:
			truncatedValue &= 0x0000FFFF;
			break;
			
		case 3:
			truncatedValue &= 0x00FFFFFF;
			break;
			
		default:
			break;
	}
	
	memset(workingCodeBuffer, 0, sizeof(workingCodeBuffer));
	snprintf(workingCodeBuffer, 16+1+1, "%08X %08X", this->_address, truncatedValue);
	this->_rawCodeString = workingCodeBuffer;
	
	memset(workingCodeBuffer, 0, sizeof(workingCodeBuffer));
	snprintf(workingCodeBuffer, 16+1, "%08X%08X", this->_address, truncatedValue);
	this->_cleanCodeString = workingCodeBuffer;
	
	this->_codeCount = 1;
}

void ClientCheatItem::_ConvertActionReplayToInternal()
{
	this->_addressString[0] = '0';
	this->_addressString[1] = 'x';
	strncpy(this->_addressString + 2, this->_cleanCodeString.c_str(), 8);
	this->_addressString[10] = '\0';
	sscanf(this->_addressString + 2, "%x", &this->_address);
	
	char workingCodeBuffer[9];
	memset(workingCodeBuffer, 0, sizeof(workingCodeBuffer));
	strncpy(workingCodeBuffer, this->_cleanCodeString.c_str() + 8, 8);
	sscanf(workingCodeBuffer, "%x", &this->_value);
	
	this->_valueLength = 4;
}

void ClientCheatItem::ClientToDesmumeCheatItem(CHEATS_LIST *outCheatItem) const
{
	if (outCheatItem == NULL)
	{
		return;
	}
	
	outCheatItem->type = this->_cheatType;
	outCheatItem->enabled = (this->_isEnabled) ? 1 : 0;
	strncpy(outCheatItem->description, this->_descriptionMajorString.c_str(), sizeof(outCheatItem->description));
	
	switch (this->_cheatType)
	{
		case CheatType_Internal:
			outCheatItem->num = 1;
			outCheatItem->size = this->_valueLength - 1; // CHEATS_LIST.size value range is [1...4], but starts counting from 0.
			outCheatItem->freezeType = (u8)this->_freezeType;
			outCheatItem->code[0][0] = this->_address;
			outCheatItem->code[0][1] = this->_value;
			break;
			
		case CheatType_ActionReplay:
		case CheatType_CodeBreaker:
		{
			outCheatItem->num = this->_codeCount;
			outCheatItem->size = 3;
			outCheatItem->freezeType = CheatFreezeType_Normal;
			
			char valueString[9];
			valueString[8] = '\0';
			
			const char *cleanCodeStr = this->_cleanCodeString.c_str();
			for (size_t i = 0; i < this->_codeCount; i++)
			{
				strncpy(valueString, cleanCodeStr + (i * 16) + 0, 8);
				sscanf(valueString, "%x", &outCheatItem->code[i][0]);
				
				strncpy(valueString, cleanCodeStr + (i * 16) + 8, 8);
				sscanf(valueString, "%x", &outCheatItem->code[i][1]);
			}
			break;
		}
			
		default:
			break;
	}
}

#pragma mark -

ClientCheatList::ClientCheatList()
{
	_list = new std::vector<ClientCheatItem *>;
}

ClientCheatList::~ClientCheatList()
{
	delete this->_list;
	this->_list = NULL;
}

CheatSystemError ClientCheatList::LoadFromFile(const char *filePath)
{
	CheatSystemError error = CheatSystemError_NoError;
	
	if (filePath == NULL)
	{
		error = CheatSystemError_FileOpenFailed;
		return error;
	}
	
	CHEATS *tempEngineList = new CHEATS;
	tempEngineList->init((char *)filePath);
	this->ReplaceFromEngine(tempEngineList);
	delete tempEngineList;
	
	return error;
}

CheatSystemError ClientCheatList::SaveToFile(const char *filePath)
{
	CheatSystemError error = CheatSystemError_NoError;
	
	if (filePath == NULL)
	{
		error = CheatSystemError_FileOpenFailed;
		return error;
	}
	
	CHEATS *tempEngineList = new CHEATS;
	tempEngineList->setFilePath(filePath);
	
	this->CopyListToEngine(false, tempEngineList);
	
	bool isSaveSuccessful = tempEngineList->save();
	if (!isSaveSuccessful)
	{
		error = CheatSystemError_FileSaveFailed;
	}
	
	delete tempEngineList;
	
	return error;
}

bool ClientCheatList::IsItemDuplicate(const ClientCheatItem *srcItem)
{
	bool isDuplicateFound = false;
	if (srcItem == NULL)
	{
		return isDuplicateFound;
	}
	
	const CheatType compareType = srcItem->GetType();
	
	const size_t totalCheatCount = this->_list->size();
	for (size_t i = 0; i < totalCheatCount; i++)
	{
		const ClientCheatItem *itemToCheck = (*this->_list)[i];
		if (itemToCheck == NULL)
		{
			continue;
		}
		
		if (srcItem == itemToCheck)
		{
			isDuplicateFound = true;
			break;
		}
		
		switch (compareType)
		{
			case CheatType_Internal:
				isDuplicateFound = ( (srcItem->GetAddress()     == itemToCheck->GetAddress()) &&
				                     (srcItem->GetValue()       == itemToCheck->GetValue()) &&
				                     (srcItem->GetValueLength() == itemToCheck->GetValueLength()) );
				break;
				
			case CheatType_ActionReplay:
				isDuplicateFound = ( (srcItem->GetCodeCount()          == itemToCheck->GetCodeCount()) &&
				                     (srcItem->GetCleanCodeCppString() == itemToCheck->GetCleanCodeCppString()) );
				break;
				
			case CheatType_CodeBreaker:
			default:
				break;
		}
		
		if (isDuplicateFound)
		{
			break;
		}
	}
	
	return isDuplicateFound;
}

ClientCheatItem* ClientCheatList::__AddItem(const ClientCheatItem *srcItem, const bool willCopy, const bool allowDuplicates)
{
	ClientCheatItem *newItem = NULL;
	if (srcItem == NULL)
	{
		return newItem;
	}
	
	if (allowDuplicates || !this->IsItemDuplicate(srcItem))
	{
		if (willCopy)
		{
			this->_list->push_back(new ClientCheatItem);
			newItem->Init(*srcItem);
		}
		else
		{
			this->_list->push_back((ClientCheatItem *)srcItem);
		}
		
		newItem = this->_list->back();
	}
	
	return newItem;
}

ClientCheatItem* ClientCheatList::AddNew()
{
	ClientCheatItem *newItem = new ClientCheatItem;
	return this->__AddItem(newItem, false, true);
}

ClientCheatItem* ClientCheatList::AddNewItemCopy(const ClientCheatItem *srcItem)
{
	return this->__AddItem(srcItem, true, true);
}

ClientCheatItem* ClientCheatList::AddNewItemCopyNoDuplicate(const ClientCheatItem *srcItem)
{
	return this->__AddItem(srcItem, true, false);
}

ClientCheatItem* ClientCheatList::AddExistingItemNoDuplicate(const ClientCheatItem *srcItem)
{
	return this->__AddItem(srcItem, false, false);
}

bool ClientCheatList::Remove(ClientCheatItem *targetItem)
{
	return this->RemoveAtIndex( this->GetIndexOfItem(targetItem) );
}

bool ClientCheatList::RemoveAtIndex(size_t index)
{
	bool didRemoveItem = false;
	ClientCheatItem *targetItem = this->GetItemAtIndex(index);
	
	if (targetItem != NULL)
	{
		this->_list->erase( this->_list->begin() + index );
		delete targetItem;
		didRemoveItem = true;
	}
	
	return didRemoveItem;
}

void ClientCheatList::RemoveAll()
{
	const size_t cheatCount = this->_list->size();
	for (size_t i = 0; i < cheatCount; i++)
	{
		ClientCheatItem *itemToRemove = (*this->_list)[i];
		delete itemToRemove;
	}
	
	this->_list->clear();
}

bool ClientCheatList::Update(const ClientCheatItem &srcItem, ClientCheatItem *targetItem)
{
	return this->UpdateAtIndex(srcItem, this->GetIndexOfItem(targetItem));
}

bool ClientCheatList::UpdateAtIndex(const ClientCheatItem &srcItem, size_t index)
{
	bool didUpdateItem = false;
	ClientCheatItem *targetItem = this->GetItemAtIndex(index);
	
	if (targetItem != NULL)
	{
		targetItem->Init(srcItem);
		didUpdateItem = true;
	}
	
	return didUpdateItem;
}

size_t ClientCheatList::GetTotalCheatCount() const
{
	return this->_list->size();
}

size_t ClientCheatList::GetActiveCheatCount() const
{
	size_t activeCount = 0;
	const size_t totalCount = this->_list->size();
	
	for (size_t i = 0; i < totalCount; i++)
	{
		ClientCheatItem *cheatItem = (*this->_list)[i];
		if (cheatItem->IsEnabled())
		{
			activeCount++;
		}
	}
	
	return activeCount;
}

std::vector<ClientCheatItem *>* ClientCheatList::GetCheatList() const
{
	return this->_list;
}

size_t ClientCheatList::GetIndexOfItem(const ClientCheatItem *cheatItem) const
{
	size_t outIndex = ~0;
	if (cheatItem == NULL)
	{
		return outIndex;
	}
	
	const size_t cheatCount = this->_list->size();
	for (size_t i = 0; i < cheatCount; i++)
	{
		if (cheatItem == (*this->_list)[i])
		{
			return outIndex;
		}
	}
	
	return outIndex;
}

ClientCheatItem* ClientCheatList::GetItemAtIndex(size_t index) const
{
	if (index >= this->GetTotalCheatCount())
	{
		return NULL;
	}
	
	return (*this->_list)[index];
}

void ClientCheatList::ReplaceFromEngine(const CHEATS *engineCheatList)
{
	if (engineCheatList == NULL)
	{
		return;
	}
	
	this->RemoveAll();
	
	const size_t totalCount = engineCheatList->getListSize();
	for (size_t i = 0; i < totalCount; i++)
	{
		ClientCheatItem *newItem = this->AddNew();
		newItem->Init( *engineCheatList->getItemPtrAtIndex(i) );
	}
}

void ClientCheatList::CopyListToEngine(const bool willApplyOnlyEnabledItems, CHEATS *engineCheatList)
{
	if (engineCheatList == NULL)
	{
		return;
	}
	
	CHEATS_LIST tempEngineItem;
	
	engineCheatList->clear();
	
	const size_t totalCount = this->_list->size();
	for (size_t i = 0; i < totalCount; i++)
	{
		ClientCheatItem *cheatItem = (*this->_list)[i];
		if (cheatItem->IsEnabled() || !willApplyOnlyEnabledItems)
		{
			cheatItem->ClientToDesmumeCheatItem(&tempEngineItem);
			engineCheatList->addItem(tempEngineItem);
		}
	}
}

#pragma mark -

ClientCheatSearcher::ClientCheatSearcher()
{
	_desmumeSearcher = new CHEATSEARCH;
	_searchValueLength = 4;
	_resultsCount = 0;
	_didSearchStart = false;
	_resultsList.resize(0);
}

ClientCheatSearcher::~ClientCheatSearcher()
{
	delete this->_desmumeSearcher;
}

bool ClientCheatSearcher::DidStart() const
{
	return this->_didSearchStart;
}

void ClientCheatSearcher::Reset()
{
	this->_desmumeSearcher->close();
	this->_didSearchStart = false;
	this->_resultsCount = 0;
	
	this->_desmumeSearcher->getListReset();
	this->_resultsList.resize(0);
}

size_t ClientCheatSearcher::SearchExactValue(uint32_t value, uint8_t valueLength, bool performSignedSearch)
{
	if (!this->_didSearchStart)
	{
		this->_searchValueLength = valueLength;
		this->_didSearchStart = this->_desmumeSearcher->start((u8)CheatSearchStyle_ExactValue, this->_searchValueLength - 1, 0);
		this->_resultsCount = 0;
		this->_resultsList.resize(0);
	}
	
	if (this->_didSearchStart)
	{
		this->_resultsCount = (size_t)this->_desmumeSearcher->search(value);
		this->RefreshResults();
	}
	
	return this->_resultsCount;
}

size_t ClientCheatSearcher::SearchComparative(CheatSearchCompareStyle compareStyle, uint8_t valueLength, bool performSignedSearch)
{
	if (!this->_didSearchStart)
	{
		this->_searchValueLength = valueLength;
		this->_didSearchStart = this->_desmumeSearcher->start((u8)CheatSearchStyle_Comparative, this->_searchValueLength - 1, 0);
		this->_resultsCount = 0;
		this->_resultsList.resize(0);
	}
	else
	{
		this->_resultsCount = (size_t)this->_desmumeSearcher->search((u8)compareStyle);
		this->RefreshResults();
	}
	
	return this->_resultsCount;
}

const DesmumeCheatSearchResultsList& ClientCheatSearcher::RefreshResults()
{
	bool didReadResult = false;
	u32 readAddress = 0;
	u32 readValue = 0;
	size_t readResultIndex = 0;
	
	this->_desmumeSearcher->getListReset();
	this->_resultsList.clear();
	
	for (size_t i = 0; i < this->_resultsCount; i++)
	{
		didReadResult = this->_desmumeSearcher->getList(&readAddress, &readValue);
		if (didReadResult)
		{
			DesmumeCheatSearchItem searchResult;
			searchResult.address = readAddress;
			searchResult.value = readValue;
			
			this->_resultsList.push_back(searchResult);
			readResultIndex++;
		}
	}
	
	return this->_resultsList;
}

const DesmumeCheatSearchResultsList& ClientCheatSearcher::GetResults()
{
	return this->_resultsList;
}

size_t ClientCheatSearcher::GetResultCount() const
{
	return this->_resultsCount;
}

#pragma mark -

ClientCheatDatabase::ClientCheatDatabase()
{
	_list = new ClientCheatList;
	_title.resize(0);
	_description.resize(0);
	_lastFilePath.resize(0);
}

ClientCheatDatabase::~ClientCheatDatabase()
{
	delete this->_list;
}

ClientCheatList* ClientCheatDatabase::GetList() const
{
	return this->_list;
}

ClientCheatList* ClientCheatDatabase::LoadFromFile(const char *dbFilePath)
{
	if (dbFilePath == NULL)
	{
		return NULL;
	}
	
	CHEATSEXPORT *exporter = new CHEATSEXPORT();
	CheatSystemError dbError = CheatSystemError_NoError;
	
	bool didFileLoad = exporter->load((char *)dbFilePath);
	if (didFileLoad)
	{
		this->_list->RemoveAll();
		this->_title = exporter->getGameTitle();
		this->_description = exporter->getDescription();
		this->_lastFilePath = dbFilePath;
		
		const size_t itemCount = exporter->getCheatsNum();
		const CHEATS_LIST *dbItem = exporter->getCheats();
		
		for (size_t i = 0; i < itemCount; i++)
		{
			ClientCheatItem *newItem = this->_list->AddNew();
			if (newItem != NULL)
			{
				newItem->Init(dbItem[i]);
			}
		}
	}
	else
	{
		dbError = (CheatSystemError)exporter->getErrorCode();
	}

	delete exporter;
	exporter = nil;
	
	if (dbError != CheatSystemError_NoError)
	{
		return NULL;
	}
	
	return this->_list;
}

const char* ClientCheatDatabase::GetTitle() const
{
	return this->_title.c_str();
}

const char* ClientCheatDatabase::GetDescription() const
{
	return this->_description.c_str();
}

#pragma mark -

ClientCheatManager::ClientCheatManager()
{
	_currentSessionList = new ClientCheatList;
	_currentDatabase = new ClientCheatDatabase;
	_currentSearcher = new ClientCheatSearcher;
	_pendingInternalCheatWriteList.resize(0);
	
	_selectedItem = NULL;
	_selectedItemIndex = 0;
	
	_untitledCount = 0;
	
	_currentSessionLastFilePath.resize(0);
	
	_masterNeedsUpdate = true;
}

ClientCheatManager::~ClientCheatManager()
{
	delete this->_currentSearcher;
	delete this->_currentDatabase;
	delete this->_currentSessionList;
}

CHEATS* ClientCheatManager::GetMaster()
{
	return cheats;
}

void ClientCheatManager::SetMaster(const CHEATS *masterCheats)
{
	cheats = (CHEATS *)masterCheats;
}

ClientCheatList* ClientCheatManager::GetSessionList() const
{
	return this->_currentSessionList;
}

const char* ClientCheatManager::GetSessionListLastFilePath() const
{
	return this->_currentSessionLastFilePath.c_str();
}

CheatSystemError ClientCheatManager::SessionListLoadFromFile(const char *filePath)
{
	CheatSystemError error = CheatSystemError_NoError;
	
	if (filePath == NULL)
	{
		error = CheatSystemError_FileOpenFailed;
		return error;
	}
	
	error = this->_currentSessionList->LoadFromFile(filePath);
	if (error == CheatSystemError_NoError)
	{
		this->_currentSessionLastFilePath = filePath;
		
		const size_t totalCount = this->_currentSessionList->GetTotalCheatCount();
		for (size_t i = 0; i < totalCount; i++)
		{
			ClientCheatItem *cheatItem = this->_currentSessionList->GetItemAtIndex(i);
			cheatItem->SetCheatManager(this);
		}
	}
	
	return error;
}

CheatSystemError ClientCheatManager::SessionListSaveToFile(const char *filePath)
{
	CheatSystemError error = CheatSystemError_NoError;
	
	if (filePath == NULL)
	{
		error = CheatSystemError_FileSaveFailed;
		return error;
	}
	
	error = this->_currentSessionList->SaveToFile(filePath);
	if (error == CheatSystemError_NoError)
	{
		this->_currentSessionLastFilePath = filePath;
	}
	
	return error;
}

ClientCheatItem* ClientCheatManager::SetSelectedItemByIndex(size_t index)
{
	this->_selectedItemIndex = index;
	this->_selectedItem = this->_currentSessionList->GetItemAtIndex(index);
	
	return this->_selectedItem;
}

ClientCheatItem* ClientCheatManager::NewItem()
{
	ClientCheatItem *newItem = this->_currentSessionList->AddNew();
	newItem->SetCheatManager(this);
	
	this->_untitledCount++;
	if (this->_untitledCount > 1)
	{
		char newDesc[16];
		snprintf(newDesc, sizeof(newDesc), "Untitled %ld", (unsigned long)this->_untitledCount);
		
		newItem->SetMajorDescription(newDesc);
	}
	else
	{
		newItem->SetMajorDescription("Untitled");
	}
	
	if (newItem->IsEnabled())
	{
		this->_masterNeedsUpdate = true;
	}
	
	return newItem;
}

ClientCheatItem* ClientCheatManager::AddExistingItemNoDuplicate(const ClientCheatItem *theItem)
{
	ClientCheatItem *addedItem = this->_currentSessionList->AddExistingItemNoDuplicate(theItem);
	if (addedItem != NULL)
	{
		addedItem->SetCheatManager(this);
		
		if (addedItem->IsEnabled())
		{
			this->_masterNeedsUpdate = true;
		}
	}
	
	return addedItem;
}

void ClientCheatManager::RemoveItem(ClientCheatItem *targetItem)
{
	this->RemoveItemAtIndex( this->_currentSessionList->GetIndexOfItem(targetItem) );
}

void ClientCheatManager::RemoveItemAtIndex(size_t index)
{
	const ClientCheatItem *targetItem = this->_currentSessionList->GetItemAtIndex(index);
	if (targetItem == NULL)
	{
		return;
	}
	
	const bool willChangeMasterUpdateFlag = targetItem->IsEnabled();
	const bool didRemoveItem = this->_currentSessionList->RemoveAtIndex(index);
	
	if (didRemoveItem && willChangeMasterUpdateFlag)
	{
		this->_masterNeedsUpdate = true;
	}
}

void ClientCheatManager::RemoveSelectedItem()
{
	this->RemoveItemAtIndex(this->_selectedItemIndex);
}

void ClientCheatManager::ModifyItem(const ClientCheatItem *srcItem, ClientCheatItem *targetItem)
{
	if ( (srcItem != NULL) && (srcItem == targetItem) )
	{
		if (targetItem->IsEnabled())
		{
			this->_masterNeedsUpdate = true;
		}
		return;
	}
	
	this->ModifyItemAtIndex(srcItem, this->_currentSessionList->GetIndexOfItem(targetItem));
}

void ClientCheatManager::ModifyItemAtIndex(const ClientCheatItem *srcItem, size_t index)
{
	const ClientCheatItem *targetItem = this->_currentSessionList->GetItemAtIndex(index);
	if ( (srcItem == NULL) || (targetItem == NULL) )
	{
		return;
	}
	
	const bool willChangeMasterUpdateFlag = targetItem->IsEnabled();
	const bool didModifyItem = this->_currentSessionList->UpdateAtIndex(*srcItem, index);
	
	if (didModifyItem && willChangeMasterUpdateFlag)
	{
		this->_masterNeedsUpdate = true;
	}
}

size_t ClientCheatManager::GetTotalCheatCount() const
{
	return this->_currentSessionList->GetTotalCheatCount();
}

size_t ClientCheatManager::GetActiveCheatCount() const
{
	return this->_currentSessionList->GetActiveCheatCount();
}

void ClientCheatManager::LoadFromMaster()
{
	size_t activeCount = 0;
	const CHEATS *masterCheats = ClientCheatManager::GetMaster();
	
	if (masterCheats == NULL)
	{
		return;
	}
	
	this->_currentSessionLastFilePath = masterCheats->getFilePath();
	
	activeCount = this->_currentSessionList->GetActiveCheatCount();
	if (activeCount > 0)
	{
		this->_masterNeedsUpdate = true;
	}
	
	this->_currentSessionList->ReplaceFromEngine(masterCheats);
	
	const size_t totalCount = this->_currentSessionList->GetTotalCheatCount();
	for (size_t i = 0; i < totalCount; i++)
	{
		ClientCheatItem *cheatItem = this->_currentSessionList->GetItemAtIndex(i);
		cheatItem->SetCheatManager(this);
	}
	
	activeCount = this->_currentSessionList->GetActiveCheatCount();
	if (activeCount > 0)
	{
		this->_masterNeedsUpdate = true;
	}
}

void ClientCheatManager::ApplyToMaster()
{
	CHEATS *masterCheats = ClientCheatManager::GetMaster();
	if ( (masterCheats == NULL) || !this->_masterNeedsUpdate )
	{
		return;
	}
	
	this->_currentSessionList->CopyListToEngine(true, masterCheats);
	this->_masterNeedsUpdate = false;
}

void ClientCheatManager::MasterNeedsUpdate()
{
	this->_masterNeedsUpdate = true;
}

ClientCheatList* ClientCheatManager::GetDatabaseList() const
{
	return this->_currentDatabase->GetList();
}

ClientCheatList* ClientCheatManager::DatabaseListLoadFromFile(const char *dbFilePath)
{
	return this->_currentDatabase->LoadFromFile(dbFilePath);
}

const char* ClientCheatManager::GetDatabaseTitle() const
{
	return this->_currentDatabase->GetTitle();
}

const char* ClientCheatManager::GetDatabaseDescription() const
{
	return this->_currentDatabase->GetDescription();
}

bool ClientCheatManager::SearchDidStart() const
{
	return this->_currentSearcher->DidStart();
}

void ClientCheatManager::SearchReset()
{
	this->_currentSearcher->Reset();
}

size_t ClientCheatManager::SearchExactValue(uint32_t value, uint8_t valueLength, bool performSignedSearch)
{
	return this->_currentSearcher->SearchExactValue(value, valueLength, performSignedSearch);
}

size_t ClientCheatManager::SearchComparative(CheatSearchCompareStyle compareStyle, uint8_t valueLength, bool performSignedSearch)
{
	return this->_currentSearcher->SearchComparative(compareStyle, valueLength, performSignedSearch);
}

const DesmumeCheatSearchResultsList& ClientCheatManager::SearchResultsRefresh()
{
	return this->_currentSearcher->RefreshResults();
}

const DesmumeCheatSearchResultsList& ClientCheatManager::GetSearchResults()
{
	return this->_currentSearcher->GetResults();
}

size_t ClientCheatManager::GetSearchResultCount() const
{
	return this->_currentSearcher->GetResultCount();
}

void ClientCheatManager::DirectWriteInternalCheatAtIndex(size_t index)
{
	this->DirectWriteInternalCheatItem( this->_currentSessionList->GetItemAtIndex(index) );
}

void ClientCheatManager::DirectWriteInternalCheatItem(const ClientCheatItem *cheatItem)
{
	if ( (cheatItem == NULL) || (cheatItem->GetType() != CheatType_Internal) )
	{
		return;
	}
	
	this->DirectWriteInternalCheat( cheatItem->GetAddress(), cheatItem->GetValue(), cheatItem->GetValueLength() );
}

void ClientCheatManager::DirectWriteInternalCheat(uint32_t targetAddress, uint32_t newValue32, size_t newValueLength)
{
	targetAddress &= 0x00FFFFFF;
	targetAddress |= 0x02000000;
	
	InternalCheatParam cheatWrite;
	cheatWrite.address = targetAddress;
	cheatWrite.value = newValue32;
	cheatWrite.valueLength = newValueLength;
	
	this->_pendingInternalCheatWriteList.push_back(cheatWrite);
}

void ClientCheatManager::ApplyPendingInternalCheatWrites()
{
	bool needsJitReset = false;
	
	const size_t writeListSize = this->_pendingInternalCheatWriteList.size();
	if (writeListSize == 0)
	{
		return;
	}
	
	for (size_t i = 0; i < writeListSize; i++)
	{
		const InternalCheatParam cheatWrite = this->_pendingInternalCheatWriteList[i];
		
		const bool shouldResetJit = CHEATS::DirectWrite(cheatWrite.valueLength, ARMCPU_ARM9, cheatWrite.address, cheatWrite.value);
		if (shouldResetJit)
		{
			needsJitReset = true;
		}
	}
	
	this->_pendingInternalCheatWriteList.clear();
	
	if (needsJitReset)
	{
		CHEATS::JitNeedsReset();
		CHEATS::ResetJitIfNeeded();
	}
}

#pragma mark -

@implementation CocoaDSCheatItem

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
	return [NSString stringWithCString:_internalData->GetMajorDescription() encoding:NSUTF8StringEncoding];
}

- (void) setDescription:(NSString *)desc
{
	if (desc == nil)
	{
		_internalData->SetMajorDescription(NULL);
	}
	else
	{
		_internalData->SetMajorDescription([desc cStringUsingEncoding:NSUTF8StringEncoding]);
	}
	
	if ((workingCopy != nil) && !_disableWorkingCopyUpdate)
	{
		[workingCopy setDescription:desc];
	}
}

- (char *) descriptionCString
{
	return (char *)_internalData->GetMajorDescription();
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
		case CHEAT_TYPE_INTERNAL:
			[self setCheatTypeIcon:iconInternalCheat];
			[self setIsSupportedCheatType:YES];
			[self setMemAddress:[self memAddress]];
			[self setValue:[self value]];
			[self setBytes:[self bytes]];
			break;
			
		case CHEAT_TYPE_ACTION_REPLAY:
			[self setCheatTypeIcon:iconActionReplay];
			[self setIsSupportedCheatType:YES];
			[self setCode:[self code]];
			break;
			
		case CHEAT_TYPE_CODE_BREAKER:
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
	
	if ([self cheatType] == CHEAT_TYPE_INTERNAL)
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
	
	if ([self cheatType] == CHEAT_TYPE_INTERNAL)
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


@implementation CocoaDSCheatManager

@synthesize _internalCheatManager;
@synthesize sessionList;
@dynamic itemTotalCount;
@dynamic itemActiveCount;
@synthesize databaseList;
@dynamic databaseTitle;
@dynamic databaseDate;
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
	
	databaseList = [[NSMutableArray alloc] initWithCapacity:100];
	if (databaseList == nil)
	{
		[sessionList release];
		[self release];
		self = nil;
		return self;
	}
	
	searchResultsList = [[NSMutableArray alloc] initWithCapacity:100];
	if (searchResultsList == nil)
	{
		[sessionList release];
		[databaseList release];
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
	
	[databaseList release];
	databaseList = nil;
	
	[searchResultsList release];
	searchResultsList = nil;
	
	delete _internalCheatManager;
	_internalCheatManager = NULL;
	
	[super dealloc];
}

- (NSString *) databaseTitle
{
	return [NSString stringWithCString:_internalCheatManager->GetDatabaseTitle() encoding:NSUTF8StringEncoding];
}

- (NSString *) databaseDate
{
	return [NSString stringWithCString:_internalCheatManager->GetDatabaseDescription() encoding:NSUTF8StringEncoding];
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

- (NSMutableArray *) databaseListLoadFromFile:(NSURL *)fileURL errorCode:(NSInteger *)error
{
	if (fileURL == nil)
	{
		return nil;
	}
	
	[self willChangeValueForKey:@"databaseTitle"];
	[self willChangeValueForKey:@"databaseDate"];
	
	ClientCheatList *dbList = _internalCheatManager->DatabaseListLoadFromFile([CocoaDSUtil cPathFromFileURL:fileURL]);
	
	[self didChangeValueForKey:@"databaseTitle"];
	[self didChangeValueForKey:@"databaseDate"];
	
	if (dbList != NULL)
	{
		[databaseList removeAllObjects];
		
		const size_t itemCount = dbList->GetTotalCheatCount();
		for (size_t i = 0; i < itemCount; i++)
		{
			CocoaDSCheatItem *cheatItem = [[CocoaDSCheatItem alloc] initWithCheatItem:dbList->GetItemAtIndex(i)];
			if (cheatItem != nil)
			{
				[databaseList addObject:[cheatItem autorelease]];
			}
		}
	}
	
	return databaseList;
}

- (NSUInteger) databaseAddSelected
{
	NSUInteger addedItemCount = 0;
	
	for (CocoaDSCheatItem *dbItem in databaseList)
	{
		if ([dbItem willAdd])
		{
			ClientCheatItem *newCheatItem = new ClientCheatItem;
			newCheatItem->Init(*[dbItem clientData]);
			
			CocoaDSCheatItem *newCocoaCheatItem = [self addExistingItem:newCheatItem];
			if (newCocoaCheatItem != nil)
			{
				addedItemCount++;
			}
		}
	}
	
	return addedItemCount;
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
