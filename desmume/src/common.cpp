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

#include "common.h"

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
		sprintf(IniName, ".\\desmume.ini",szPath);
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
