/*	
	Copyright (C) 2008-2010 DeSmuME team

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

#include "common.h"
#include "path.h"
#include "winutil.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windef.h>
#include <ShlObj.h>


char IniName[MAX_PATH];

char* _hack_alternateModulePath;


static char vPath[MAX_PATH*2], *szPath;

void GetINIPath()
{   

	bool useModulePath = true;

	//check if desmume is running from the temp directory.
	{
		//DebugBreak();
		wchar_t tempPath[MAX_PATH];
		GetTempPathW(MAX_PATH,tempPath);
		wchar_t modulePath[MAX_PATH];
		GetModuleFileNameW(NULL, modulePath, MAX_PATH);
		if(!_wcsnicmp(tempPath,modulePath,wcslen(tempPath)))
		{
			//running from temp dir
			
			//decided not to warn user
			//GetPrivateProfileBool("General","Temp Directory Warning", false, IniName))
			//MessageBox(NULL,"You are running DeSmuME from your temp directory, probably by running straight from an archive instead of dearchiving to your disk manually......

			//use an alternate path
			useModulePath = false;
			static char userpath[MAX_PATH];
			SHGetFolderPath(NULL,CSIDL_LOCAL_APPDATA,NULL,0,userpath);
			_snprintf(vPath,MAX_PATH,"%s\\%s",userpath,"DeSmuME");
			szPath = vPath;
			_hack_alternateModulePath = szPath;

			//not so sure about this.. but lets go for it.
			SetCurrentDirectory(userpath);
		}
	}


	if(useModulePath)
	{
		 char *p;
		 ZeroMemory(vPath, sizeof(vPath));
		 GetModuleFileName(NULL, vPath, sizeof(vPath));
		 p = vPath + lstrlen(vPath);
		 while (p >= vPath && *p != '\\') p--;
		 if (++p >= vPath) *p = 0;
		 szPath = vPath;
	}
	if (strlen(szPath) + strlen("\\desmume.ini") < MAX_PATH)
	{
		sprintf(IniName, "%s\\desmume.ini",szPath);
	} else if (MAX_PATH> strlen(".\\desmume.ini")) {
		sprintf(IniName, ".\\desmume.ini");
	} else
	{
		memset(IniName,0,MAX_PATH) ;
	}

	FCEUD_MakePathDirs(IniName);
}

void PreventScreensaver()
{
	//a 0,0 mouse motion is indeed sufficient
	//i have been unable to notice any ill effects
	INPUT fakeMouse;
	fakeMouse.type = INPUT_MOUSE;
	fakeMouse.mi.dx = fakeMouse.mi.dy = 0;
	fakeMouse.mi.dwFlags = MOUSEEVENTF_MOVE;
	fakeMouse.mi.time = 0;
	fakeMouse.mi.dwExtraInfo = 0;
	SendInput(1,&fakeMouse,sizeof(INPUT));
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

void DesEnableMenuItem(HMENU hMenu, UINT uIDEnableItem, bool enable)
{
	EnableMenuItem(hMenu, uIDEnableItem, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
}

std::string GetPrivateProfileStdString(LPCSTR lpAppName,LPCSTR lpKeyName,LPCSTR lpDefault)
{
	static char buf[65536];
	GetPrivateProfileString(lpAppName, lpKeyName, lpDefault, buf, 65536, IniName);
	return buf;
}

std::wstring STRW(UINT id)
{
	static const int BUFSIZE = 32768;
	static wchar_t wstr[BUFSIZE];
	LoadStringW(NULL,id,wstr,BUFSIZE);
	return wstr;
}

bool IsDlgCheckboxChecked(HWND hDlg, int id)
{
	return IsDlgButtonChecked(hDlg,id) == BST_CHECKED;
}

void CheckDlgItem(HWND hDlg, int id, bool checked)
{
	CheckDlgButton(hDlg, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

HMENU GetSubMenuByIdOfFirstChild(HMENU menu, UINT child)
{
	int count = GetMenuItemCount(menu);
	for(int i=0;i<count;i++)
	{
		HMENU sub = GetSubMenu(menu,i);
		MENUITEMINFO moo;
		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_ID;
		GetMenuItemInfo(sub,0,TRUE,&moo);
		if(moo.wID == child)
			return sub;
	}
	return (HMENU)0;
}

HMENU GetSubMenuById(HMENU menu, UINT id)
{
	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU;
	GetMenuItemInfo(menu,id,FALSE,&moo);
	return moo.hSubMenu;
}

int GetSubMenuIndexByHMENU(HMENU menu, HMENU sub)
{
	int count = GetMenuItemCount(menu);
	for(int i=0;i<count;i++)
	{
		HMENU trial = GetSubMenu(menu,i);
		if(sub == trial) return i;
	}
	return -1;
}