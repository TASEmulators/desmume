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

#include "common.h"

#define CHEAT_VERSION_MAJOR		1
#define CHEAT_VERSION_MINOR		3
#define MAX_CHEAT_LIST 100

typedef struct
{
	u8		type;				// 0 - internal cheat system
								// 1 - Action Replay
								// 2 - Codebreakers
	BOOL	enabled;
	u32		hi[255];
	u32		lo[255];
	char	description[75];
	int		num;
	u8		size;

} CHEATS_LIST;

extern void cheatsInit(char *path);
extern BOOL cheatsAdd(u8 size, u32 address, u32 val, char *description, BOOL enabled);
extern BOOL cheatsUpdate(u8 size, u32 address, u32 val, char *description, BOOL enabled, u32 pos);
extern BOOL cheatsAdd_AR(char *code, char *description, BOOL enabled);
extern BOOL cheatsUpdate_AR(char *code, char *description, BOOL enabled, u32 pos);
extern BOOL cheatsAdd_CB(char *code, char *description, BOOL enabled);
extern BOOL cheatsUpdate_CB(char *code, char *description, BOOL enabled, u32 pos);
extern BOOL cheatsRemove(u32 pos);
extern void cheatsGetListReset();
extern BOOL cheatsGetList(CHEATS_LIST *cheat);
extern BOOL cheatsGet(CHEATS_LIST *cheat, u32 pos);
extern u32	cheatsGetSize();
extern BOOL cheatsSave();
extern BOOL cheatsLoad();
extern BOOL cheatsPush();
extern BOOL cheatsPop();
extern void cheatsStackClear();
extern void cheatsProcess();
extern void cheatGetXXcodeString(CHEATS_LIST cheat, char *res_buf);
extern void cheatsDisable(bool disable);

// ==================================================== cheat search
extern void cheatsSearchInit(u8 type, u8 size, u8 sign);
extern void cheatsSearchClose();
extern u32 cheatsSearchValue(u32 val);
extern u32 cheatsSearchComp(u8 comp);
extern u32 cheatSearchNumber();
extern BOOL cheatSearchGetList(u32 *address, u32 *curVal);
extern void cheatSearchGetListReset();
