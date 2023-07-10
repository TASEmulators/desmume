/*
	Copyright (C) 2009-2023 DeSmuME team

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "types.h"

#define CHEAT_VERSION_MAJOR			2
#define CHEAT_VERSION_MINOR			0
#define MAX_CHEAT_LIST				100
#define	MAX_XX_CODE					1024
#define CHEAT_FILE_MIN_FGETS_BUFFER	32768
#define CHEAT_DB_GAME_TITLE_SIZE	256

#define CHEAT_TYPE_EMPTY 0xFF
#define CHEAT_TYPE_INTERNAL 0
#define CHEAT_TYPE_AR 1
#define CHEAT_TYPE_CODEBREAKER 2

struct CHEATS_LIST
{
	CHEATS_LIST()
	{
		memset(this,0,sizeof(*this));
		type = 0xFF;
	}
	u8 type;
	u8 enabled;
	// TODO
	u8		freezeType;			// 0 - normal freeze
								// 1 - can decrease
								// 2 - can increase
	u32		code[MAX_XX_CODE][2];
	char	description[1024];
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

enum CHEATS_DB_TYPE
{
	CHEATS_DB_R4 = 0
};

#pragma pack(push)
#pragma pack(1)
typedef struct FAT_R4
{
	u8	serial[4];
	u32	CRC;
	u64 addr;
} FAT_R4;
#pragma pack(pop)

class CHEATSEXPORT
{
private:
	CHEATS_DB_TYPE		type;
	bool				encrypted;
	FILE				*fp;
	size_t				fsize;
	u64					dataSize;
	u64					encOffset;
	FAT_R4				fat;
	bool				search();
	bool				getCodes();
	void				R4decrypt(u8 *buf, const size_t len, u64 n);

	u32					numCheats;
	CHEATS_LIST			*cheats;

	u8					error;		//	0 - no errors
									//	1 - open failed/file not found
									//	2 - file format is wrong (no valid header ID)
									//	3 - cheat not found in database
									//	4 - export error from database

public:
	CHEATSEXPORT() :
			fp(NULL),
			fsize(0),
			dataSize(0),
			encOffset(0),
			type(CHEATS_DB_R4),
			encrypted(false),
			numCheats(0),
			cheats(0),
			CRC(0),
			error(0)
	{
		memset(&fat, 0, sizeof(fat));
		memset(gametitle, 0, sizeof(gametitle));
		memset(date, 0, sizeof(date));
	}

	u8				gametitle[CHEAT_DB_GAME_TITLE_SIZE];
	u8				date[17];
	u32				CRC;
	bool			load(char *path);
	void			close();
	CHEATS_LIST		*getCheats();
	u32				getCheatsNum();
	u8				getErrorCode() { return error; }
};

extern CHEATS *cheats;
extern CHEATSEARCH *cheatSearch;
