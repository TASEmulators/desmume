/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <string.h>
#include "common.h"

#define CHEAT_VERSION_MAJOR		2
#define CHEAT_VERSION_MINOR		0
#define MAX_CHEAT_LIST			100
#define	MAX_XX_CODE				255

typedef struct
{
	u8		type;				// 0 - internal cheat system
								// 1 - Action Replay
								// 2 - Codebreakers
	BOOL	enabled;
	u32		code[MAX_XX_CODE][2];
	char	description[75];
	int		num;
	u8		size;

} CHEATS_LIST;

class CHEATS
{
private:
	CHEATS_LIST			list[MAX_CHEAT_LIST];
	u16					num;
	u8					filename[MAX_PATH];
	u32					currentGet;

	u8					*stack;
	u16					numStack;
	
	void	clear();
	void	ARparser(CHEATS_LIST cheat);
	BOOL	XXcodePreParser(CHEATS_LIST *cheat, char *code);
	char	*clearCode(char *s);

public:
	CHEATS():
				 num(0), currentGet(0), stack(0), numStack(0)
	{
		memset(list, 0, sizeof(list)); 
		memset(filename, 0, sizeof(filename));
	}
	~CHEATS() {}

	void	init(char *path);
	BOOL	add(u8 size, u32 address, u32 val, char *description, BOOL enabled);
	BOOL	update(u8 size, u32 address, u32 val, char *description, BOOL enabled, u32 pos);
	BOOL	add_AR(char *code, char *description, BOOL enabled);
	BOOL	update_AR(char *code, char *description, BOOL enabled, u32 pos);
	BOOL	add_CB(char *code, char *description, BOOL enabled);
	BOOL	update_CB(char *code, char *description, BOOL enabled, u32 pos);
	BOOL	remove(u32 pos);
	void	getListReset();
	BOOL	getList(CHEATS_LIST *cheat);
	BOOL	get(CHEATS_LIST *cheat, u32 pos);
	u32		getSize();
	BOOL	save();
	BOOL	load();
	BOOL	push();
	BOOL	pop();
	void	stackClear();
	void	process();
	void	getXXcodeString(CHEATS_LIST cheat, char *res_buf);
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

extern CHEATS *cheats;
extern CHEATSEARCH *cheatSearch;


