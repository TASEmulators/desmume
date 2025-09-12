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

#include "ClientCheatManager.h"
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
	_nameString = "No description.";
	_commentString = "";
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
	this->_nameString = inCheatItem.description;
	this->_commentString = "";
	
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
	this->SetName(inCheatItem.GetName());
	this->SetComments(inCheatItem.GetComments());
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

const char* ClientCheatItem::GetName() const
{
	return this->_nameString.c_str();
}

void ClientCheatItem::SetName(const char *nameString)
{
	if (nameString == NULL)
	{
		this->_nameString = "";
	}
	else
	{
		this->_nameString = nameString;
	}
}

const char* ClientCheatItem::GetComments() const
{
	return this->_commentString.c_str();
}

void ClientCheatItem::SetComments(const char *commentString)
{
	if (commentString == NULL)
	{
		this->_commentString = "";
	}
	else
	{
		this->_commentString = commentString;
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
	
	u32 function = this->_address & 0x0FFFFFFF;
	u32 truncatedValue = this->_value;
	
	switch (this->_valueLength)
	{
		case 1:
			function |= 0x20000000;
			truncatedValue &= 0x000000FF;
			break;
			
		case 2:
			function |= 0x10000000;
			truncatedValue &= 0x0000FFFF;
			break;
			
		case 3:
			truncatedValue &= 0x00FFFFFF;
			break;
			
		default:
			break;
	}
	
	memset(workingCodeBuffer, 0, sizeof(workingCodeBuffer));
	snprintf(workingCodeBuffer, 16+1+1, "%08X %08X", function, truncatedValue);
	this->_rawCodeString = workingCodeBuffer;
	
	memset(workingCodeBuffer, 0, sizeof(workingCodeBuffer));
	snprintf(workingCodeBuffer, 16+1, "%08X%08X", function, truncatedValue);
	this->_cleanCodeString = workingCodeBuffer;
	
	this->_codeCount = 1;
}

void ClientCheatItem::_ConvertActionReplayToInternal()
{
	char workingCodeBuffer[11] = {0};
	size_t cleanCodeLength = this->_cleanCodeString.length();
	size_t i = 0;
	
	// Note that we're only searching for the first valid command for
	// a constant RAM write, since internal cheats can only support a
	// single constant RAM write.
	for (; i < cleanCodeLength; i+=16)
	{
		workingCodeBuffer[0] = '0';
		workingCodeBuffer[1] = 'x';
		strncpy(workingCodeBuffer + 2, this->_cleanCodeString.c_str() + i, 8);
		workingCodeBuffer[10] = '\0';
		
		if (workingCodeBuffer[2] == '2')
		{
			this->_valueLength = 1;
			workingCodeBuffer[2] = '0';
			break;
		}
		else if (workingCodeBuffer[2] == '1')
		{
			this->_valueLength = 2;
			workingCodeBuffer[2] = '0';
			break;
		}
		else if (workingCodeBuffer[2] == '0')
		{
			this->_valueLength = 4;
			break;
		}
		else
		{
			continue;
		}
	}
	
	if (i >= cleanCodeLength)
	{
		return;
	}
	
	strncpy(this->_addressString, workingCodeBuffer, sizeof(this->_addressString));
	sscanf(workingCodeBuffer + 2, "%x", &this->_address);
	
	memset(workingCodeBuffer, 0, sizeof(workingCodeBuffer));
	strncpy(workingCodeBuffer, this->_cleanCodeString.c_str() + i + 8, 8);
	sscanf(workingCodeBuffer, "%x", &this->_value);
	
	switch (this->_valueLength)
	{
		case 1:
			this->_value &= 0x000000FF;
			break;
			
		case 2:
			this->_value &= 0x0000FFFF;
			break;
			
		default:
			break;
	}
}

void ClientCheatItem::ClientToDesmumeCheatItem(CHEATS_LIST *outCheatItem) const
{
	if (outCheatItem == NULL)
	{
		return;
	}
	
	outCheatItem->type = this->_cheatType;
	outCheatItem->enabled = (this->_isEnabled) ? 1 : 0;
	strncpy(outCheatItem->description, this->_nameString.c_str(), sizeof(outCheatItem->description));
	
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
		dbError = exporter->getErrorCode();
	}

	delete exporter;
	exporter = NULL;
	
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
		
		newItem->SetName(newDesc);
	}
	else
	{
		newItem->SetName("Untitled");
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
