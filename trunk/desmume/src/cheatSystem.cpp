/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright 2009 CrazyMax
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
#include "cheatSystem.h"
#include "mem.h"
#include "MMU.h"
#include "debug.h"

static	CHEATS_LIST			cheats[MAX_CHEAT_LIST];
static	u16					cheatsNum = 0;
static	u8					cheatFilename[MAX_PATH];
static	u32					cheatsCurrentGet = 0;

static	u8					*cheatsStack = NULL;
static	u16					cheatsNumStack = 0;

void cheatsInit(char *path)
{
	memset(cheats, 0, sizeof(cheats));
	for (int i = 0; i < MAX_CHEAT_LIST; i++)
		cheats[i].type = 0xFF;
	cheatsNum = 0;
	cheatsCurrentGet = 0;
	strcpy((char *)cheatFilename, path);
	strcat((char *)cheatFilename, "cht");

	if (cheatsStack) delete [] cheatsStack;
	cheatsStack = NULL;

	cheatsLoad();
}

BOOL cheatsAdd(u8 proc, u8 size, u32 address, u32 val, char *description, BOOL enabled)
{
	if (cheatsNum == MAX_CHEAT_LIST) return FALSE;
	cheats[cheatsNum].proc = proc;
	cheats[cheatsNum].hi[0] = address & 0x00FFFFFF;
	cheats[cheatsNum].lo[0] = val;
	cheats[cheatsNum].num = 1;
	cheats[cheatsNum].type = 0;
	cheats[cheatsNum].size = size;
	strcpy(cheats[cheatsNum].description, description);
	cheats[cheatsNum].enabled = enabled;
	cheatsNum++;
	return TRUE;
}

BOOL cheatsAdd_AR(char *code, char *description, BOOL enabled)
{
	return FALSE;
}

BOOL cheatsAdd_CB(char *code, char *description, BOOL enabled)
{
	return FALSE;
}

BOOL cheatsRemove(u32 pos)
{
	if (pos > cheatsNum) return FALSE;
	for (int i = pos; i < cheatsNum; i++)
		memcpy(&cheats[i], &cheats[i+1], sizeof(CHEATS_LIST));

	if (cheatsNum == 0) return FALSE;
	cheatsNum--;
	return TRUE;
}

void cheatsGetListReset()
{
	cheatsCurrentGet = 0;
	return;
}

BOOL cheatsGetList(CHEATS_LIST *cheat)
{
	if (cheatsCurrentGet > cheatsNum) 
	{
		cheatsCurrentGet = 0;
		return FALSE;
	}
	memcpy(cheat, &cheats[cheatsCurrentGet++], sizeof(CHEATS_LIST));
	if (cheatsCurrentGet > cheatsNum) 
	{
		cheatsCurrentGet = 0;
		return FALSE;
	}
	return TRUE;
}

BOOL cheatsSave()
{
#ifdef WIN32
	char buf[20];
	FILE *file = fopen((char *)cheatFilename, "w");
	fclose(file);
	WritePrivateProfileString("General", "Name", "<game name>", (char *)cheatFilename);
	WritePrivateProfileInt("General", "NumberOfCheats", cheatsNum, (char *)cheatFilename);
	for (int i = 0; i < cheatsNum; i++)
	{
		wsprintf(buf, "Desc%04i", i);
		WritePrivateProfileString("Cheats", buf, cheats[i].description, (char *)cheatFilename);
		wsprintf(buf, "Type%04i", i);
		WritePrivateProfileInt("Cheats", buf, cheats[i].type, (char *)cheatFilename);
		wsprintf(buf, "Num_%04i", i);
		WritePrivateProfileInt("Cheats", buf, cheats[i].num, (char *)cheatFilename);
		wsprintf(buf, "Enab%04i", i);
		WritePrivateProfileInt("Cheats", buf, cheats[i].enabled, (char *)cheatFilename);
		wsprintf(buf, "Proc%04i", i);
		WritePrivateProfileInt("Cheats", buf, cheats[i].proc, (char *)cheatFilename);
		wsprintf(buf, "Size%04i", i);
		WritePrivateProfileInt("Cheats", buf, cheats[i].size, (char *)cheatFilename);
		for (int t = 0; t < cheats[i].num; t++)
		{
			char tmp_hex[6] = {0};

			wsprintf(buf, "H%03i%04i", i, t);
			wsprintf(tmp_hex, "%06X", cheats[i].hi[t]);
			WritePrivateProfileString("Cheats", buf, tmp_hex, (char *)cheatFilename);
			wsprintf(buf, "L%03i%04i", i, t);
			wsprintf(tmp_hex, "%06X", cheats[i].lo[t]);
			WritePrivateProfileString("Cheats", buf, tmp_hex, (char *)cheatFilename);
		}
	}
#endif
	return TRUE;
}

BOOL cheatsLoad()
{
#ifdef WIN32
	char buf[20];
	cheatsNum = GetPrivateProfileInt("General", "NumberOfCheats", 0, (char *)cheatFilename);
	if (cheatsNum > MAX_CHEAT_LIST) cheatsNum = MAX_CHEAT_LIST;
	for (int i = 0; i < cheatsNum; i++)
	{
		wsprintf(buf, "Desc%04i", i);
		GetPrivateProfileString("Cheats", buf, "", cheats[i].description, 75, (char *)cheatFilename);
		wsprintf(buf, "Type%04i", i);
		cheats[i].type = GetPrivateProfileInt("Cheats", buf, 0xFF, (char *)cheatFilename);
		wsprintf(buf, "Num_%04i", i);
		cheats[i].num = GetPrivateProfileInt("Cheats", buf, 0, (char *)cheatFilename);
		wsprintf(buf, "Enab%04i", i);
		cheats[i].enabled = GetPrivateProfileInt("Cheats", buf, 0, (char *)cheatFilename);
		wsprintf(buf, "Proc%04i", i);
		cheats[i].proc = GetPrivateProfileInt("Cheats", buf, 0, (char *)cheatFilename);
		wsprintf(buf, "Size%04i", i);
		cheats[i].size = GetPrivateProfileInt("Cheats", buf, 0, (char *)cheatFilename);
		for (int t = 0; t < cheats[i].num; t++)
		{
			char tmp_buf[10] = { 0 };
			wsprintf(buf, "H%03i%04i", i, t);
			GetPrivateProfileString("Cheats", buf, "0", tmp_buf, 10, (char *)cheatFilename);
			sscanf_s(tmp_buf, "%x", &cheats[i].hi[t]);
			wsprintf(buf, "L%03i%04i", i, t);
			GetPrivateProfileString("Cheats", buf, "0", tmp_buf, 10, (char *)cheatFilename);
			sscanf_s(tmp_buf, "%x", &cheats[i].lo[t]);
		}
	}
#endif
	return TRUE;
}

BOOL cheatsPush()
{
	if (cheatsStack) return FALSE;
	cheatsStack = new u8 [sizeof(cheats)];
	memcpy(cheatsStack, cheats, sizeof(cheats));
	cheatsNumStack = cheatsNum;
	return TRUE;
}

BOOL cheatsPop()
{
	if (!cheatsStack) return FALSE;
	memcpy(cheats, cheatsStack, sizeof(cheats));
	cheatsNum = cheatsNumStack;
	delete [] cheatsStack;
	cheatsStack = NULL;
	return TRUE;
}

void cheatsStackClear()
{
	if (!cheatsStack) return;
	delete [] cheatsStack;
	cheatsStack = NULL;
}

void cheatsProcess()
{
	if (!cheatsNum) return;
	for (int i = 0; i < cheatsNum; i++)
	{
		if (!cheats[i].enabled) continue;

		switch (cheats[i].type)
		{
			case 0:		// internal cheat system
				//INFO("cheat at 0x02|%06X value %i (size %i)\n",cheats[i].hi[0], cheats[i].lo[0], cheats[i].size);
				switch (cheats[i].size)
				{
					case 0: T1WriteByte(MMU.MMU_MEM[cheats[i].proc][0x20], cheats[i].hi[0], cheats[i].lo[0]); break;
					case 1: T1WriteWord(MMU.MMU_MEM[cheats[i].proc][0x20], cheats[i].hi[0], cheats[i].lo[0]); break;
					case 2:
						{
							u32 tmp = T1ReadLong(MMU.MMU_MEM[cheats[i].proc][0x20], cheats[i].hi[0]);
							tmp &= 0xFF000000;
							tmp |= (cheats[i].lo[0] & 0x00FFFFFF);
							T1WriteLong(MMU.MMU_MEM[cheats[i].proc][0x20], cheats[i].hi[0], tmp); 
							break;
						}
					case 3: T1WriteLong(MMU.MMU_MEM[cheats[i].proc][0x20], cheats[i].hi[0], cheats[i].lo[0]); break;
				}
			break;

			case 1:		// Action Replay
				break;
			case 2:		// Codebreaker
				break;
			default: continue;
		}
	}
}

// ========================================== search
u8					*searchStatMemARM9 = NULL;
u8					*searchStatMemARM7 = NULL;
u8					*searchMemARM9 = NULL;
u8					*searchMemARM7 = NULL;
u32					searchNumber = 0;
u32					searchLastRecord = 0;
u32					searchLastRecordProc = 0;

u32					searchType = 0;
u32					searchSize = 0;
u32					searchSign = 0;

void cheatsSearchInit(u8 type, u8 size, u8 sign)
{
	if (searchStatMemARM9) return;
	if (searchStatMemARM7) return;
	if (searchMemARM9) return;
	if (searchMemARM7) return;

	searchStatMemARM9 = new u8 [ ( 4 * 1024 * 1024 ) / 8 ];
	searchStatMemARM7 = new u8 [ ( 4 * 1024 * 1024 ) / 8 ];
	
	memset(searchStatMemARM9, 0xFF, ( 4 * 1024 * 1024 ) / 8);
	memset(searchStatMemARM7, 0xFF, ( 4 * 1024 * 1024 ) / 8);

	if (type == 1) // comparative search type (need 8Mb RAM !!! (4+4))
	{
		searchMemARM9 = new u8 [ ( 4 * 1024 * 1024 ) ];
		searchMemARM7 = new u8 [ ( 4 * 1024 * 1024 ) ];
		memcpy(searchMemARM9, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );
		memcpy(searchMemARM7, MMU.MMU_MEM[1][0x20], ( 4 * 1024 * 1024 ) );
	}

	searchType = type;
	searchSize = size;
	searchSign = sign;
	searchNumber = 0;
	searchLastRecord = 0;
	searchLastRecordProc = 0;
	
	//INFO("Cheat search system is inited (type %s)\n", type?"comparative":"exact");
}

void cheatsSearchClose()
{
	if (searchStatMemARM9) delete [] searchStatMemARM9;
	if (searchStatMemARM7) delete [] searchStatMemARM7;
	searchStatMemARM9 = NULL;
	searchStatMemARM7 = NULL;

	if (searchMemARM9) delete [] searchMemARM9;
	if (searchMemARM7) delete [] searchMemARM7;
	searchMemARM9 = NULL;
	searchMemARM7 = NULL;
	searchNumber = 0;
	searchLastRecord = 0;
	searchLastRecordProc = 0;
	//INFO("Cheat search system is closed\n");
}

u32 cheatsSearchValue(u32 val)
{
	u8		*stat[2] = {searchStatMemARM9, searchStatMemARM7};

	searchNumber = 0;

	switch (searchSize)
	{
		case 0:		// 1 byte
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i++)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (1<<offs))
					{
						if ( T1ReadByte(MMU.MMU_MEM[t][0x20], i) == val )
						{
							stat[t][addr] |= (1<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(1<<offs);
					}
				}
		break;

		case 1:		// 2 bytes
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (3<<offs))
					{
						if ( T1ReadWord(MMU.MMU_MEM[t][0x20], i) == val )
						{
							stat[t][addr] |= (3<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(3<<offs);
					}
				}
		break;

		case 2:		// 3 bytes
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (0x7<<offs))
					{
						if ( (T1ReadLong(MMU.MMU_MEM[t][0x20], i) & 0x00FFFFFF) == val )
						{
							stat[t][addr] |= (0x7<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(0x7<<offs);
					}
				}
		break;

		case 3:		// 4 bytes
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (0xF<<offs))
					{
						if ( T1ReadLong(MMU.MMU_MEM[t][0x20], i) == val )
						{
							stat[t][addr] |= (0xF<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(0xF<<offs);
					}
				}
		break;
	}

	return (searchNumber);
}

u32 cheatsSearchComp(u8 comp)
{
	u8		*mem[2] = {searchMemARM9, searchMemARM7};
	u8		*stat[2] = {searchStatMemARM9, searchStatMemARM7};
	BOOL	res = FALSE;

	searchNumber = 0;
	
	switch (searchSize)
	{
		case 0:		// 1 byte
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i++)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (1<<offs))
					{
						switch (comp)
						{
							case 0: res=(T1ReadByte(MMU.MMU_MEM[t][0x20], i) > T1ReadByte(mem[t], i)); break;
							case 1: res=(T1ReadByte(MMU.MMU_MEM[t][0x20], i) < T1ReadByte(mem[t], i)); break;
							case 2: res=(T1ReadByte(MMU.MMU_MEM[t][0x20], i) == T1ReadByte(mem[t], i)); break;
							case 3: res=(T1ReadByte(MMU.MMU_MEM[t][0x20], i) != T1ReadByte(mem[t], i)); break;
							default: res = FALSE; break;
						}
						if ( res )
						{
							stat[t][addr] |= (1<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(1<<offs);
					}
				}
		break;

		case 1:		// 2 bytes
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (3<<offs))
					{
						switch (comp)
						{
							case 0: res=(T1ReadWord(MMU.MMU_MEM[t][0x20], i) > T1ReadWord(mem[t], i)); break;
							case 1: res=(T1ReadWord(MMU.MMU_MEM[t][0x20], i) < T1ReadWord(mem[t], i)); break;
							case 2: res=(T1ReadWord(MMU.MMU_MEM[t][0x20], i) == T1ReadWord(mem[t], i)); break;
							case 3: res=(T1ReadWord(MMU.MMU_MEM[t][0x20], i) != T1ReadWord(mem[t], i)); break;
							default: res = FALSE; break;
						}
						if ( res )
						{
							stat[t][addr] |= (3<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(3<<offs);
					}
				}
		break;

		case 2:		// 3 bytes
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (7<<offs))
					{
						switch (comp)
						{
							case 0: res=((T1ReadLong(MMU.MMU_MEM[t][0x20], i) & 0x00FFFFFF) > (T1ReadLong(mem[t], i) & 0x00FFFFFF) ); break;
							case 1: res=((T1ReadLong(MMU.MMU_MEM[t][0x20], i) & 0x00FFFFFF) < (T1ReadLong(mem[t], i) & 0x00FFFFFF) ); break;
							case 2: res=((T1ReadLong(MMU.MMU_MEM[t][0x20], i) & 0x00FFFFFF) == (T1ReadLong(mem[t], i) & 0x00FFFFFF) ); break;
							case 3: res=((T1ReadLong(MMU.MMU_MEM[t][0x20], i) & 0x00FFFFFF) != (T1ReadLong(mem[t], i) & 0x00FFFFFF) ); break;
							default: res = FALSE; break;
						}
						if ( res )
						{
							stat[t][addr] |= (7<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(7<<offs);
					}
				}
		break;

		case 3:		// 4 bytes
			for (u32 t = 0; t < 2; t++)
				for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
				{
					u32	addr = (i >> 3);
					u32	offs = (i % 8);
					if (stat[t][addr] & (0xF<<offs))
					{
						switch (comp)
						{
							case 0: res=(T1ReadLong(MMU.MMU_MEM[t][0x20], i) > T1ReadLong(mem[t], i)); break;
							case 1: res=(T1ReadLong(MMU.MMU_MEM[t][0x20], i) < T1ReadLong(mem[t], i)); break;
							case 2: res=(T1ReadLong(MMU.MMU_MEM[t][0x20], i) == T1ReadLong(mem[t], i)); break;
							case 3: res=(T1ReadLong(MMU.MMU_MEM[t][0x20], i) != T1ReadLong(mem[t], i)); break;
							default: res = FALSE; break;
						}
						if ( res )
						{
							stat[t][addr] |= (0xF<<offs);
							searchNumber++;
							continue;
						}
						stat[t][addr] &= ~(0xF<<offs);
					}
				}
		break;
	}

	memcpy(searchMemARM9, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );
	memcpy(searchMemARM7, MMU.MMU_MEM[1][0x20], ( 4 * 1024 * 1024 ) );

	return (searchNumber);
}

u32 cheatSearchNumber()
{
	return (searchNumber);
}

BOOL cheatSearchGetList(u8 *proc, u32 *address, u32 *curVal)
{
	u8		*stat[2] = {searchStatMemARM9, searchStatMemARM7};

	for (u32 t = searchLastRecordProc; t < 2; t++)
	{
		for (u32 i = searchLastRecord; i < (4 * 1024 * 1024); i++)
		{
			u32	addr = (i >> 3);
			u32	offs = (i % 8);
			if (stat[t][addr] & (1<<offs))
			{
				*proc = t;
				*address = i;
				searchLastRecord = i+1;
				
				switch (searchSize)
				{
					case 0: *curVal=(u32)T1ReadByte(MMU.MMU_MEM[t][0x20], i); return TRUE;
					case 1: *curVal=(u32)T1ReadWord(MMU.MMU_MEM[t][0x20], i); return TRUE;
					case 2: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[t][0x20], i) & 0x00FFFFFF; return TRUE;
					case 3: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[t][0x20], i); return TRUE;
					default: return TRUE;
				}
			}
		}
		searchLastRecordProc = t;
	}
	searchLastRecord = 0;
	searchLastRecordProc = 0;
	return FALSE;
}

void cheatSearchGetListReset()
{
	searchLastRecord = 0;
	searchLastRecordProc = 0;
}
