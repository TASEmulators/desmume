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
#include "cheatSystem.h"
#include "NDSSystem.h"
#include "mem.h"
#include "MMU.h"
#include "debug.h"

static	CHEATS_LIST			cheats[MAX_CHEAT_LIST];
static	u16					cheatsNum = 0;
static	u8					cheatFilename[MAX_PATH];
static	u32					cheatsCurrentGet = 0;

static	u8					*cheatsStack = NULL;
static	u16					cheatsNumStack = 0;

static void cheatsClear()
{
	memset(cheats, 0, sizeof(cheats));
	for (int i = 0; i < MAX_CHEAT_LIST; i++)
		cheats[i].type = 0xFF;
	cheatsNum = 0;
	cheatsCurrentGet = 0;
}

void cheatsInit(char *path)
{
	cheatsClear();
	strcpy((char *)cheatFilename, path);

	if (cheatsStack) delete [] cheatsStack;
	cheatsStack = NULL;

	cheatsLoad();
}

BOOL cheatsAdd(u8 size, u32 address, u32 val, char *description, BOOL enabled)
{
	if (cheatsNum == MAX_CHEAT_LIST) return FALSE;
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

BOOL cheatsUpdate(u8 size, u32 address, u32 val, char *description, BOOL enabled, u32 pos)
{
	if (pos > cheatsNum) return FALSE;
	cheats[pos].hi[0] = address & 0x00FFFFFF;
	cheats[pos].lo[0] = val;
	cheats[pos].num = 1;
	cheats[pos].type = 0;
	cheats[pos].size = size;
	strcpy(cheats[pos].description, description);
	cheats[pos].enabled = enabled;
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
	if (cheatsNum == 0) return FALSE;

	for (int i = pos; i < cheatsNum; i++)
		memcpy(&cheats[i], &cheats[i+1], sizeof(CHEATS_LIST));

	memset(&cheats[cheatsNum], 0, sizeof(CHEATS_LIST));

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

BOOL cheatsGet(CHEATS_LIST *cheat, u32 pos)
{
	if (pos > cheatsNum) return FALSE;
	memcpy(cheat, &cheats[pos], sizeof(CHEATS_LIST));
	return TRUE;
}

u32	cheatsGetSize()
{
	return cheatsNum;
}

BOOL cheatsSave()
{
	FILE	*fcheat = fopen((char *)cheatFilename, "w");

	if (fcheat)
	{
		fprintf(fcheat, "; DeSmuME cheat file. VERSION %i.%03i\n", CHEAT_VERSION_MAJOR, CHEAT_VERSION_MINOR);
		fprintf(fcheat, "Name=%s\n", ROMserial);
		fputs("; cheats list\n", fcheat);
		for (int i = 0;  i < cheatsNum; i++)
		{
			fprintf(fcheat, "Desc=%s\n", cheats[i].description);
			fprintf(fcheat, "Info=%i,%i,%i,%i\n", cheats[i].type, cheats[i].num, cheats[i].enabled, cheats[i].size);

			if (cheats[i].num > 0)
			{
				for (int t = 0; t < cheats[i].num; t++)
				{
					fprintf(fcheat, "Data=%08lX%08lX", cheats[i].hi[t], cheats[i].lo[t]);
					if (t < (cheats[i].num - 1)) fputs(",", fcheat);
				}
				fputs("\n", fcheat);
			}
			fputs("\n", fcheat);
		}

		fclose(fcheat);
		return TRUE;
	}

	return FALSE;
}

BOOL cheatsLoad()
{
	FILE	*fcheat = fopen((char *)cheatFilename, "r");
	char	buf[1024];
	int		last=0;
	if (fcheat)
	{
		cheatsClear();
		memset(buf, 0, 1024);

		while (!feof(fcheat))
		{
			fgets(buf, 1024, fcheat);
			if (buf[0] == ';') continue;
			removeCR((char *)&buf);
			if (!strlen_ws(buf)) continue;
			if ( (buf[0] == 'D') &&
					(buf[1] == 'e') &&
					(buf[2] == 's') &&
					(buf[3] == 'c') &&
					(buf[4] == '=') )		// new cheat block
			{
				strncpy((char *)cheats[last].description, (char *)buf+5, strlen(buf)-5);
				fgets(buf, 1024, fcheat);
				if ( (buf[0] == 'I') &&
						(buf[1] == 'n') &&
						(buf[2] == 'f') &&
						(buf[3] == 'o') &&
						(buf[4] == '=') )		// Info cheat
				{
					// TODO: parse number for comma
					cheats[last].type=atoi(&buf[5]);
					if (buf[6]!=',') continue;			// error
					cheats[last].num=atoi(&buf[7]);
					if (buf[8]!=',') continue;			// error
					cheats[last].enabled=atoi(&buf[9]);
					if (buf[10]!=',') continue;			// error
					cheats[last].size=atoi(&buf[11]);
					fgets(buf, 1024, fcheat);
					if ( (buf[0] == 'D') &&
							(buf[1] == 'a') &&
							(buf[2] == 't') &&
							(buf[3] == 'a') &&
							(buf[4] == '=') )		// Data cheat
					{
						int		offs = 5;
						char	tmp_buf[9];
						memset(tmp_buf, 0, 9);

						for (int j=0; j<cheats[last].num; j++)
						{
							strncpy(tmp_buf, (char *)buf+offs, 8);
							sscanf_s(tmp_buf, "%lx", &cheats[last].hi[j]);
							offs+=8;
							strncpy(tmp_buf, (char *)buf+offs, 8);
							sscanf_s(tmp_buf, "%lx", &cheats[last].lo[j]);
							offs++;		// skip comma
						}
					}
					else
						continue;				// error
				}
				else 
					continue;				// error

				last++;
			} //		else parser error
		}
		fclose(fcheat);

		//INFO("Loaded %i cheats\n", last);
		cheatsNum = last;
		return TRUE;
	}
	return FALSE;
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
					case 0: T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], cheats[i].hi[0], cheats[i].lo[0]); break;
					case 1: T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], cheats[i].hi[0], cheats[i].lo[0]); break;
					case 2:
						{
							u32 tmp = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], cheats[i].hi[0]);
							tmp &= 0xFF000000;
							tmp |= (cheats[i].lo[0] & 0x00FFFFFF);
							T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], cheats[i].hi[0], tmp); 
							break;
						}
					case 3: T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], cheats[i].hi[0], cheats[i].lo[0]); break;
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
u8					*searchStatMem = NULL;
u8					*searchMem = NULL;
u32					searchNumber = 0;
u32					searchLastRecord = 0;

u32					searchType = 0;
u32					searchSize = 0;
u32					searchSign = 0;

void cheatsSearchInit(u8 type, u8 size, u8 sign)
{
	if (searchStatMem) return;
	if (searchMem) return;

	searchStatMem = new u8 [ ( 4 * 1024 * 1024 ) / 8 ];
	
	memset(searchStatMem, 0xFF, ( 4 * 1024 * 1024 ) / 8);

	if (type == 1) // comparative search type (need 8Mb RAM !!! (4+4))
	{
		searchMem = new u8 [ ( 4 * 1024 * 1024 ) ];
		memcpy(searchMem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );
	}

	searchType = type;
	searchSize = size;
	searchSign = sign;
	searchNumber = 0;
	searchLastRecord = 0;
	
	//INFO("Cheat search system is inited (type %s)\n", type?"comparative":"exact");
}

void cheatsSearchClose()
{
	if (searchStatMem) delete [] searchStatMem;
	searchStatMem = NULL;

	if (searchMem) delete [] searchMem;
	searchMem = NULL;
	searchNumber = 0;
	searchLastRecord = 0;
	//INFO("Cheat search system is closed\n");
}

u32 cheatsSearchValue(u32 val)
{
	searchNumber = 0;

	switch (searchSize)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (1<<offs))
				{
					if ( T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						searchStatMem[addr] |= (1<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (3<<offs))
				{
					if ( T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						searchStatMem[addr] |= (3<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (0x7<<offs))
				{
					if ( (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == val )
					{
						searchStatMem[addr] |= (0x7<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(0x7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (0xF<<offs))
				{
					if ( T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						searchStatMem[addr] |= (0xF<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	return (searchNumber);
}

u32 cheatsSearchComp(u8 comp)
{
	BOOL	res = FALSE;

	searchNumber = 0;
	
	switch (searchSize)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (1<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadByte(searchMem, i)); break;
						case 1: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadByte(searchMem, i)); break;
						case 2: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadByte(searchMem, i)); break;
						case 3: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadByte(searchMem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						searchStatMem[addr] |= (1<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (3<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadWord(searchMem, i)); break;
						case 1: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadWord(searchMem, i)); break;
						case 2: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadWord(searchMem, i)); break;
						case 3: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadWord(searchMem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						searchStatMem[addr] |= (3<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (7<<offs))
				{
					switch (comp)
					{
						case 0: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) > (T1ReadLong(searchMem, i) & 0x00FFFFFF) ); break;
						case 1: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) < (T1ReadLong(searchMem, i) & 0x00FFFFFF) ); break;
						case 2: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == (T1ReadLong(searchMem, i) & 0x00FFFFFF) ); break;
						case 3: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) != (T1ReadLong(searchMem, i) & 0x00FFFFFF) ); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						searchStatMem[addr] |= (7<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (searchStatMem[addr] & (0xF<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadLong(searchMem, i)); break;
						case 1: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadLong(searchMem, i)); break;
						case 2: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadLong(searchMem, i)); break;
						case 3: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadLong(searchMem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						searchStatMem[addr] |= (0xF<<offs);
						searchNumber++;
						continue;
					}
					searchStatMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	memcpy(searchMem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	return (searchNumber);
}

u32 cheatSearchNumber()
{
	return (searchNumber);
}

BOOL cheatSearchGetList(u32 *address, u32 *curVal)
{
	for (u32 i = searchLastRecord; i < (4 * 1024 * 1024); i++)
	{
		u32	addr = (i >> 3);
		u32	offs = (i % 8);
		if (searchStatMem[addr] & (1<<offs))
		{
			*address = i;
			searchLastRecord = i+1;
			
			switch (searchSize)
			{
				case 0: *curVal=(u32)T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				case 1: *curVal=(u32)T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				case 2: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF; return TRUE;
				case 3: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				default: return TRUE;
			}
		}
	}
	searchLastRecord = 0;
	return FALSE;
}

void cheatSearchGetListReset()
{
	searchLastRecord = 0;
}
