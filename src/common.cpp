/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2008 DeSmuME team

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <string>
#include "common.h"

u8	gba_header_data_0x04[156] = {
	0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD,
	0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21,0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,
	0x10,0x46,0x4A,0x4A,0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF,
	0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0,0x13,0x72,0xA7,0xFC,
	0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61,0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,
	0x23,0x1D,0xC7,0x61,0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD,
	0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85,0x60,0xD6,0x80,0x25,
	0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2,0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,
	0x78,0x00,0x90,0xCB,0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF,
	0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07};

#ifdef WIN32
char IniName[MAX_PATH];

void GetINIPath()
{   
	char		vPath[MAX_PATH], *szPath, currDir[MAX_PATH];
    /*if (*vPath)
       szPath = vPath;
    else
    {*/
       char *p;
       ZeroMemory(vPath, sizeof(vPath));
       GetModuleFileName(NULL, vPath, sizeof(vPath));
       p = vPath + lstrlen(vPath);
       while (p >= vPath && *p != '\\') p--;
       if (++p >= vPath) *p = 0;
       szPath = vPath;
    //}
	if (strlen(szPath) + strlen("\\desmume.ini") < MAX_PATH)
	{
		sprintf(IniName, "%s\\desmume.ini",szPath);
	} else if (MAX_PATH> strlen(".\\desmume.ini")) {
		sprintf(IniName, ".\\desmume.ini");
	} else
	{
		memset(IniName,0,MAX_PATH) ;
	}
}

void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val);
	WritePrivateProfileString(appname, keyname, temp, file);
}

#endif
	
u8 reverseBitsInByte(u8 x)
{
	u8 h = 0;
	u8 i = 0;

	for (i = 0; i < 8; i++)
	{
		h = (h << 1) + (x & 1); 
		x >>= 1; 
	}

	return h;
}

void removeCR(char *buf)
{
	int l=strlen(buf);
	for (int i=0; i < l; i++)
	{
		if (buf[i]==0x0A) buf[i]=0;
		if (buf[i]==0x0D) buf[i]=0;
	}
}

u32 strlen_ws(char *buf)		// length without last spaces
{
	if (!strlen(buf)) return 0;
	for (int i=strlen(buf); i>0; i--)
	{
		if (buf[i]!=32) return (i-1);		// space
	}
	return 0;
}

std::string RomName = "";				//Stores the name of the Rom currently loaded in memory

/***
 * Author: adelikat
 * Date Added: May 8, 2009
 * Description: Sets the Global variable RomName
 * Known Usage:
 *				LoadRom
 **/
void SetRomName(const char *filename)
{
	std::string str = filename;
	
	//Truncate the path from filename
	int x = str.find_last_of("/\\");
	if (x > 0)
		str = str.substr(x+1);
	RomName = str;
}

/***
 * Author: adelikat
 * Date Added: May 8, 2009
 * Description: Returns the Global variable RomName
 * Known Usage:
 *				included in main.h
 *				ramwatch.cpp - SaveStrings
 **/

const char *GetRomName()
{
	return RomName.c_str();
}