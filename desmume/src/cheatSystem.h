/*
	Copyright (C) 2009-2025 DeSmuME team

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

#ifndef _CHEAT_SYSTEM_H_
#define _CHEAT_SYSTEM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "types.h"

#define CHEAT_VERSION_MAJOR			2
#define CHEAT_VERSION_MINOR			0
#define	MAX_XX_CODE					1024
#define CHEAT_FILE_MIN_FGETS_BUFFER	32768

#define CHEAT_TYPE_EMPTY 0xFF
#define CHEAT_TYPE_INTERNAL 0
#define CHEAT_TYPE_AR 1
#define CHEAT_TYPE_CODEBREAKER 2

enum CheatSystemError
{
	CheatSystemError_NoError           = 0,
	CheatSystemError_FileOpenFailed    = 1,
	CheatSystemError_FileFormatInvalid = 2,
	CheatSystemError_GameNotFound      = 3,
	CheatSystemError_LoadEntryError    = 4,
	CheatSystemError_FileSaveFailed    = 5,
	CheatSystemError_FileDoesNotExist  = 6
};

struct CHEATS_LIST
{
	CHEATS_LIST()
	{
		memset(this,0,sizeof(*this));
		type = CHEAT_TYPE_EMPTY;
	}
	u8 type;
	u8 enabled;
	// TODO
	u8		freezeType;			// 0 - normal freeze
								// 1 - can decrease
								// 2 - can increase
	u32		code[MAX_XX_CODE][2];
	
	union
	{
		char description[1024];
		
		struct
		{
			char descriptionMajor[512];
			char descriptionMinor[512];
		};
	};
	
	u32		num;
	u8		size;
};

class CHEATS
{
private:
	std::vector<CHEATS_LIST> _list;
	u8					_filename[MAX_PATH];
	size_t				_currentGet;

	char	*clearCode(char *s);

public:
	CHEATS()
		: _currentGet(0)
	{
		memset(_filename, 0, sizeof(_filename));
	}
	~CHEATS() {}

	void	clear();
	void	init(const char *thePath);
	const char* getFilePath() const;
	void	setFilePath(const char *thePath);
	
	size_t	addItem(const CHEATS_LIST &srcCheat);
	bool	add(u8 size, u32 address, u32 val, char *description, bool enabled);
	bool	add(u8 size, u32 address, u32 val, char *description, u8 enabled);
	
	bool	updateItemAtIndex(const CHEATS_LIST &srcCheat, const size_t pos);
	bool	update(u8 size, u32 address, u32 val, char *description, bool enabled, const size_t pos);
	bool	update(u8 size, u32 address, u32 val, char *description, u8 enabled, const size_t pos);
	
	bool	move(size_t srcPos, size_t dstPos);
	
	size_t	add_AR_Direct(const CHEATS_LIST &srcCheat);
	bool	add_AR(char *code, char *description, bool enabled);
	bool	add_AR(char *code, char *description, u8 enabled);
	
	bool	update_AR(char *code, char *description, bool enabled, const size_t pos);
	bool	update_AR(char *code, char *description, u8 enabled, const size_t pos);
	
	bool	add_CB(char *code, char *description, bool enabled);
	bool	add_CB(char *code, char *description, u8 enabled);
	
	bool	update_CB(char *code, char *description, bool enabled, const size_t pos);
	bool	update_CB(char *code, char *description, u8 enabled, const size_t pos);
	
	bool	remove(const size_t pos);
	
	void	toggle(bool enabled, const size_t pos);
	void	toggle(u8 enablbed, const size_t pos);
	
	void	getListReset();
	bool	getList(CHEATS_LIST *cheat);
	CHEATS_LIST*	getListPtr();
	bool	copyItemFromIndex(const size_t pos, CHEATS_LIST &outCheatItem);
	CHEATS_LIST* getItemPtrAtIndex(const size_t pos) const;
	size_t	getListSize() const;
	size_t	getActiveCount() const;
	void	setDescription(const char *description, const size_t pos);
	bool	save();
	bool	load();
	bool	process(int targetType) const;
	
	static void JitNeedsReset();
	static bool ResetJitIfNeeded();
	
	template<size_t LENGTH> static bool DirectWrite(const int targetProc, const u32 targetAddress, u32 newValue);
	static bool DirectWrite(const size_t newValueLength, const int targetProc, const u32 targetAddress, u32 newValue);
	
	static bool ARparser(const CHEATS_LIST &cheat);
	
	static void StringFromXXCode(const CHEATS_LIST &srcCheatItem, char *outCStringBuffer);
	static bool XXCodeFromString(const std::string codeString, CHEATS_LIST &outCheatItem);
	static bool XXCodeFromString(const char *codeString, CHEATS_LIST &outCheatItem);
};

class CHEATSEARCH
{
private:
	u8	*_statMem;
	u8	*_mem;
	u32	_amount;
	u32	_lastRecord;

	u32	_type;
	u32	_size;
	u32	_sign;

public:
	CHEATSEARCH()
			: _statMem(0), _mem(0), _amount(0), _lastRecord(0), _type(0), _size(0), _sign(0) 
	{}
	~CHEATSEARCH() { this->close(); }
	bool start(u8 type, u8 size, u8 sign);
	void close();
	u32 search(u32 val);
	u32 search(u8 comp);
	u32 getAmount();
	bool getList(u32 *address, u32 *curVal);
	void getListReset();
};

#define CHEATDB_OFFSET_FILE_DESCRIPTION    0x00000010
#define CHEATDB_FILEOFFSET_FIRST_FAT_ENTRY 0x00000100

enum CheatDBFileFormat
{
	CheatDBFileFormat_Undefined = 0,
	CheatDBFileFormat_R4        = 1,
	CheatDBFileFormat_Unknown   = 65535
};

// This struct maps to the FAT entries in the R4 cheat database file.
#pragma pack(push)
#pragma pack(1)
typedef struct FAT_R4
{
	u8  serial[4];
	u32 CRC;
	u64 addr;
} FAT_R4;
#pragma pack(pop)

// Wrapper for a single entry in a memory block that contains data read from a cheat database file.
// This struct also maintains the hierarchical relationships between entries.
struct CheatDBEntry
{
	u8   *base;       // Pointer to the entry's base location in memory.
	char *name;       // Pointer to the entry's name string.
	char *note;       // Pointer to the entry's note string.
	u32  *codeLength; // Pointer to the entry's 32-bit code length in bytes. This value is NULL if the entry is a directory.
	u32  *codeData;   // Pointer to the entry's code data, provided in pairs of 32-bit values. This value is NULL if the entry is a directory.
	
	CheatDBEntry *parent;
	std::vector<CheatDBEntry> child;
};
typedef struct CheatDBEntry CheatDBEntry;

class CheatDBGame
{
protected:
	u32 _baseOffset; // This offset is relative to the file head.
	u32 _firstEntryOffset; // This offset is relative to the file head.
	u32 _encryptOffset; // This offset is relative to the memory address of this->_entryDataRawPtr.
	
	u32 _rawDataSize;
	u32 _workingDataSize;
	u32 _crc;
	u32 _entryCount;
	
	std::string _title;
	char _serial[4 + 1];
	
	u8 *_entryDataRawPtr;
	u8 *_entryData;
	CheatDBEntry _entryRoot;
	u32 _cheatItemCount;
	
	void _UpdateDirectoryParents(CheatDBEntry &directory);
	bool _CreateCheatItemFromCheatEntry(const CheatDBEntry &inEntry, const bool isHierarchical, CHEATS_LIST &outCheatItem);
	size_t _DirectoryAddCheatsFlat(const CheatDBEntry &directory, const bool isHierarchical, size_t cheatIndex, CHEATS_LIST *outCheatsList);
	
public:
	CheatDBGame();
	CheatDBGame(const u32 encryptOffset, const FAT_R4 &fat, const u32 rawDataSize);
	CheatDBGame(FILE *fp, const bool isEncrypted, const u32 encryptOffset, const FAT_R4 &fat, const u32 rawDataSize, u8 (&workingBuffer)[1024]);
	~CheatDBGame();
	
	void SetInitialProperties(const u32 rawDataSize, const u32 encryptOffset, const FAT_R4 &fat);
	void LoadPropertiesFromFile(FILE *fp, const bool isEncrypted, u8 (&workingBuffer)[1024]);
	
	u32 GetBaseOffset() const;
	u32 GetFirstEntryOffset() const;
	u32 GetEncryptOffset() const;
	u32 GetRawDataSize() const;
	u32 GetWorkingDataSize() const;
	u32 GetCRC() const;
	u32 GetEntryCount() const;
	u32 GetCheatItemCount() const;
	const char* GetTitle() const;
	const char* GetSerial() const;
	
	const CheatDBEntry& GetEntryRoot() const;
	const u8* GetEntryRawData() const;
	bool IsEntryDataLoaded() const;
	
	u8* LoadEntryData(FILE *fp, const bool isEncrypted);
	size_t ParseEntriesToCheatsListFlat(CHEATS_LIST *outCheatsList);
};

typedef std::vector<CheatDBGame> CheatDBGameList;

class CheatDBFile
{
protected:
	std::string _path;
	std::string _description;
	std::string _formatString;
	
	CheatDBFileFormat _format;
	bool _isEncrypted;
	size_t _size;
	
	FILE *_fp;
	
	CheatDBGame _ReadGame(const u32 encryptOffset, const FAT_R4 &fat, const u32 gameDataSize, u8 (&workingBuffer)[1024]);
	
public:
	CheatDBFile();
	~CheatDBFile();
	
	static void R4Decrypt(u8 *buf, const size_t len, u64 n);
	static bool ReadToBuffer(FILE *fp, const size_t fileOffset, const bool isEncrypted, const size_t encryptOffset, const size_t requestedSize, u8 *outBuffer);
	
	FILE* GetFilePtr() const;
	bool IsEncrypted() const;
	const char* GetDescription() const;
	CheatDBFileFormat GetFormat() const;
	const char* GetFormatString() const;
	
	CheatSystemError OpenFile(const char *filePath);
	void CloseFile();
	
	u32 LoadGameList(const char *gameCode, const u32 gameDatabaseCRC, CheatDBGameList &outList);
};

class CHEATSEXPORT
{
private:
	CheatDBFile _dbFile;
	CheatDBGameList _tempGameList;
	CheatDBGame *_selectedDbGame;
	CHEATS_LIST *_cheats;
	CheatSystemError _lastError;

public:
	CHEATSEXPORT();
	~CHEATSEXPORT();
	
	bool load(const char *path);
	void close();
	
	CHEATS_LIST *getCheats() const;
	size_t getCheatsNum() const;
	const char* getGameTitle() const;
	const char* getDescription() const;
	CheatSystemError getErrorCode() const;
};

CheatDBGame* GetCheatDBGameEntryFromList(const CheatDBGameList &gameList, const char *gameCode, const u32 gameDatabaseCRC);
void CheatItemGenerateDescriptionHierarchical(const char *itemName, const char *itemNote, CHEATS_LIST &outCheatItem);
void CheatItemGenerateDescriptionFlat(const char *folderName, const char *folderNote, const char *itemName, const char *itemNote, CHEATS_LIST &outCheatItem);

extern CHEATS *cheats;
extern CHEATSEARCH *cheatSearch;

#endif // _CHEAT_SYSTEM_H_
