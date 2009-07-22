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

static void cheats_ARparser(CHEATS_LIST cheat)
{
	u8	type = 0;
	u8	subtype = 0;
	u32	hi = 0;
	u32	lo = 0;
	u32	addr = 0;
	u32	val = 0;
	// AR temporary vars & flags
	u32	offset = 0;
	u32	datareg = 0;
	u32	loopcount = 0;
	u32	counter = 0;
	u32	if_flag = 0;
	s32 loopbackline = 0;
	u32 loop_flag = 0;
	
	for (int i=0; i < cheat.num; i++)
	{
		type = cheat.hi[i] >> 28;
		subtype = (cheat.hi[i] >> 24) & 0x0F;

		hi = cheat.hi[i] & 0x0FFFFFFF;
		lo = cheat.lo[i];

		if (if_flag > 0) 
		{
			if ( (type == 0x0D) && (subtype == 0)) if_flag--;	// ENDIF
			if ( (type == 0x0D) && (subtype == 2))				// NEXT & Flush
			{
				if (loop_flag)
					i = (loopbackline-1);
				else
				{
					offset = 0;
					datareg = 0;
					loopcount = 0;
					counter = 0;
					if_flag = 0;
					loop_flag = 0;
				}
			}
			continue;
		}

		switch (type)
		{
			case 0x00:
			{
				if (hi==0)
				{
					//manual hook
				}
				else
				if ((hi==0x0000AA99) && (lo==0))
				{
					//parameter bytes 9..10 for above code (padded with 00s)
				}
				else
				{
					addr = hi + offset;
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20], lo);
				}
			}
			break;

			case 0x01:
				addr = hi + offset;
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20], lo & 0x0000FFFF);
			break;

			case 0x02:
				addr = hi + offset;
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20], lo & 0x000000FF);
			break;

			case 0x03:
				val = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]);
				if ( lo > val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x04:
				val = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]);
				if ( lo < val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x05:
				val = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]);
				if ( lo == val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x06:
				val = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]);
				if ( lo != val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x07:
				val = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]) & 0x0000FFFF;
				if ( (lo & 0xFFFF) > ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x08:
				val = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]) & 0x0000FFFF;
				if ( (lo & 0xFFFF) < ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x09:
				val = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]);
				if ( (lo & 0xFFFF) == ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x0A:
				val = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][hi>>20], hi & MMU.MMU_MASK[ARMCPU_ARM9][hi>>20]) & 0x0000FFFF;
				if ( (lo & 0xFFFF) != ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x0B:
				addr = hi + offset;
				offset = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20]);
			break;

			case 0x0C:
				switch (subtype)
				{
					case 0x0:
						if (loopcount < (lo+1))
							loop_flag = 1;
						else
							loop_flag = 0;
						loopcount++;
						loopbackline = i;
					break;

					case 0x4:
					break;

					case 0x5:
						counter++;
						if ( (counter & (lo & 0xFFFF)) == ((lo >> 8) & 0xFFFF) )
						{
							if (if_flag > 0) if_flag--;
						}
						else
						{
							if_flag++;
						}
					break;

					case 0x6:
						T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][lo>>20], lo & MMU.MMU_MASK[ARMCPU_ARM9][lo>>20], offset);
					break;
				}
			break;

			case 0x0D:
			{
				switch (subtype)
				{
					case 0x0:
					break;

					case 0x1:
						if (loop_flag)
							i = (loopbackline-1);
					break;

					case 0x2:
						if (loop_flag)
							i = (loopbackline-1);
						else
						{
							offset = 0;
							datareg = 0;
							loopcount = 0;
							counter = 0;
							if_flag = 0;
							loop_flag = 0;
						}
					break;

					case 0x3:
						offset = lo;
					break;

					case 0x4:
						datareg += lo;
					break;

					case 0x5:
						datareg = lo;
					break;

					case 0x6:
						addr = lo + offset;
						T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20], datareg);
						offset += 4;
					break;

					case 0x7:
						addr = lo + offset;
						T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20], datareg & 0x0000FFFF);
						offset += 2;
					break;

					case 0x8:
						addr = lo + offset;
						T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20], datareg & 0x000000FF);
						offset += 1;
					break;

					case 0x9:
						addr = lo + offset;
						datareg = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20]);
					break;

					case 0xA:
						addr = lo + offset;
						datareg = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20]) & 0x0000FFFF;
					break;

					case 0xB:
						addr = lo + offset;
						datareg = T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][addr>>20], addr & MMU.MMU_MASK[ARMCPU_ARM9][addr>>20]) & 0x000000FF;
					break;

					case 0xC:
						offset += lo;
					break;
				}
			}
			break;

			case 0xE:
				for (u32 t = 0; t < lo; t++)
				{
					u8 tmp = T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][(hi+t)>>20], (hi+t) & MMU.MMU_MASK[ARMCPU_ARM9][(hi+t)>>20]);
					T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][(offset+t)>>20], (offset+t) & MMU.MMU_MASK[ARMCPU_ARM9][(offset+t)>>20], tmp);
				}
			break;

			case 0xF:
				for (u32 t = 0; t < lo; t++)
				{
					u8 tmp = T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][(offset+t)>>20], (offset+t) & MMU.MMU_MASK[ARMCPU_ARM9][(offset+t)>>20]);
					T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][(hi+t)>>20], (hi+t) & MMU.MMU_MASK[ARMCPU_ARM9][(hi+t)>>20], tmp);
				}
			break;
			//default: INFO("AR: ERROR uknown command 0x%2X at %08X:%08X\n", type, hi, lo); break;
		}
	}
}

static BOOL cheatsXXcodePreparser(CHEATS_LIST *cheat, char *code)
{
	u16		count = 0;
	u16		t = 0;
	char	tmp_buf[sizeof(cheat->hi)+sizeof(cheat->lo)];

	memset(tmp_buf, 0, sizeof(tmp_buf));
	// remove wrong chars
	for (unsigned int i=0; i < strlen(code); i++)
	{
		switch (code[i])
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case 'A':
			case 'a':
			case 'B':
			case 'b':
			case 'C':
			case 'c':
			case 'D':
			case 'd':
			case 'E':
			case 'e':
			case 'F':
			case 'f':
				tmp_buf[t] = code[i];
				t++;
			break;
		}
	}

	if ((strlen(tmp_buf) % 8) != 0) return FALSE;			// error
	if ((strlen(tmp_buf) % 16) != 0) return FALSE;			// error

	// TODO: syntax check
	count = (strlen(tmp_buf) / 16);
	for (int i=0; i < count; i++)
	{
		char buf[8] = {0};
		strncpy(buf, tmp_buf+(i*16), 8);
		sscanf_s(buf, "%x", &cheat->hi[i]);
		strncpy(buf, tmp_buf+(i*16) + 8, 8);
		sscanf_s(buf, "%x", &cheat->lo[i]);
	}
	
	cheat->num = count;
	cheat->size = 0;
	return TRUE;
}

BOOL cheatsAdd_AR(char *code, char *description, BOOL enabled)
{
	if (cheatsNum == MAX_CHEAT_LIST) return FALSE;

	if (!cheatsXXcodePreparser(&cheats[cheatsNum], code)) return FALSE;
	
	cheats[cheatsNum].type = 1;
	
	strcpy(cheats[cheatsNum].description, description);
	cheats[cheatsNum].enabled = enabled;
	cheatsNum++;
	return TRUE;
}

BOOL cheatsUpdate_AR(char *code, char *description, BOOL enabled, u32 pos)
{
	if (pos > cheatsNum) return FALSE;

	if (code != NULL)
	{
		if (!cheatsXXcodePreparser(&cheats[pos], code)) return FALSE;
		strcpy(cheats[pos].description, description);
		cheats[pos].type = 1;
	}
	
	cheats[pos].enabled = enabled;
	return TRUE;
}

BOOL cheatsAdd_CB(char *code, char *description, BOOL enabled)
{
	if (cheatsNum == MAX_CHEAT_LIST) return FALSE;

	if (!cheatsXXcodePreparser(&cheats[cheatsNum], code)) return FALSE;
	
	cheats[cheatsNum].type = 2;
	
	strcpy(cheats[cheatsNum].description, description);
	cheats[cheatsNum].enabled = enabled;
	cheatsNum++;
	return TRUE;
}

BOOL cheatsUpdate_CB(char *code, char *description, BOOL enabled, u32 pos)
{
	if (pos > cheatsNum) return FALSE;

	if (code != NULL)
	{
		if (!cheatsXXcodePreparser(&cheats[pos], code)) return FALSE;
		cheats[pos].type = 2;
		strcpy(cheats[pos].description, description);
	}
	cheats[pos].enabled = enabled;
	return TRUE;
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
		fprintf(fcheat, "Name=%s\n", gameInfo.ROMserial);
		fputs("; cheats list\n", fcheat);
		for (int i = 0;  i < cheatsNum; i++)
		{
			fprintf(fcheat, "Desc=%s\n", cheats[i].description);
			fprintf(fcheat, "Info=%i,%i,%i,%i\n", cheats[i].type, cheats[i].num, cheats[i].enabled, cheats[i].size);

			if (cheats[i].num > 0)
			{
				fprintf(fcheat, "Data=");
				for (int t = 0; t < cheats[i].num; t++)
				{
					fprintf(fcheat, "%08X%08X", cheats[i].hi[t], cheats[i].lo[t]);
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
					u32		dstart = 5;
					u32		dsize = 0;
					char	bf[4] = { 0 };

					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					if (buf[dstart+dsize]==0) continue;			// error
					strncpy(bf, (char*)buf+dstart, dsize);
					cheats[last].type=atoi(bf);
					dstart += (dsize+1); dsize=0;
					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					if (buf[dstart+dsize]==0) continue;			// error
					strncpy(bf, (char*)buf+dstart, dsize);
					dstart += (dsize+1); dsize=0;
					cheats[last].num=atoi(bf);
					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					if (buf[dstart+dsize]==0) continue;			// error
					strncpy(bf, (char*)(buf+dstart), dsize);
					dstart += (dsize+1); dsize=0;
					cheats[last].enabled=atoi(bf);
					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					strncpy(bf, (char*)buf+dstart, dsize);
					cheats[last].size=atoi(bf);
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
							sscanf_s(tmp_buf, "%x", &cheats[last].hi[j]);
							offs+=8;
							strncpy(tmp_buf, (char *)buf+offs, 8);
							sscanf_s(tmp_buf, "%x", &cheats[last].lo[j]);
							offs+=8;
							if (buf[offs] != ',') continue; //error
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
				cheats_ARparser(cheats[i]);
				break;
			case 2:		// Codebreaker
				break;
			default: continue;
		}
	}
}

void cheatGetXXcodeString(CHEATS_LIST cheat, char *res_buf)
{
	char	buf[50] = { 0 };

	for (int i=0; i < cheat.num; i++)
	{
		sprintf(buf, "%08X %08X\n", cheat.hi[i], cheat.lo[i]);
		strcat(res_buf, buf);
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
	u8	step = (searchSize+1);
	u8	stepMem = 1;
	switch (searchSize)
	{
		case 1: stepMem = 0x3; break;
		case 2: stepMem = 0x7; break;
		case 3: stepMem = 0xF; break;
	}

	for (u32 i = searchLastRecord; i < (4 * 1024 * 1024); i+=step)
	{
		u32	addr = (i >> 3);
		u32	offs = (i % 8);
		if (searchStatMem[addr] & (stepMem<<offs))
		{
			*address = i;
			searchLastRecord = i+step;
			
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
