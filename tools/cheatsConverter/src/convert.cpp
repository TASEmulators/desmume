/* Cheats converter

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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <algorithm>
#include "convert.h"

CHEATS_LIST	list[MAX_CHEAT_LIST] = {0};
u32			num = 0;
char		ROMserial[50] = {0};

static void convertSerial()
{
	const char *regions[] = {	"JPFSEDIRKH",
								"JPN",
								"EUR",
								"FRA",
								"ESP",
								"USA",
								"NOE",
								"ITA",
								"RUS",
								"KOR",
								"HOL",

	};
	char buf[5] = {0};

	if (!ROMserial[0]) return;

	memcpy(buf, &ROMserial[strlen(ROMserial)-6], 4);
	sprintf(ROMserial, "NTR-%s-", buf);
	u32 region = (u32)(std::max<s32>(strchr(regions[0], buf[3]) - regions[0] + 1, 0));
	if (region != 0)
		strcat(ROMserial, regions[region]);
	else
		memset(ROMserial, 0, sizeof(ROMserial));
}

static void cheatsClear()
{
	memset(list, 0, sizeof(list));
	for (int i = 0; i < MAX_CHEAT_LIST; i++)
		list[i].type = 0xFF;
	num = 0;
}

bool save(char *filename)
{
	char	*types[] = {"DS", "AR", "CB"};
	char	buf[(sizeof(list[0].hi)+sizeof(list[0].lo)) * 2 + 200] = { 0 };

	FILE	*flist = fopen(filename, "w");

	if (flist)
	{
		fprintf(flist, "; DeSmuME cheat file. VERSION %i.%03i\n", CHEAT_VERSION_MAJOR, CHEAT_VERSION_MINOR);
		fprintf(flist, "Name=\n");
		fprintf(flist, "Serial=%s\n", ROMserial);
		fputs("\n; lists list\n", flist);
		for (unsigned int i = 0;  i < num; i++)
		{
			if (list[i].num == 0) continue;
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "%s %c ", types[list[i].type], list[i].enabled?'1':'0');
			for (int t = 0; t < list[i].num; t++)
			{
				char buf2[10] = { 0 };
				sprintf(buf2, "%08X", list[i].hi[t]);
				strcat(buf, buf2);
				sprintf(buf2, "%08X", list[i].lo[t]);
				strcat(buf, buf2);
				if (t < (list[i].num - 1))
					strcat(buf, ",");
			}
			strcat(buf, " ;");
			strcat(buf, trim(list[i].description));
			fprintf(flist, "%s\n", buf);
		}
		fputs("\n", flist);
		fclose(flist);
		return true;
	}

	return false;
}

//==================================================================================== Loads
bool load_1_0(char *fname)
{
#ifdef WIN32
	char buf[200] = {0};
	num = GetPrivateProfileInt("General", "NumberOfCheats", 0, fname);
	if (num == 0) return false;
	if (num > MAX_CHEAT_LIST) num = MAX_CHEAT_LIST;
	for (unsigned int i = 0; i < num; i++)
	{
		wsprintf(buf, "Desc%04i", i);
		GetPrivateProfileString("Cheats", buf, "", list[i].description, 75, fname);
		wsprintf(buf, "Type%04i", i);
		list[i].type = GetPrivateProfileInt("Cheats", buf, 0xFF, fname);
		wsprintf(buf, "Num_%04i", i);
		list[i].num = GetPrivateProfileInt("Cheats", buf, 0, fname);
		wsprintf(buf, "Enab%04i", i);
		list[i].enabled = GetPrivateProfileInt("Cheats", buf, 0, fname)==1?true:false;
		wsprintf(buf, "Size%04i", i);
		list[i].size = GetPrivateProfileInt("Cheats", buf, 0, fname);
		for (int t = 0; t < list[i].num; t++)
		{
			char tmp_buf[10] = { 0 };
			wsprintf(buf, "H%03i%04i", i, t);
			GetPrivateProfileString("Cheats", buf, "0", tmp_buf, 10, fname);
			sscanf_s(tmp_buf, "%x", &list[i].hi[t]);
			wsprintf(buf, "L%03i%04i", i, t);
			list[i].lo[t] = GetPrivateProfileInt("Cheats", buf, 0, fname);
		}
	}
#endif
	return true;
}

bool load_1_3(char *fname)
{
	FILE	*fcheat = fopen(fname, "r");
	char	buf[4096] = {0};
	int		last=0;
	
	if (fcheat)
	{
		cheatsClear();
		memset(buf, 0, sizeof(buf));

		while (!feof(fcheat))
		{
			fgets(buf, sizeof(buf), fcheat);
			if (buf[0] == ';') continue;
			strcpy(buf, trim(buf));
			if (strlen(buf) == 0) continue;
			if ( (buf[0] == 'D') &&
					(buf[1] == 'e') &&
					(buf[2] == 's') &&
					(buf[3] == 'c') &&
					(buf[4] == '=') )		// new cheat block
			{
				strncpy((char *)list[last].description, (char *)buf+5, strlen(buf)-5);
				fgets(buf, sizeof(buf), fcheat);
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
					list[last].type=atoi(bf);
					dstart += (dsize+1); dsize=0;
					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					if (buf[dstart+dsize]==0) continue;			// error
					strncpy(bf, (char*)buf+dstart, dsize);
					dstart += (dsize+1); dsize=0;
					list[last].num=atoi(bf);
					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					if (buf[dstart+dsize]==0) continue;			// error
					strncpy(bf, (char*)(buf+dstart), dsize);
					dstart += (dsize+1); dsize=0;
					list[last].enabled=atoi(bf)==1?true:false;
					while ( (buf[dstart+dsize] != ',') && (buf[dstart+dsize]!=0)) { dsize++; };
					strncpy(bf, (char*)buf+dstart, dsize);
					list[last].size=atoi(bf);
					fgets(buf, sizeof(buf), fcheat);
					if ( (buf[0] == 'D') &&
							(buf[1] == 'a') &&
							(buf[2] == 't') &&
							(buf[3] == 'a') &&
							(buf[4] == '=') )		// Data cheat
					{
						int		offs = 5;
						char	tmp_buf[9];
						memset(tmp_buf, 0, 9);

						for (int j=0; j<list[last].num; j++)
						{
							strncpy(tmp_buf, (char *)buf+offs, 8);
							sscanf_s(tmp_buf, "%x", &list[last].hi[j]);
							offs+=8;
							strncpy(tmp_buf, (char *)buf+offs, 8);
							sscanf_s(tmp_buf, "%x", &list[last].lo[j]);
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
		num = last;

		convertSerial();
		return true;
	}
	return false;
}

bool getVersion(char *fname, u8 &major, u8 &minor)
{
	FILE	*fp = fopen(fname, "r");
	major = 0;
	minor = 0;
	memset(ROMserial, 0, sizeof(ROMserial));
	
	if (fp)
	{
		char buf[1024] = { 0 };
		fgets(buf, 1024, fp);
		strcpy(buf, trim(buf));
		if (strlen(buf) == 0)
		{
			fclose(fp);
			return false;
		}

		if (strncmp(buf, "; DeSmuME cheat file. VERSION ", 29) == 0)
		{
			char bf[10] = {0};
			strncpy(bf, (char*)(buf + 30), 1);
			major = atoi(bf);
			strncpy(bf, (char*)(buf + 32), 3);
			minor = atoi(bf);

			while (!feof(fp))
			{
				fgets(buf, 1024, fp);
				strcpy(buf, trim(buf));
				if ( (buf[0] == 'N') &&
						(buf[1] == 'a') &&
						(buf[2] == 'm') &&
						(buf[3] == 'e') &&
						(buf[4] == '=') )		// serial
				{
					strcpy(ROMserial, (char *)(buf+5));
					break;
				}
			}

			fclose(fp);
			return true;
		}

		if (strncmp(buf, "[General]", 9) == 0)
		{
			major = 1; minor = 0;
			fclose(fp);
			return true;
		}

		fclose(fp);
	}
	return false;
}

bool convertFile(char *fname)
{
	u8	majorVersion = 0;
	u8	minorVersion = 0;

	if (!getVersion(fname, majorVersion, minorVersion))
		return false;
	//printf(" - version %i.%03i - ", majorVersion, minorVersion);

	switch(majorVersion)
	{
		case 1:
			switch (minorVersion)
			{
				case 0:
				{
					if (!load_1_0(fname)) return false;
					if (!save(fname)) return false;
					return true;
				}
				case 3:
				{
					if (!load_1_3(fname)) return false;
					if (!save(fname)) return false;
					return true;
				}
				
				default:
					return false;
			}
			break;
		case 2:
			return false;
	}
	
	return false;
}

