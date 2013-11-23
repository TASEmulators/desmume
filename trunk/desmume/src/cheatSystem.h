/*
	Copyright (C) 2009-2012 DeSmuME team

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

#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <vector>

#define CHEAT_VERSION_MAJOR			2
#define CHEAT_VERSION_MINOR			0
#define MAX_CHEAT_LIST				100
#define	MAX_XX_CODE					1024
#define CHEAT_FILE_MIN_FGETS_BUFFER	32768
#define CHEAT_DB_GAME_TITLE_SIZE	256

struct CHEATS_LIST
{
	CHEATS_LIST()
	{
		memset(this,0,sizeof(*this));
		type = 0xFF;
	}
	u8		type;				// 0 - internal cheat system
								// 1 - Action Replay
								// 2 - Codebreakers
	BOOL	enabled;
	// TODO
	u8		freezeType;			// 0 - normal freeze
								// 1 - can decrease
								// 2 - can increase
	u32		code[MAX_XX_CODE][2];
	char	description[1024];
	int		num;
	u8		size;
};

class CHEATS
{
private:
	std::vector<CHEATS_LIST> list;
	u8					filename[MAX_PATH];
	u32					currentGet;

	void	clear();
	void	ARparser(CHEATS_LIST& cheat);
	char	*clearCode(char *s);

public:
	CHEATS()
		: currentGet(0)
	{
		memset(filename, 0, sizeof(filename));
	}
	~CHEATS() {}

	void	init(char *path);
	BOOL	add(u8 size, u32 address, u32 val, char *description, BOOL enabled);
	BOOL	update(u8 size, u32 address, u32 val, char *description, BOOL enabled, u32 pos);
	BOOL	add_AR(char *code, char *description, BOOL enabled);
	BOOL	update_AR(char *code, char *description, BOOL enabled, u32 pos);
	BOOL	add_AR_Direct(CHEATS_LIST cheat);
	BOOL	add_CB(char *code, char *description, BOOL enabled);
	BOOL	update_CB(char *code, char *description, BOOL enabled, u32 pos);
	BOOL	remove(u32 pos);
	void	getListReset();
	BOOL	getList(CHEATS_LIST *cheat);
	CHEATS_LIST*	getListPtr();
	BOOL	get(CHEATS_LIST *cheat, u32 pos);
	CHEATS_LIST*	getItemByIndex(const u32 pos);
	u32		getSize();
	size_t	getActiveCount();
	void	setDescription(const char *description, u32 pos);
	BOOL	save();
	BOOL	load();
	void	process();
	void	getXXcodeString(CHEATS_LIST cheat, char *res_buf);
	
	static BOOL XXCodeFromString(CHEATS_LIST *cheatItem, const std::string codeString);
	static BOOL XXCodeFromString(CHEATS_LIST *cheatItem, const char *codeString);
};

class CHEATSEARCH
{
private:
	u8	*statMem;
	u8	*mem;
	u32	amount;
	u32	lastRecord;

	u32	_type;
	u32	_size;
	u32	_sign;

public:
	CHEATSEARCH()
			: statMem(0), mem(0), amount(0), lastRecord(0), _type(0), _size(0), _sign(0) 
	{}
	~CHEATSEARCH() { close(); }
	BOOL start(u8 type, u8 size, u8 sign);
	BOOL close();
	u32 search(u32 val);
	u32 search(u8 comp);
	u32 getAmount();
	BOOL getList(u32 *address, u32 *curVal);
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
	u32					fsize;
	u32					dataSize;
	u32					encOffset;
	FAT_R4				fat;
	bool				search();
	bool				getCodes();
	void				R4decrypt(u8 *buf, u32 len, u32 n);

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
		memset(date, 0, sizeof(date));
		gametitle = (u8 *)malloc(CHEAT_DB_GAME_TITLE_SIZE);
		memset(gametitle, 0, CHEAT_DB_GAME_TITLE_SIZE);
	}
	~CHEATSEXPORT()
	{
		free(gametitle);
		gametitle = NULL;
	}

	u8				*gametitle;
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


