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

#include "common.h"
#include <windowsx.h>
#include <shlobj.h>
#include <commctrl.h>
#include <time.h>
#include <stdlib.h>
#include "main.h"
#include "path.h"
#include "pathsettings.h"

#define HANDLE_DLGMSG(hwnd, message, fn)										\
	case (message): return (SetDlgMsgResult(hDlg, uMsg,							\
	HANDLE_##message( (hwnd), (wParam), (lParam), (fn) ) ) )


void WritePathSettings()
{
	WritePrivateProfileString(SECTION, ROMKEY, path.pathToRoms, IniName);
	WritePrivateProfileString(SECTION, BATTERYKEY, path.pathToBattery, IniName);
	WritePrivateProfileString(SECTION, STATEKEY, path.pathToStates, IniName);
	WritePrivateProfileString(SECTION, SCREENSHOTKEY, path.pathToScreenshots, IniName);
	WritePrivateProfileString(SECTION, AVIKEY, path.pathToAviFiles, IniName);
	WritePrivateProfileString(SECTION, CHEATKEY, path.pathToCheats, IniName);
	WritePrivateProfileString(SECTION, SOUNDKEY, path.pathToSounds, IniName);
	WritePrivateProfileString(SECTION, FIRMWAREKEY, path.pathToFirmware, IniName);
	WritePrivateProfileString(SECTION, LUAKEY, path.pathToLua, IniName);

	WritePrivateProfileString(SECTION, FORMATKEY, path.screenshotFormat, IniName);

	WritePrivateProfileInt(SECTION, LASTVISITKEY, path.savelastromvisit, IniName);
//	WritePrivateProfileInt(SECTION, DEFAULTFORMATKEY, defaultFormat, IniName);
//	WritePrivateProfileInt(SECTION, NEEDSSAVINGKEY, needsSaving, IniName);
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
//	PathSettings_OnSelChange(hDlg, NULL);

	CheckDlgButton(hDlg, IDC_USELASTVISIT, (path.savelastromvisit) ? BST_CHECKED : BST_UNCHECKED);
	CheckRadioButton(hDlg, IDC_PNG, IDC_BMP, (int)path.imageformat());

// IDC_FORMATEDIT setup
	SetDlgItemText(hDlg, IDC_FORMATEDIT, path.screenshotFormat);

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

	SetDlgItemText(hDlg, IDC_ROMPATHEDIT, path.pathToRoms);
	SetDlgItemText(hDlg, IDC_SAVERAMPATHEDIT, path.pathToBattery);
	SetDlgItemText(hDlg, IDC_STATEPATHEDIT, path.pathToStates);
	SetDlgItemText(hDlg, IDC_SCREENSHOTPATHEDIT, path.pathToScreenshots);
	SetDlgItemText(hDlg, IDC_AVIPATHEDIT, path.pathToAviFiles);
	SetDlgItemText(hDlg, IDC_CHEATPATHEDIT, path.pathToCheats);
	SetDlgItemText(hDlg, IDC_LUAPATHEDIT, path.pathToLua);

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


void PathSettings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
	switch(id)
	{
		case IDC_BROWSEROMS:
			{
				if(BrowseForPath(path.pathToRoms))
					SetDlgItemText(hDlg, IDC_ROMPATHEDIT, path.pathToRoms);
			}
			break;
		case IDC_BROWSESRAM:
			{
				if(BrowseForPath(path.pathToBattery))
					SetDlgItemText(hDlg, IDC_SAVERAMPATHEDIT, path.pathToBattery);
			}
			break;
		case IDC_BROWSESTATES:
			{
				if(BrowseForPath(path.pathToStates))
					SetDlgItemText(hDlg, IDC_STATEPATHEDIT, path.pathToStates);
			}
			break;
		case IDC_BROWSESCREENSHOTS:
			{
				if(BrowseForPath(path.pathToScreenshots))
					SetDlgItemText(hDlg, IDC_SCREENSHOTPATHEDIT, path.pathToScreenshots);
			}
			break;
		case IDC_BROWSEAVI:
			{
				if(BrowseForPath(path.pathToAviFiles))
					SetDlgItemText(hDlg, IDC_AVIPATHEDIT, path.pathToAviFiles);
			}
			break;
		case IDC_BROWSECHEATS:
			{
				if(BrowseForPath(path.pathToCheats))
					SetDlgItemText(hDlg, IDC_CHEATPATHEDIT, path.pathToCheats);
			}
			break;
		case IDC_BROWSELUA:
			{
				if(BrowseForPath(path.pathToLua))
					SetDlgItemText(hDlg, IDC_LUAPATHEDIT, path.pathToLua);
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
				path.currentimageformat = path.PNG;
			break;
		case IDC_BMP:
				CheckRadioButton(hDlg, IDC_PNG, IDC_BMP, IDC_BMP);
				path.currentimageformat = path.BMP;
			break;
		case IDC_USELASTVISIT:
			{
				path.savelastromvisit = !path.savelastromvisit;
				CheckDlgButton(hDlg, IDC_USELASTVISIT, (path.savelastromvisit) ? BST_CHECKED : BST_UNCHECKED);
			}
			break;
		case IDC_PATHLIST:
			{
				if(codeNotify == CBN_SELCHANGE)
				{
//					PathSettings_OnSelChange(hDlg, hwndCtl);
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
					strncpy(path.screenshotFormat, buffer, MAX_FORMAT);
				}
			}
			break;
		case IDOK:

			GetDlgItemText(hDlg, IDC_ROMPATHEDIT, path.pathToRoms, MAX_PATH);
			GetDlgItemText(hDlg, IDC_SAVERAMPATHEDIT, path.pathToBattery, MAX_PATH);
			GetDlgItemText(hDlg, IDC_STATEPATHEDIT, path.pathToStates, MAX_PATH);
			GetDlgItemText(hDlg, IDC_SCREENSHOTPATHEDIT, path.pathToScreenshots, MAX_PATH);
			GetDlgItemText(hDlg, IDC_AVIPATHEDIT, path.pathToAviFiles, MAX_PATH);
			GetDlgItemText(hDlg, IDC_CHEATPATHEDIT, path.pathToCheats, MAX_PATH);
			GetDlgItemText(hDlg, IDC_LUAPATHEDIT, path.pathToLua, MAX_PATH);

			WritePathSettings();
			EndDialog(hDlg, 0);
			break;
		case IDCANCEL:
			path.ReadPathSettings();
			EndDialog(hDlg, 0);
			break;
	}
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