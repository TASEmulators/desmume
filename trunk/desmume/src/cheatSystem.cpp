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

#include <string.h>
#include "cheatSystem.h"
#include "NDSSystem.h"
#include "mem.h"
#include "MMU.h"
#include "debug.h"
#include "utils/xstring.h"

#ifndef _MSC_VER 
#include <stdint.h>
#endif

CHEATS *cheats = NULL;
CHEATSEARCH *cheatSearch = NULL;

void CHEATS::clear()
{
	list.resize(0);
	currentGet = 0;
}

void CHEATS::init(char *path)
{
	clear();
	strcpy((char *)filename, path);

	load();
}

BOOL CHEATS::add(u8 size, u32 address, u32 val, char *description, BOOL enabled)
{
	size_t num = list.size();
	list.push_back(CHEATS_LIST());
	list[num].code[0][0] = address & 0x00FFFFFF;
	list[num].code[0][1] = val;
	list[num].num = 1;
	list[num].type = 0;
	list[num].size = size;
	this->setDescription(description, num);
	list[num].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::update(u8 size, u32 address, u32 val, char *description, BOOL enabled, u32 pos)
{
	if (pos >= list.size()) return FALSE;
	list[pos].code[0][0] = address & 0x00FFFFFF;
	list[pos].code[0][1] = val;
	list[pos].num = 1;
	list[pos].type = 0;
	list[pos].size = size;
	this->setDescription(description, pos);
	list[pos].enabled = enabled;
	return TRUE;
}

void CHEATS::ARparser(CHEATS_LIST& list)
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
	
	for (int i=0; i < list.num; i++)
	{
		type = list.code[i][0] >> 28;
		subtype = (list.code[i][0] >> 24) & 0x0F;

		hi = list.code[i][0] & 0x0FFFFFFF;
		lo = list.code[i][1];

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
				if ((hi==0x0000AA99) && (lo==0))	// 0000AA99 00000000   parameter bytes 9..10 for above code (padded with 00s)
				{
					//parameter bytes 9..10 for above code (padded with 00s)
				}
				else	// 0XXXXXXX YYYYYYYY   word[XXXXXXX+offset] = YYYYYYYY
				{
					addr = hi + offset;
					_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, lo);
				}
			}
			break;

			case 0x01:	// 1XXXXXXX 0000YYYY   half[XXXXXXX+offset] = YYYY
				addr = hi + offset;
				_MMU_write16<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, lo);
			break;

			case 0x02:	// 2XXXXXXX 000000YY   byte[XXXXXXX+offset] = YY
				addr = hi + offset;
				_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, lo);
			break;

			case 0x03:	// 3XXXXXXX YYYYYYYY   IF YYYYYYYY > word[XXXXXXX]   ;unsigned
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( lo > val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x04:	// 4XXXXXXX YYYYYYYY   IF YYYYYYYY < word[XXXXXXX]   ;unsigned
				if ((hi == 0x04332211) && (lo == 88776655))	//44332211 88776655   parameter bytes 1..8 for above code  (example)
				{
					break;
				}
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( lo < val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x05:	// 5XXXXXXX YYYYYYYY   IF YYYYYYYY = word[XXXXXXX]
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( lo == val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x06:	// 6XXXXXXX YYYYYYYY   IF YYYYYYYY <> word[XXXXXXX]
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( lo != val )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x07:	// 7XXXXXXX ZZZZYYYY   IF YYYY > ((not ZZZZ) AND half[XXXXXXX])
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read16<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( (lo & 0xFFFF) > ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x08:	// 8XXXXXXX ZZZZYYYY   IF YYYY < ((not ZZZZ) AND half[XXXXXXX])
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read16<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( (lo & 0xFFFF) < ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x09:	// 9XXXXXXX ZZZZYYYY   IF YYYY = ((not ZZZZ) AND half[XXXXXXX])
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read16<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( (lo & 0xFFFF) == ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x0A:	// AXXXXXXX ZZZZYYYY   IF YYYY <> ((not ZZZZ) AND half[XXXXXXX])
				if (hi == 0) hi = offset;	// V1.54+
				val = _MMU_read16<ARMCPU_ARM9,MMU_AT_DEBUG>(hi);
				if ( (lo & 0xFFFF) != ( (~(lo >> 16)) & val) )
				{
					if (if_flag > 0) if_flag--;
				}
				else
				{
					if_flag++;
				}
			break;

			case 0x0B:	// BXXXXXXX 00000000   offset = word[XXXXXXX+offset]
				addr = hi + offset;
				offset = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr);;
			break;

			case 0x0C:
				switch (subtype)
				{
					case 0x0:	// C0000000 YYYYYYYY   FOR loopcount=0 to YYYYYYYY  ;execute Y+1 times
						if (loopcount < (lo+1))
							loop_flag = 1;
						else
							loop_flag = 0;
						loopcount++;
						loopbackline = i;
					break;

					case 0x4:	// C4000000 00000000   offset = address of the C4000000 code ; V1.54
						printf("AR: untested code C4\n");
					break;

					case 0x5:	// C5000000 XXXXYYYY   counter=counter+1, IF (counter AND YYYY) = XXXX ; V1.54
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

					case 0x6:	// C6000000 XXXXXXXX   [XXXXXXXX]=offset ; V1.54
						_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(lo, offset);
					break;
				}
			break;

			case 0x0D:
			{
				switch (subtype)
				{
					case 0x0:	// D0000000 00000000   ENDIF
					break;

					case 0x1:	// D1000000 00000000   NEXT loopcount
						if (loop_flag)
							i = (loopbackline-1);
					break;

					case 0x2:	// D2000000 00000000   NEXT loopcount, and then FLUSH everything
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

					case 0x3:	// D3000000 XXXXXXXX   offset = XXXXXXXX
						offset = lo;
					break;

					case 0x4:	// D4000000 XXXXXXXX   datareg = datareg + XXXXXXXX
						datareg += lo;
					break;

					case 0x5:	// D5000000 XXXXXXXX   datareg = XXXXXXXX
						datareg = lo;
					break;

					case 0x6:	// D6000000 XXXXXXXX   word[XXXXXXXX+offset]=datareg, offset=offset+4
						addr = lo + offset;
						_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, datareg);
						offset += 4;
					break;

					case 0x7:	// D7000000 XXXXXXXX   half[XXXXXXXX+offset]=datareg, offset=offset+2
						addr = lo + offset;
						_MMU_write16<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, datareg);
						offset += 2;
					break;

					case 0x8:	// D8000000 XXXXXXXX   byte[XXXXXXXX+offset]=datareg, offset=offset+1
						addr = lo + offset;
						_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, datareg);
						offset += 1;
					break;

					case 0x9:	// D9000000 XXXXXXXX   datareg = word[XXXXXXXX+offset]
						addr = lo + offset;
						datareg = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr);
					break;

					case 0xA:	// DA000000 XXXXXXXX   datareg = half[XXXXXXXX+offset]
						addr = lo + offset;
						datareg = _MMU_read16<ARMCPU_ARM9,MMU_AT_DEBUG>(addr);
					break;

					case 0xB:	// DB000000 XXXXXXXX   datareg = byte[XXXXXXXX+offset] ;bugged on pre-v1.54
						addr = lo + offset;
						datareg = _MMU_read08<ARMCPU_ARM9,MMU_AT_DEBUG>(addr);
					break;

					case 0xC:	// DC000000 XXXXXXXX   offset = offset + XXXXXXXX
						offset += lo;
					break;
				}
			}
			break;

			case 0xE:		// EXXXXXXX YYYYYYYY   Copy YYYYYYYY parameter bytes to [XXXXXXXX+offset...]
			{
				u8	*tmp_code = (u8*)(list.code[i+1]);
				u32 addr = hi+offset;
				u32 maxByteReadLocation = ((2 * 4) * (MAX_XX_CODE - i - 1)) - 1; // 2 = 2 array dimensions, 4 = 4 bytes per array element
				
				if (lo <= maxByteReadLocation)
				{
					for (u32 t = 0; t < lo; t++)
					{
						u8	tmp = tmp_code[t];
						_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(addr, tmp);
						addr++;
					}
				}
				
				i += (lo / 8);
			}
			break;

			case 0xF:		// FXXXXXXX YYYYYYYY   Copy YYYYYYYY bytes from [offset..] to [XXXXXXX...]
				for (u32 t = 0; t < lo; t++)
				{
					u8 tmp = _MMU_read08<ARMCPU_ARM9,MMU_AT_DEBUG>(offset+t);
					_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(hi+t, tmp);
				}
			break;
			default: PROGINFO("AR: ERROR unknown command 0x%2X at %08X:%08X\n", type, hi, lo); break;
		}
	}
}

BOOL CHEATS::add_AR_Direct(CHEATS_LIST cheat)
{
	size_t num = list.size();
	list.push_back(cheat);
	list[num].type = 1;
	return TRUE;
}

BOOL CHEATS::add_AR(char *code, char *description, BOOL enabled)
{
	//if (num == MAX_CHEAT_LIST) return FALSE;
	size_t num = list.size();

	CHEATS_LIST temp;
	if (!CHEATS::XXCodeFromString(&temp, code)) return FALSE;

	list.push_back(temp);
	
	list[num].type = 1;
	
	this->setDescription(description, num);
	list[num].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::update_AR(char *code, char *description, BOOL enabled, u32 pos)
{
	if (pos >= list.size()) return FALSE;

	if (code != NULL)
	{
		if (!CHEATS::XXCodeFromString(this->getItemByIndex(pos), code)) return FALSE;
		this->setDescription(description, pos);
		list[pos].type = 1;
	}
	
	list[pos].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::add_CB(char *code, char *description, BOOL enabled)
{
	//if (num == MAX_CHEAT_LIST) return FALSE;
	size_t num = list.size();

	if (!CHEATS::XXCodeFromString(this->getItemByIndex(num), code)) return FALSE;
	
	list[num].type = 2;
	
	this->setDescription(description, num);
	list[num].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::update_CB(char *code, char *description, BOOL enabled, u32 pos)
{
	if (pos >= list.size()) return FALSE;

	if (code != NULL)
	{
		if (!CHEATS::XXCodeFromString(this->getItemByIndex(pos), code)) return FALSE;
		list[pos].type = 2;
		this->setDescription(description, pos);
	}
	list[pos].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::remove(u32 pos)
{
	if (pos >= list.size()) return FALSE;
	if (list.size() == 0) return FALSE;

	list.erase(list.begin()+pos);

	return TRUE;
}

void CHEATS::getListReset()
{
	currentGet = 0;
	return;
}

BOOL CHEATS::getList(CHEATS_LIST *cheat)
{
	BOOL result = FALSE;
	
	if (currentGet >= this->list.size()) 
	{
		this->getListReset();
		return result;
	}
	
	result = this->get(cheat, currentGet++);
	
	return result;
}

CHEATS_LIST* CHEATS::getListPtr()
{
	return &this->list[0];
}

BOOL CHEATS::get(CHEATS_LIST *cheat, u32 pos)
{
	CHEATS_LIST *item = this->getItemByIndex(pos);
	if (item == NULL)
	{
		return FALSE;
	}
	
	*cheat = *item;
	
	return TRUE;
}

CHEATS_LIST* CHEATS::getItemByIndex(const u32 pos)
{
	if (pos >= this->getSize())
	{
		return NULL;
	}
	
	return &this->list[pos];
}

u32	CHEATS::getSize()
{
	return list.size();
}

size_t CHEATS::getActiveCount()
{
	size_t activeCheatCount = 0;
	const size_t cheatListCount = this->getSize();
	
	for (size_t i = 0; i < cheatListCount; i++)
	{
		if (list[i].enabled)
		{
			activeCheatCount++;
		}
	}
	
	return activeCheatCount;
}

void CHEATS::setDescription(const char *description, u32 pos)
{
	strncpy(list[pos].description, description, sizeof(list[pos].description));
	list[pos].description[sizeof(list[pos].description) - 1] = '\0';
}

BOOL CHEATS::save()
{
	const char	*types[] = {"DS", "AR", "CB"};
	std::string	cheatLineStr = "";
	FILE		*flist = fopen((char *)filename, "w");

	if (flist)
	{
		fprintf(flist, "; DeSmuME cheats file. VERSION %i.%03i\n", CHEAT_VERSION_MAJOR, CHEAT_VERSION_MINOR);
		fprintf(flist, "Name=%s\n", gameInfo.ROMname);
		fprintf(flist, "Serial=%s\n", gameInfo.ROMserial);
		fputs("\n; cheats list\n", flist);
		for (size_t i = 0;  i < list.size(); i++)
		{
			if (list[i].num == 0) continue;
			
			char buf1[8] = {0};
			sprintf(buf1, "%s %c ", types[list[i].type], list[i].enabled?'1':'0');
			cheatLineStr = buf1;
			
			for (int t = 0; t < list[i].num; t++)
			{
				char buf2[10] = { 0 };

				u32 adr = list[i].code[t][0];
				if (list[i].type == 0)
				{
					//size of the cheat is written out as adr highest nybble
					adr &= 0x0FFFFFFF;
					adr |= (list[i].size << 28);
				}
				sprintf(buf2, "%08X", adr);
				cheatLineStr += buf2;
				
				sprintf(buf2, "%08X", list[i].code[t][1]);
				cheatLineStr += buf2;
				if (t < (list[i].num - 1))
					cheatLineStr += ",";
			}
			
			cheatLineStr += " ;";
			cheatLineStr += trim(list[i].description);
			fprintf(flist, "%s\n", cheatLineStr.c_str());
		}
		fputs("\n", flist);
		fclose(flist);
		return TRUE;
	}

	return FALSE;
}

char *CHEATS::clearCode(char *s)
{
	char	*buf = s;
	if (!s) return NULL;
	if (!*s) return s;

	for (u32 i = 0; i < strlen(s); i++)
	{
		if (s[i] == ';') break;
		if (strchr(hexValid, s[i]))
		{
			*buf = s[i];
			buf++;
		}
	}
	*buf = 0;
	return s;
}

BOOL CHEATS::load()
{
	FILE *flist = fopen((char *)filename, "r");
	if (flist == NULL)
	{
		return FALSE;
	}
	
	size_t readSize = (MAX_XX_CODE * 17) + sizeof(list[0].description) + 7;
	if (readSize < CHEAT_FILE_MIN_FGETS_BUFFER)
	{
		readSize = CHEAT_FILE_MIN_FGETS_BUFFER;
	}
	
	char *buf = (char *)malloc(readSize);
	if (buf == NULL)
	{
		fclose(flist);
		return FALSE;
	}
	
	readSize *= sizeof(*buf);
	
	std::string		codeStr = "";
	u32				last = 0;
	u32				line = 0;
	
	INFO("Load cheats: %s\n", filename);
	clear();
	last = 0; line = 0;
	while (!feof(flist))
	{
		CHEATS_LIST		tmp_cht;
		line++;				// only for debug
		memset(buf, 0, readSize);
		if (fgets(buf, readSize, flist) == NULL) {
			//INFO("Cheats: Failed to read from flist at line %i\n", line);
			continue;
		}
		trim(buf);
		if ((buf[0] == 0) || (buf[0] == ';')) continue;
		if(!strncasecmp(buf,"name=",5)) continue;
		if(!strncasecmp(buf,"serial=",7)) continue;

		memset(&tmp_cht, 0, sizeof(tmp_cht));
		if ((buf[0] == 'D') && (buf[1] == 'S'))		// internal
			tmp_cht.type = 0;
		else
			if ((buf[0] == 'A') && (buf[1] == 'R'))	// Action Replay
				tmp_cht.type = 1;
			else
				if ((buf[0] == 'B') && (buf[1] == 'S'))	// Codebreaker
					tmp_cht.type = 2;
				else
					continue;
		// TODO: CB not supported
		if (tmp_cht.type == 3)
		{
			INFO("Cheats: Codebreaker code no supported at line %i\n", line);
			continue;
		}
		
		codeStr = (char *)(buf + 5);
		codeStr = clearCode((char *)codeStr.c_str());
		
		if (codeStr.empty() || (codeStr.length() % 16 != 0))
		{
			INFO("Cheats: Syntax error at line %i\n", line);
			continue;
		}

		tmp_cht.enabled = (buf[3] == '0')?FALSE:TRUE;
		u32 descr_pos = (u32)(std::max<s32>(strchr((char*)buf, ';') - buf, 0));
		if (descr_pos != 0)
		{
			strncpy(tmp_cht.description, (buf + descr_pos + 1), sizeof(tmp_cht.description));
			tmp_cht.description[sizeof(tmp_cht.description) - 1] = '\0';
		}

		tmp_cht.num = codeStr.length() / 16;
		if ((tmp_cht.type == 0) && (tmp_cht.num > 1))
		{
			INFO("Cheats: Too many values for internal cheat\n", line);
			continue;
		}
		for (int i = 0; i < tmp_cht.num; i++)
		{
			char tmp_buf[9] = {0};

			strncpy(tmp_buf, &codeStr[i * 16], 8);
			sscanf_s(tmp_buf, "%x", &tmp_cht.code[i][0]);

			if (tmp_cht.type == 0)
			{
				tmp_cht.size = std::min<u32>(3, ((tmp_cht.code[i][0] & 0xF0000000) >> 28));
				tmp_cht.code[i][0] &= 0x00FFFFFF;
			}
			
			strncpy(tmp_buf, &codeStr[(i * 16) + 8], 8);
			sscanf_s(tmp_buf, "%x", &tmp_cht.code[i][1]);
		}

		list.push_back(tmp_cht);
		last++;
	}
	
	free(buf);
	buf = NULL;

	fclose(flist);
	INFO("Added %i cheat codes\n", list.size());
	
	return TRUE;
}

void CHEATS::process()
{
	if (CommonSettings.cheatsDisable) return;
	if (list.size() == 0) return;
	size_t num = list.size();
	for (size_t i = 0; i < num; i++)
	{
		if (!list[i].enabled) continue;

		switch (list[i].type)
		{
			case 0:		// internal cheat system
			{
				//INFO("list at 0x02|%06X value %i (size %i)\n",list[i].code[0], list[i].lo[0], list[i].size);
				u32 addr = list[i].code[0][0] | 0x02000000;
				u32 val = list[i].code[0][1];
				switch (list[i].size)
				{
				case 0: 
					_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,val);
					break;
				case 1: 
					_MMU_write16<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,val);
					break;
				case 2:
					{
						u32 tmp = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr);
						tmp &= 0xFF000000;
						tmp |= (val & 0x00FFFFFF);
						_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,tmp);
						break;
					}
				case 3: 
					_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,val);
					break;
				}
				break;
			} //end case 0 internal cheat system

			case 1:		// Action Replay
				ARparser(list[i]);
				break;
			case 2:		// Codebreaker
				break;
			default: continue;
		}
	}
}

void CHEATS::getXXcodeString(CHEATS_LIST list, char *res_buf)
{
	char	buf[50] = { 0 };

	for (int i=0; i < list.num; i++)
	{
		sprintf(buf, "%08X %08X\n", list.code[i][0], list.code[i][1]);
		strcat(res_buf, buf);
	}
}

BOOL CHEATS::XXCodeFromString(CHEATS_LIST *cheatItem, const std::string codeString)
{
	return CHEATS::XXCodeFromString(cheatItem, codeString.c_str());
}

BOOL CHEATS::XXCodeFromString(CHEATS_LIST *cheatItem, const char *codeString)
{
	BOOL result = FALSE;
	
	if (cheatItem == NULL || codeString == NULL)
	{
		return result;
	}
	
	int		count = 0;
	u16		t = 0;
	char	tmp_buf[sizeof(cheatItem->code) * 2 + 1];
	memset(tmp_buf, 0, sizeof(tmp_buf));
	
	size_t code_len = strlen(codeString);
	// remove wrong chars
	for (size_t i=0; i < code_len; i++)
	{
		char c = codeString[i];
		//apparently 100% of pokemon codes were typed with the letter O in place of zero in some places
		//so let's try to adjust for that here
		static const char *AR_Valid = "Oo0123456789ABCDEFabcdef";
		if (strchr(AR_Valid, c))
		{
			if(c=='o' || c=='O') c='0';
			tmp_buf[t++] = c;
		}
	}
	
	size_t len = strlen(tmp_buf);
	if ((len % 16) != 0) return result;			// error
	
	// TODO: syntax check
	count = (len / 16);
	for (int i=0; i < count; i++)
	{
		char buf[9] = {0};
		memcpy(buf, tmp_buf+(i*16), 8);
		sscanf(buf, "%x", &cheatItem->code[i][0]);
		memcpy(buf, tmp_buf+(i*16) + 8, 8);
		sscanf(buf, "%x", &cheatItem->code[i][1]);
	}
	
	cheatItem->num = count;
	cheatItem->size = 0;
	
	result = TRUE;
	
	return result;
}

// ========================================== search
BOOL CHEATSEARCH::start(u8 type, u8 size, u8 sign)
{
	if (statMem) return FALSE;
	if (mem) return FALSE;

	statMem = new u8 [ ( 4 * 1024 * 1024 ) / 8 ];
	memset(statMem, 0xFF, ( 4 * 1024 * 1024 ) / 8);

	// comparative search type (need 8Mb RAM !!! (4+4))
	mem = new u8 [ ( 4 * 1024 * 1024 ) ];
	memcpy(mem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	_type = type;
	_size = size;
	_sign = sign;
	amount = 0;
	lastRecord = 0;
	
	//INFO("Cheat search system is inited (type %s)\n", type?"comparative":"exact");
	return TRUE;
}

BOOL CHEATSEARCH::close()
{
	if (statMem)
	{
		delete [] statMem;
		statMem = NULL;
	}

	if (mem)
	{
		delete [] mem;
		mem = NULL;
	}
	amount = 0;
	lastRecord = 0;
	//INFO("Cheat search system is closed\n");
	return FALSE;
}

u32 CHEATSEARCH::search(u32 val)
{
	amount = 0;

	switch (_size)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (1<<offs))
				{
					if ( T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						statMem[addr] |= (1<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (3<<offs))
				{
					if ( T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						statMem[addr] |= (3<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (0x7<<offs))
				{
					if ( (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == val )
					{
						statMem[addr] |= (0x7<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(0x7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (0xF<<offs))
				{
					if ( T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						statMem[addr] |= (0xF<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	return (amount);
}

u32 CHEATSEARCH::search(u8 comp)
{
	BOOL	res = FALSE;

	amount = 0;
	
	switch (_size)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (1<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadByte(mem, i)); break;
						case 1: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadByte(mem, i)); break;
						case 2: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadByte(mem, i)); break;
						case 3: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadByte(mem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (1<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (3<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadWord(mem, i)); break;
						case 1: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadWord(mem, i)); break;
						case 2: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadWord(mem, i)); break;
						case 3: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadWord(mem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (3<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (7<<offs))
				{
					switch (comp)
					{
						case 0: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) > (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						case 1: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) < (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						case 2: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						case 3: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) != (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (7<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (0xF<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadLong(mem, i)); break;
						case 1: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadLong(mem, i)); break;
						case 2: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadLong(mem, i)); break;
						case 3: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadLong(mem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (0xF<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	memcpy(mem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	return (amount);
}

u32 CHEATSEARCH::getAmount()
{
	return (amount);
}

BOOL CHEATSEARCH::getList(u32 *address, u32 *curVal)
{
	u8	step = (_size+1);
	u8	stepMem = 1;
	switch (_size)
	{
		case 1: stepMem = 0x3; break;
		case 2: stepMem = 0x7; break;
		case 3: stepMem = 0xF; break;
	}

	for (u32 i = lastRecord; i < (4 * 1024 * 1024); i+=step)
	{
		u32	addr = (i >> 3);
		u32	offs = (i % 8);
		if (statMem[addr] & (stepMem<<offs))
		{
			*address = i;
			lastRecord = i+step;
			
			switch (_size)
			{
				case 0: *curVal=(u32)T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				case 1: *curVal=(u32)T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				case 2: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF; return TRUE;
				case 3: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				default: return TRUE;
			}
		}
	}
	lastRecord = 0;
	return FALSE;
}

void CHEATSEARCH::getListReset()
{
	lastRecord = 0;
}

// ========================================================================= Export
void CHEATSEXPORT::R4decrypt(u8 *buf, u32 len, u32 n)
{
	size_t r = 0;
	while (r < len)
	{
		size_t i ;
		u16 key = n ^ 0x484A;
		for (i = 0 ; i < 512 && i < len - r ; i ++)
		{
			u8 _xor = 0;
			if (key & 0x4000) _xor |= 0x80;
			if (key & 0x1000) _xor |= 0x40;
			if (key & 0x0800) _xor |= 0x20;
			if (key & 0x0200) _xor |= 0x10;
			if (key & 0x0080) _xor |= 0x08;
			if (key & 0x0040) _xor |= 0x04;
			if (key & 0x0002) _xor |= 0x02;
			if (key & 0x0001) _xor |= 0x01;

			u32 k = ((buf[i] << 8) ^ key) << 16;
			u32 x = k;
			for (u8 j = 1; j < 32; j ++)
				x ^= k >> j;
			key = 0x0000;
			if (BIT_N(x, 23)) key |= 0x8000;
			if (BIT_N(k, 22)) key |= 0x4000;
			if (BIT_N(k, 21)) key |= 0x2000;
			if (BIT_N(k, 20)) key |= 0x1000;
			if (BIT_N(k, 19)) key |= 0x0800;
			if (BIT_N(k, 18)) key |= 0x0400;
			if (BIT_N(k, 17) != BIT_N(x, 31)) key |= 0x0200;
			if (BIT_N(k, 16) != BIT_N(x, 30)) key |= 0x0100;
			if (BIT_N(k, 30) != BIT_N(k, 29)) key |= 0x0080;
			if (BIT_N(k, 29) != BIT_N(k, 28)) key |= 0x0040;
			if (BIT_N(k, 28) != BIT_N(k, 27)) key |= 0x0020;
			if (BIT_N(k, 27) != BIT_N(k, 26)) key |= 0x0010;
			if (BIT_N(k, 26) != BIT_N(k, 25)) key |= 0x0008;
			if (BIT_N(k, 25) != BIT_N(k, 24)) key |= 0x0004;
			if (BIT_N(k, 25) != BIT_N(x, 26)) key |= 0x0002;
			if (BIT_N(k, 24) != BIT_N(x, 25)) key |= 0x0001;
			buf[i] ^= _xor;
		}

		buf+= 512;
		r  += 512;
		n  += 1;
	}
}

bool CHEATSEXPORT::load(char *path)
{
	error = 0;

	fp = fopen(path, "rb");
	if (!fp)
	{
		printf("Error open database\n");
		error = 1;
		return false;
	}

	const char *headerID = "R4 CheatCode";
	char buf[255] = {0};
	fread(buf, 1, strlen(headerID), fp);
	if (strncmp(buf, headerID, strlen(headerID)) != 0)
	{
		// check encrypted
		R4decrypt((u8 *)buf, strlen(headerID), 0);
		if (strcmp(buf, headerID) != 0)
		{
			error = 2;
			return false;
		}
		encrypted = true;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (!search())
	{
		printf("ERROR: cheat in database not found\n");
		error = 3;
		return false;
	}
	
	if (!getCodes())
	{
		printf("ERROR: export cheats failed\n");
		error = 4;
		return false;
	}

	return true;
}
void CHEATSEXPORT::close()
{
	if (fp)
		fclose(fp);
	if (cheats)
	{
		delete [] cheats;
		cheats = NULL;
	}
}

bool CHEATSEXPORT::search()
{
	if (!fp) return false;

	u32		pos = 0x0100;
	FAT_R4	fat_tmp = {0};
	u8		buf[512] = {0};

	CRC = 0;
	encOffset = 0;
	u32 t = 0;
	memset(date, 0, sizeof(date));
	if (encrypted)
	{
		fseek(fp, 0, SEEK_SET);
		fread(&buf[0], 1, 512, fp);
		R4decrypt((u8 *)&buf[0], 512, 0);
		memcpy(&date[0], &buf[0x10], 16);
	}
	else
	{
		fseek(fp, 0x10, SEEK_SET);
		fread(&date, 16, 1, fp);
		fseek(fp, pos, SEEK_SET);
		fread(&fat_tmp, sizeof(fat), 1, fp);
	}

	while (1)
	{
		if (encrypted)
		{
			memcpy(&fat, &buf[pos % 512], sizeof(fat));
			pos += sizeof(fat);
			if ((pos>>9) > t)
			{
				t++;
				fread(&buf[0], 1, 512, fp);
				R4decrypt((u8 *)&buf[0], 512, t);
			}
			memcpy(&fat_tmp, &buf[pos % 512], sizeof(fat_tmp));	// next
		}
		else
		{
			memcpy(&fat, &fat_tmp, sizeof(fat));
			fread(&fat_tmp, sizeof(fat_tmp), 1, fp);
			
		}
		//printf("serial: %s, offset %08X\n", fat.serial, fat.addr);
		if (memcmp(gameInfo.header.gameCode, &fat.serial[0], 4) == 0)
		{
			dataSize = fat_tmp.addr?(fat_tmp.addr - fat.addr):0;
			if (encrypted)
			{
				encOffset = fat.addr % 512;
				dataSize += encOffset;
			}
			if (!dataSize) return false;
			CRC = fat.CRC;
			char buf[5] = {0};
			memcpy(&buf, &fat.serial[0], 4);
			printf("Cheats: found %s CRC %08X at 0x%08llX, size %i byte(s)\n", buf, fat.CRC, fat.addr, dataSize - encOffset);
			return true;
		}

		if (fat.addr == 0) break;
	}

	memset(&fat, 0, sizeof(FAT_R4));
	return false;
}

bool CHEATSEXPORT::getCodes()
{
	if (!fp) return false;

	u32	pos = 0;
	u32	pos_cht = 0;

	u8 *data = new u8 [dataSize+8];
	if (!data) return false;
	memset(data, 0, dataSize+8);
	
	fseek(fp, fat.addr - encOffset, SEEK_SET);

	if (fread(data, 1, dataSize, fp) != dataSize)
	{
		delete [] data;
		data = NULL;
		return false;
	}

	if (encrypted)
		R4decrypt(data, dataSize, fat.addr >> 9);
	
	intptr_t ptrMask = (~0 << 2);
	u8 *gameTitlePtr = (u8 *)data + encOffset;
	
	memset(gametitle, 0, CHEAT_DB_GAME_TITLE_SIZE);
	memcpy(gametitle, gameTitlePtr, strlen((const char *)gameTitlePtr));
	
	u32 *cmd = (u32 *)(((intptr_t)gameTitlePtr + strlen((const char *)gameTitlePtr) + 4) & ptrMask);
	numCheats = cmd[0] & 0x0FFFFFFF;
	cmd += 9;
	cheats = new CHEATS_LIST[numCheats];
	memset(cheats, 0, sizeof(CHEATS_LIST) * numCheats);

	while (pos < numCheats)
	{
		u32 folderNum = 1;
		u8	*folderName = NULL;
		u8	*folderNote = NULL;
		if ((*cmd & 0xF0000000) == 0x10000000)	// Folder
		{
			folderNum = (*cmd  & 0x00FFFFFF);
			folderName = (u8*)((intptr_t)cmd + 4);
			folderNote = (u8*)((intptr_t)folderName + strlen((char*)folderName) + 1);
			pos++;
			cmd = (u32 *)(((intptr_t)folderName + strlen((char*)folderName) + 1 + strlen((char*)folderNote) + 1 + 3) & ptrMask);
		}

		for (u32 i = 0; i < folderNum; i++)		// in folder
		{
			u8 *cheatName = (u8 *)((intptr_t)cmd + 4);
			u8 *cheatNote = (u8 *)((intptr_t)cheatName + strlen((char*)cheatName) + 1);
			u32 *cheatData = (u32 *)(((intptr_t)cheatNote + strlen((char*)cheatNote) + 1 + 3) & ptrMask);
			u32 cheatDataLen = *cheatData++;
			u32 numberCodes = cheatDataLen / 2;

			if (numberCodes <= MAX_XX_CODE)
			{
				std::string descriptionStr = "";
				
				if ( folderName && *folderName )
				{
					descriptionStr += (char *)folderName;
					descriptionStr += ": ";
				}
				
				descriptionStr += (char *)cheatName;
				
				if ( cheatNote && *cheatNote )
				{
					descriptionStr += " | ";
					descriptionStr += (char *)cheatNote;
				}
				
				strncpy(cheats[pos_cht].description, descriptionStr.c_str(), sizeof(cheats[pos_cht].description));
				cheats[pos_cht].description[sizeof(cheats[pos_cht].description) - 1] = '\0';
				
				cheats[pos_cht].num = numberCodes;
				cheats[pos_cht].type = 1;

				for(u32 j = 0, t = 0; j < numberCodes; j++, t+=2 )
				{
					cheats[pos_cht].code[j][0] = (u32)*(cheatData+t);
					//printf("%i: %08X ", j, cheats[pos_cht].code[j][0]);
					cheats[pos_cht].code[j][1] = (u32)*(cheatData+t+1);
					//printf("%08X\n", cheats[pos_cht].code[j][1]);
					
				}
				pos_cht++;
			}

			pos++;
			cmd = (u32 *)((intptr_t)cmd + ((*cmd + 1)*4));
		}
		
	};

	delete [] data;

	numCheats = pos_cht;
	//for (int i = 0; i < numCheats; i++)
	//	printf("%i: %s\n", i, cheats[i].description);
	
	return true;
}

CHEATS_LIST *CHEATSEXPORT::getCheats()
{
	return cheats;
}
u32 CHEATSEXPORT::getCheatsNum()
{
	return numCheats;
}
