/*	winutil.cpp

    Copyright (C) 2008-2009 DeSmuME team

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

#include "winutil.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windef.h>

char IniName[MAX_PATH];

void GetINIPath()
{   
	char		vPath[MAX_PATH], *szPath;
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

void WritePrivateProfileBool(char* appname, char* keyname, bool val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val?1:0);
	WritePrivateProfileString(appname, keyname, temp, file);
}

bool GetPrivateProfileBool(const char* appname, const char* keyname, bool defval, const char* filename)
{
	return GetPrivateProfileInt(appname,keyname,defval?1:0,filename) != 0;
}

void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val);
	WritePrivateProfileString(appname, keyname, temp, file);
}

