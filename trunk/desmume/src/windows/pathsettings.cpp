/*  
	Copyright (C) 2007 Hicoder

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
/* notes: will not save paths for current session if IDCANCEL is sent it 
will only change paths for that session

the only way paths will be saved is if IDOK is sent

the default paths are saved on first load
*/

#include "../common.h"
#include <windowsx.h>
#include <shlobj.h>
#include <commctrl.h>
#include <time.h>
#include <stdlib.h>
#include "main.h"
#include "NDSSystem.h"
#include "pathSettings.h"
#include "../debug.h"

/*macros to forward messages
the dialog procedure was getting long and confusing for me
*/
#define HANDLE_DLGMSG(hwnd, message, fn)										\
	case (message): return (SetDlgMsgResult(hDlg, uMsg,							\
	HANDLE_##message( (hwnd), (wParam), (lParam), (fn) ) ) )


/*variable declaration*/

char pathToRoms[MAX_PATH];
char pathToBattery[MAX_PATH];
char pathToStates[MAX_PATH];
char pathToScreenshots[MAX_PATH];
char pathToAviFiles[MAX_PATH];
char pathToCheats[MAX_PATH];
char pathToSounds[MAX_PATH];
char pathToFirmware[MAX_PATH];
char pathToModule[MAX_PATH];
char pathToLua[MAX_PATH];
char screenshotFormat[MAX_FORMAT];

char *currentSelection = 0;
char *currentKey = 0;

BOOL romsLastVisit = FALSE;
BOOL needsSaving = FALSE;

ImageFormat defaultFormat = PNG;

/* end variable declaration*/

/*private functions*/

void ReadKey(char *pathToRead, const char *key)
{
	ZeroMemory(pathToRead, sizeof(pathToRead));
	GetPrivateProfileString(SECTION, key, key, pathToRead, MAX_PATH, IniName);
	if(strcmp(pathToRead, key) == 0)
		//since the variables are all intialized in this file they all use MAX_PATH
		GetDefaultPath(pathToRead, key, MAX_PATH);
}

int InitialFolder(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if(uMsg == BFFM_INITIALIZED)
	{
		SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM)TRUE, lpData);
	}
	return 0;
}

BOOL BrowseForPath(char *pathToBrowse)
{
	LPMALLOC shMalloc;
	BOOL changed = false;
	LPITEMIDLIST idList;
	BROWSEINFO bi;

	//stupid shell
	if(SHGetMalloc( &shMalloc) != S_OK)
		return FALSE;

	ZeroMemory(&idList, sizeof(idList));
	ZeroMemory(&bi, sizeof(bi));

	char tmp[MAX_PATH];
	strncpy(tmp, pathToBrowse, MAX_PATH);

	bi.hwndOwner = MainWindow->getHWnd();
	bi.lpszTitle = "Choose a Folder";
	bi.ulFlags = BIF_NONEWFOLDERBUTTON;

	/*wanted to add a callback function for the folder initialization but it crashes everytime i do
	bi.lpfn = (BFFCALLBACK)InitialFolder;
	bi.lParam = (LPARAM)pathToBrowse;
	*/

	if( (idList = SHBrowseForFolder(&bi)) )
	{
		changed = true;
		SHGetPathFromIDList(idList, pathToBrowse);
//		shMalloc->Free(&idList);
	}

	return changed;
}

void LoadModulePath()
{
	//stolen from common.cpp GetINIPath
	char *p;
	ZeroMemory(pathToModule, sizeof(pathToModule));
	GetModuleFileName(NULL, pathToModule, sizeof(pathToModule));
	p = pathToModule + lstrlen(pathToModule);
	while (p >= pathToModule && *p != '\\') p--;
	if (++p >= pathToModule) *p = 0;
}

void SwitchPath(Action action, KnownPath path, char * buffer, int maxCount)
{
	char *pathToCopy = 0;
	switch(path)
	{
		case ROMS:
			pathToCopy = pathToRoms;
			break;
		case BATTERY:
			pathToCopy = pathToBattery;
			break;
		case STATES:
			pathToCopy = pathToStates;
			break;
		case SCREENSHOTS:
			pathToCopy = pathToScreenshots;
			break;
		case AVI_FILES:
			pathToCopy = pathToAviFiles;
			break;
		case CHEATS:
			pathToCopy = pathToCheats;
			break;
		case SOUNDS:
			pathToCopy = pathToSounds;
			break;
		case FIRMWARE:
			pathToCopy = pathToFirmware;
			break;
		case MODULE:
			pathToCopy = pathToModule;
			break;
	}

	if(action == GET)
	{
		strncpy(buffer, pathToCopy, maxCount);
		strcat(buffer, "\\");
	}
	else if(action == SET)
	{
		int len = strlen(buffer)-1;
		if(buffer[len] == '\\') 
			buffer[len] = '\0';

		strncpy(pathToCopy, buffer, MAX_PATH);
	}
}

/* end private functions */

/* public functions */

//returns "filename"
void GetFilename(char *buffer, int maxCount)
{
	char *lchr = strrchr(pathFilenameToROMwithoutExt, '\\');
	lchr++;
	strncpy(buffer, lchr, maxCount);
}

void GetFullPathNoExt(KnownPath path, char *buffer, int maxCount)
{
	GetPathFor(path, buffer, maxCount);
	char filename[MAX_PATH];
	ZeroMemory(filename, sizeof(filename));
	GetFilename(filename, MAX_PATH);	
	strcat(buffer, filename);
}

void GetPathFor(KnownPath path, char *buffer, int maxCount)
{
	SwitchPath(GET, path, buffer, maxCount);
}

void SetPathFor(KnownPath path, char *buffer)
{
	SwitchPath(SET, path, buffer, 0);
}

void GetDefaultPath(char *pathToDefault, const char *key, int maxCount)
{
	strncpy(pathToDefault, pathToModule, maxCount);
//	strcat(pathToDefault, key);

//	if(GetFileAttributes(pathToDefault) == INVALID_FILE_ATTRIBUTES)
//		CreateDirectory(pathToDefault, NULL);
}

void WritePathSettings()
{
	WritePrivateProfileString(SECTION, ROMKEY, pathToRoms, IniName);
	WritePrivateProfileString(SECTION, BATTERYKEY, pathToBattery, IniName);
	WritePrivateProfileString(SECTION, STATEKEY, pathToStates, IniName);
	WritePrivateProfileString(SECTION, SCREENSHOTKEY, pathToScreenshots, IniName);
	WritePrivateProfileString(SECTION, AVIKEY, pathToAviFiles, IniName);
	WritePrivateProfileString(SECTION, CHEATKEY, pathToCheats, IniName);
	WritePrivateProfileString(SECTION, SOUNDKEY, pathToSounds, IniName);
	WritePrivateProfileString(SECTION, FIRMWAREKEY, pathToFirmware, IniName);
	WritePrivateProfileString(SECTION, LUAKEY, pathToLua, IniName);

	WritePrivateProfileString(SECTION, FORMATKEY, screenshotFormat, IniName);

	WritePrivateProfileInt(SECTION, LASTVISITKEY, romsLastVisit, IniName);
	WritePrivateProfileInt(SECTION, DEFAULTFORMATKEY, defaultFormat, IniName);
	WritePrivateProfileInt(SECTION, NEEDSSAVINGKEY, needsSaving, IniName);
}

void ReadPathSettings()
{
	if( ( strcmp(pathToModule, "") == 0) || !pathToModule)
		LoadModulePath();

	ReadKey(pathToRoms, ROMKEY);
	ReadKey(pathToBattery, BATTERYKEY);
	ReadKey(pathToStates, STATEKEY);
	ReadKey(pathToScreenshots, SCREENSHOTKEY);
	ReadKey(pathToAviFiles, AVIKEY);
	ReadKey(pathToCheats, CHEATKEY);
	ReadKey(pathToSounds, SOUNDKEY);
	ReadKey(pathToFirmware, FIRMWAREKEY);
	ReadKey(pathToLua, LUAKEY);
	
	GetPrivateProfileString(SECTION, FORMATKEY, "%f_%s_%r", screenshotFormat, MAX_FORMAT, IniName);

	romsLastVisit	= GetPrivateProfileInt(SECTION, LASTVISITKEY, TRUE, IniName);
	defaultFormat	= (ImageFormat)GetPrivateProfileInt(SECTION, DEFAULTFORMATKEY, PNG, IniName);

	needsSaving		= GetPrivateProfileInt(SECTION, NEEDSSAVINGKEY, TRUE, IniName);
	if(needsSaving)
	{
		needsSaving = FALSE;
		WritePathSettings();
	}
}

ImageFormat GetImageFormatType()
{
	return defaultFormat;
}

void FormatName(char *output, int maxCount)
{
	char file[MAX_PATH];
	ZeroMemory(file, sizeof(file));
	time_t now = time(NULL);
	tm *time_struct = localtime(&now);
	srand(now);
  
	for(int i = 0; i < MAX_FORMAT;i++) 
	{		
		char *c = &screenshotFormat[i];
		char tmp[MAX_PATH];
		ZeroMemory(tmp, sizeof(tmp));

		if(*c == '%')
		{
			c = &screenshotFormat[++i];
			switch(*c)
			{
				case 'f':
						GetFilename(tmp, MAX_PATH);
					break;
				case 'D':
						strftime(tmp, MAX_PATH, "%d", time_struct);
					break;
				case 'M':
						strftime(tmp, MAX_PATH, "%m", time_struct);
					break;
				case 'Y':
						strftime(tmp, MAX_PATH, "%Y", time_struct);
					break;
				case 'h':
						strftime(tmp, MAX_PATH, "%H", time_struct);
					break;
				case 'm':
						strftime(tmp, MAX_PATH, "%M", time_struct);
					break;
				case 's':
						strftime(tmp, MAX_PATH, "%S", time_struct);
					break;
				case 'r':
						sprintf(tmp, "%d", rand() % RAND_MAX);
					break;
			}
		}
		else
		{
			int j;
			for(j=i;j<MAX_FORMAT-i;j++)
				if(screenshotFormat[j] != '%')
					tmp[j-i]=screenshotFormat[j];
				else
					break;
			tmp[j-i]='\0';
		}
		strcat(file, tmp);
	}
	strncpy(output, file, maxCount);
}

BOOL SavePathForRomVisit()
{
	return romsLastVisit;
}
/* end public functions 

/*class functions*/

void HideControls(HWND hDlg)
{/*
	HWND h = GetDlgItem(hDlg, IDC_USELASTVISIT);
	ShowWindow(h, SW_HIDE);
	
	h = GetDlgItem(hDlg, IDC_SAVEAS);
	ShowWindow(h, SW_HIDE);

	h = GetDlgItem(hDlg, IDC_PNG);
	ShowWindow(h, SW_HIDE);

	h = GetDlgItem(hDlg, IDC_BMP);
	ShowWindow(h, SW_HIDE);

	h = GetDlgItem(hDlg, IDC_FORMATSTATIC);
	ShowWindow(h, SW_HIDE);

	h = GetDlgItem(hDlg, IDC_FORMATEDIT);
	ShowWindow(h, SW_HIDE);*/
}

void PathSettings_OnSelChange(HWND hDlg, HWND hwndCtl)
{
/*	HideControls(hDlg);

	switch(ComboBox_GetCurSel(hwndCtl))
	{
		case ROMS:
			{
				currentSelection = pathToRoms;
				currentKey = ROMKEY;
				HWND h = GetDlgItem(hDlg, IDC_USELASTVISIT);
				ShowWindow(h, SW_SHOWNA);
			}			
			break;
		case BATTERY:
			currentSelection = pathToBattery;
			currentKey = BATTERYKEY;
			break;
		case STATES:
			currentSelection = pathToStates;
			currentKey = STATEKEY;
			break;
		case SCREENSHOTS:
			{
				currentSelection = pathToScreenshots;
				currentKey = SCREENSHOTKEY;

				HWND h = GetDlgItem(hDlg, IDC_SAVEAS);
				ShowWindow(h, SW_SHOWNA);
				
				h = GetDlgItem(hDlg, IDC_PNG);
				ShowWindow(h, SW_SHOWNA);
				
				h = GetDlgItem(hDlg, IDC_BMP);
				ShowWindow(h, SW_SHOWNA);
				
				h = GetDlgItem(hDlg, IDC_FORMATSTATIC);
				ShowWindow(h, SW_SHOWNA);
				
				h = GetDlgItem(hDlg, IDC_FORMATEDIT);
				ShowWindow(h, SW_SHOWNA);
			}
			break;
		case AVI_FILES:
			{
				currentSelection = pathToAviFiles;
				currentKey = AVIKEY;

				HWND h = GetDlgItem(hDlg, IDC_FORMATSTATIC);
				ShowWindow(h, SW_SHOWNA);

				h = GetDlgItem(hDlg, IDC_FORMATEDIT);
				ShowWindow(h, SW_SHOWNA);
			}	
			break;
		case CHEATS:
			currentSelection = pathToCheats;
			currentKey = CHEATKEY;
			break;
		case SOUNDS:
			currentSelection = pathToSounds;
			currentKey = SOUNDKEY;
			break;
		case FIRMWARE:
			currentSelection = pathToFirmware;
			currentKey = FIRMWAREKEY;
			break;
	}
	SetDlgItemText(hDlg, IDC_PATHEDIT, currentSelection);*/

}

void PathSettings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
	switch(id)
	{
		case IDC_BROWSEROMS:
			{
				if(BrowseForPath(pathToRoms))
					SetDlgItemText(hDlg, IDC_ROMPATHEDIT, pathToRoms);
			}
			break;
		case IDC_BROWSESRAM:
			{
				if(BrowseForPath(pathToBattery))
					SetDlgItemText(hDlg, IDC_SAVERAMPATHEDIT, pathToBattery);
			}
			break;
		case IDC_BROWSESTATES:
			{
				if(BrowseForPath(pathToStates))
					SetDlgItemText(hDlg, IDC_STATEPATHEDIT, pathToStates);
			}
			break;
		case IDC_BROWSESCREENSHOTS:
			{
				if(BrowseForPath(pathToScreenshots))
					SetDlgItemText(hDlg, IDC_SCREENSHOTPATHEDIT, pathToScreenshots);
			}
			break;
		case IDC_BROWSEAVI:
			{
				if(BrowseForPath(pathToAviFiles))
					SetDlgItemText(hDlg, IDC_AVIPATHEDIT, pathToAviFiles);
			}
			break;
		case IDC_BROWSECHEATS:
			{
				if(BrowseForPath(pathToCheats))
					SetDlgItemText(hDlg, IDC_CHEATPATHEDIT, pathToCheats);
			}
			break;
		case IDC_BROWSELUA:
			{
				if(BrowseForPath(pathToLua))
					SetDlgItemText(hDlg, IDC_LUAPATHEDIT, pathToLua);
			}
			break;
		case IDC_PATHDEFAULTS:
			{
			/*	GetDefaultPath(currentSelection, currentKey, MAX_PATH);
				SetDlgItemText(hDlg, IDC_PATHEDIT, currentSelection);*/
			}
			break;
		case IDC_PNG:
				CheckRadioButton(hDlg, IDC_PNG, IDC_BMP, IDC_PNG);
				defaultFormat = PNG;
			break;
		case IDC_BMP:
				CheckRadioButton(hDlg, IDC_PNG, IDC_BMP, IDC_BMP);
				defaultFormat = BMP;
			break;
		case IDC_USELASTVISIT:
			{
				romsLastVisit = !romsLastVisit;
				CheckDlgButton(hDlg, IDC_USELASTVISIT, (romsLastVisit) ? BST_CHECKED : BST_UNCHECKED);
			}
			break;
		case IDC_PATHLIST:
			{
				if(codeNotify == CBN_SELCHANGE)
				{
					PathSettings_OnSelChange(hDlg, hwndCtl);
					return;
				}
			}
			break;
		case IDC_FORMATEDIT:
			{
				if(codeNotify == EN_KILLFOCUS)
				{
					char buffer[MAX_FORMAT];
					GetDlgItemText(hDlg, IDC_FORMATEDIT, buffer, MAX_FORMAT);
					strncpy(screenshotFormat, buffer, MAX_FORMAT);
				}
			}
			break;
		case IDOK:

			GetDlgItemText(hDlg, IDC_ROMPATHEDIT, pathToRoms, MAX_PATH);
			GetDlgItemText(hDlg, IDC_SAVERAMPATHEDIT, pathToBattery, MAX_PATH);
			GetDlgItemText(hDlg, IDC_STATEPATHEDIT, pathToStates, MAX_PATH);
			GetDlgItemText(hDlg, IDC_SCREENSHOTPATHEDIT, pathToScreenshots, MAX_PATH);
			GetDlgItemText(hDlg, IDC_AVIPATHEDIT, pathToAviFiles, MAX_PATH);
			GetDlgItemText(hDlg, IDC_CHEATPATHEDIT, pathToCheats, MAX_PATH);
			GetDlgItemText(hDlg, IDC_LUAPATHEDIT, pathToLua, MAX_PATH);

			WritePathSettings();
			EndDialog(hDlg, 0);
			break;
		case IDCANCEL:
			ReadPathSettings();
			EndDialog(hDlg, 0);
			break;

	}
}

BOOL PathSettings_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	HWND hwnd = GetDlgItem(hDlg, IDC_PATHLIST);
/*	ComboBox_AddString(hwnd, ROMKEY);
	ComboBox_AddString(hwnd, BATTERYKEY);
	ComboBox_AddString(hwnd, STATEKEY);
	ComboBox_AddString(hwnd, SCREENSHOTKEY);
	ComboBox_AddString(hwnd, AVIKEY);
	ComboBox_AddString(hwnd, CHEATKEY);
	ComboBox_AddString(hwnd, SOUNDKEY);
	ComboBox_AddString(hwnd, FIRMWAREKEY);

	ComboBox_SetCurSel(hwnd, 0);*/
	PathSettings_OnSelChange(hDlg, NULL);

	CheckDlgButton(hDlg, IDC_USELASTVISIT, (romsLastVisit) ? BST_CHECKED : BST_UNCHECKED);
	CheckRadioButton(hDlg, IDC_PNG, IDC_BMP, (int)GetImageFormatType());

// IDC_FORMATEDIT setup
	SetDlgItemText(hDlg, IDC_FORMATEDIT, screenshotFormat);

	hwnd = GetDlgItem(hDlg, IDC_FORMATEDIT);
	Edit_LimitText(hwnd, MAX_FORMAT);

	HWND toolTip = CreateWindowEx(NULL, 
		TOOLTIPS_CLASS, NULL, 
		TTS_ALWAYSTIP, 
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hDlg, NULL, 
		hAppInst, NULL);
	SendMessage(toolTip, TTM_SETMAXTIPWIDTH, NULL, (LPARAM)330);

	SetWindowPos(toolTip, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	SetDlgItemText(hDlg, IDC_ROMPATHEDIT, pathToRoms);
	SetDlgItemText(hDlg, IDC_SAVERAMPATHEDIT, pathToBattery);
	SetDlgItemText(hDlg, IDC_STATEPATHEDIT, pathToStates);
	SetDlgItemText(hDlg, IDC_SCREENSHOTPATHEDIT, pathToScreenshots);
	SetDlgItemText(hDlg, IDC_AVIPATHEDIT, pathToAviFiles);
	SetDlgItemText(hDlg, IDC_CHEATPATHEDIT, pathToCheats);
	SetDlgItemText(hDlg, IDC_LUAPATHEDIT, pathToLua);

	TOOLINFO ti;
	ZeroMemory(&ti, sizeof(ti));
	ti.cbSize = sizeof(ti);
	ti.hwnd = hDlg;
	ti.hinst = hAppInst;
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	ti.uId = (UINT_PTR)hwnd;
	ti.lpszText = "The format a screenshot should be saved in.\r\n%f\t\t\tFilename\r\n%D\t\t\tDay:Two Digit\r\n%M\t\t\tMonth:Two Digit\r\n%Y\t\t\tYear:Four Digit\r\n%h\t\t\tHour:Two Digit\r\n%m\t\t\tMinute: Two Digit\r\n%s\t\t\tSecond: Two Digit\r\n%r\t\tRandom: Min:0 Max:RAND_MAX";
	GetClientRect(hwnd, &ti.rect);
	SendMessage(toolTip, TTM_ADDTOOL, NULL, (LPARAM)&ti);

	return TRUE;
}

LRESULT CALLBACK PathSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		HANDLE_DLGMSG(hDlg, WM_INITDIALOG,	PathSettings_OnInitDialog);
		HANDLE_DLGMSG(hDlg, WM_COMMAND,		PathSettings_OnCommand);
	}
	return FALSE;
}



