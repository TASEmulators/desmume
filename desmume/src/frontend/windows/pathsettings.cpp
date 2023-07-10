/*
	Copyright (C) 2009 Hicoder
	Copyright (C) 2009-2011 DeSmuME team

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


#include <windowsx.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <time.h>
#include <stdlib.h>
#include "main.h"
#include "path.h"
#include "pathsettings.h"
#include "utils/xstring.h"

extern bool autoLoadLua;

#define HANDLE_DLGMSG(hwnd, message, fn)										\
	case (message): return (SetDlgMsgResult(hDlg, uMsg,							\
	HANDLE_##message( (hwnd), (wParam), (lParam), (fn) ) ) )

BOOL associate;
#define ASSOCIATEKEY "Associate"

void DoAssociations()
{
	string fileTypes[3] = { ".nds", ".ds.gba", ".srl" };
	string program = "Desmume.Emulator";
	string classes = "Software\\Classes";
	string defaultIcon = "DefaultIcon";
	string openVerb = "shell\\open\\command";
	string iconIndex = ", 0";
	string argument = " \"%1\"";

	HKEY user = NULL;
	if(RegOpenKeyEx(HKEY_CURRENT_USER, classes.c_str(),0, KEY_ALL_ACCESS, &user) == ERROR_SUCCESS)
	{
		if(associate)
		{
			HKEY icon, shell, registered;
			if(RegCreateKeyEx(user, program.c_str(), 0, 0, 0, KEY_ALL_ACCESS, NULL, &registered, 0) == ERROR_SUCCESS)
			{
				string module;				
				char buf[MAX_PATH];
				GetModuleFileName(NULL, buf, MAX_PATH);
				module.append(buf);

				if(RegCreateKeyEx(registered, defaultIcon.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &icon, 0) == ERROR_SUCCESS)
				{
					string iconPath = "\"";
					iconPath.append(module);
					iconPath.append("\"");
					iconPath.append(iconIndex);
					RegSetValueEx(icon, NULL, 0,REG_SZ, (const BYTE*)iconPath.c_str(), iconPath.size() + 1);
					RegCloseKey(icon);
				}

				if(RegCreateKeyEx(registered, openVerb.c_str(), 0, 0, 0, KEY_ALL_ACCESS, NULL, &shell, 0) == ERROR_SUCCESS)
				{
					string openPath = "\"";
					openPath.append(module);
					openPath.append("\"");
					openPath.append(argument);
					RegSetValueEx(shell, NULL, 0,REG_SZ, (const BYTE*)openPath.c_str(), openPath.size() + 1);
					RegCloseKey(shell);
				}
				RegCloseKey(registered);
			}

			for(int i = 0; i < ARRAY_SIZE(fileTypes); i++)
			{
				HKEY tmp;
				if(RegCreateKeyEx(user, fileTypes[i].c_str(), 0, 0, 0, KEY_ALL_ACCESS, NULL, &tmp, 0) == ERROR_SUCCESS)
				{
					RegSetValueEx(tmp, NULL, 0,REG_SZ, (const BYTE*)program.c_str(), ARRAY_SIZE(program) + 1);
				}
				RegCloseKey(tmp);
			}
		}
		else
		{
			SHDeleteKeyA(user, program.c_str());
			for(int i = 0; i < ARRAY_SIZE(fileTypes); i++)
				RegDeleteKey(user, fileTypes[i].c_str());
		}
	}

	if(user != NULL)
		RegCloseKey(user);
}

void WritePathSettings()
{
	WritePrivateProfileStringW(LSECTION, ROMKEY, mbstowcs((std::string)path.pathToRoms).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, BATTERYKEY, mbstowcs((std::string)path.pathToBattery).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, SRAMIMPORTKEY, mbstowcs((std::string)path.pathToSramImportExport).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, STATEKEY, mbstowcs((std::string)path.pathToStates).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, STATESLOTKEY, mbstowcs((std::string)path.pathToStateSlots).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, SCREENSHOTKEY, mbstowcs((std::string)path.pathToScreenshots).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, AVIKEY, mbstowcs((std::string)path.pathToAviFiles).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, CHEATKEY, mbstowcs((std::string)path.pathToCheats).c_str(), IniNameW);
	WritePrivateProfileInt(SECTION, R4FORMATKEY, path.r4Format, IniName);
	WritePrivateProfileStringW(LSECTION, SOUNDKEY, mbstowcs((std::string)path.pathToSounds).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, FIRMWAREKEY, mbstowcs((std::string)path.pathToFirmware).c_str(), IniNameW);
	WritePrivateProfileStringW(LSECTION, LUAKEY, mbstowcs((std::string)path.pathToLua).c_str(), IniNameW);

	WritePrivateProfileInt(SECTION, DEFAULTFORMATKEY, path.currentimageformat, IniName);
	WritePrivateProfileString(SECTION, FORMATKEY, path.screenshotFormat, IniName);

	WritePrivateProfileInt(SECTION, LASTVISITKEY, path.savelastromvisit, IniName);
	WritePrivateProfileInt(SECTION, ASSOCIATEKEY, associate, IniName);
	WritePrivateProfileBool("Scripting", "AutoLoad", autoLoadLua, IniName);

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

	associate = GetPrivateProfileInt(SECTION, ASSOCIATEKEY, 0, IniName);

	CheckDlgButton(hDlg, IDC_USELASTVISIT, (path.savelastromvisit) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_ASSOCIATE, (associate) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hDlg, IDC_AUTOLOADLUA, (autoLoadLua) ? BST_CHECKED : BST_UNCHECKED);
	static const int imageFormatMap[] = { IDC_PNG, IDC_BMP };
	static const int r4TypeMap[] = { IDC_R4TYPE1, IDC_R4TYPE2 };
	CheckRadioButton(hDlg, IDC_PNG, IDC_BMP, imageFormatMap[(int)path.imageformat()]);
	CheckRadioButton(hDlg, IDC_R4TYPE1, IDC_R4TYPE2, r4TypeMap[(int)path.r4Format]);

// IDC_FORMATEDIT setup
	SetDlgItemText(hDlg, IDC_FORMATEDIT, path.screenshotFormat);

	hwnd = GetDlgItem(hDlg, IDC_FORMATEDIT);
	Edit_LimitText(hwnd, MAX_FORMAT);

	HWND toolTip = CreateWindowExW(NULL, 
		TOOLTIPS_CLASSW, NULL, 
		TTS_ALWAYSTIP, 
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		hDlg, NULL, 
		hAppInst, NULL);
	SendMessage(toolTip, TTM_SETMAXTIPWIDTH, NULL, (LPARAM)330);

	SetWindowPos(toolTip, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	SetDlgItemTextW(hDlg, IDC_ROMPATHEDIT, mbstowcs(path.pathToRoms).c_str());
	SetDlgItemTextW(hDlg, IDC_SAVERAMPATHEDIT, mbstowcs(path.pathToBattery).c_str());
	SetDlgItemTextW(hDlg, IDC_SRAMIMPORTPATHEDIT, mbstowcs(path.pathToSramImportExport).c_str());
	SetDlgItemTextW(hDlg, IDC_STATEPATHEDIT, mbstowcs(path.pathToStates).c_str());
	SetDlgItemTextW(hDlg, IDC_STATESLOTPATHEDIT, mbstowcs(path.pathToStateSlots).c_str());
	SetDlgItemTextW(hDlg, IDC_SCREENSHOTPATHEDIT, mbstowcs(path.pathToScreenshots).c_str());
	SetDlgItemTextW(hDlg, IDC_AVIPATHEDIT, mbstowcs(path.pathToAviFiles).c_str());
	SetDlgItemTextW(hDlg, IDC_CHEATPATHEDIT, mbstowcs(path.pathToCheats).c_str());
	SetDlgItemTextW(hDlg, IDC_LUAPATHEDIT, mbstowcs(path.pathToLua).c_str());

	TOOLINFO ti;
	ZeroMemory(&ti, sizeof(ti));
	ti.cbSize = sizeof(ti);
	ti.hwnd = hDlg;
	ti.hinst = hAppInst;
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	ti.uId = (UINT_PTR)hwnd;
	ti.lpszText =
		"The string format a screenshot should be saved with (google strftime).\r\n"
		"%f\t\tFilename\r\n"
		"%r\t\tRandom: 0 ~ RAND_MAX\r\n"
		"%t\t\tTick: Reset on startup\r\n"
		"%Y\t\tYear:Four Digit\r\n"
		"%y\t\tYear:Two Digit\r\n"
		"%m\t\tMonth:Two Digit\r\n"
		"%d\t\tDay:Two Digit\r\n"
		"%H\t\tHour (24):Two Digit\r\n"
		"%I\t\tHour (12):Two Digit\r\n"
		"%p\t\tAM/PM\r\n"
		"%M\t\tMinute: Two Digit\r\n"
		"%S\t\tSecond: Two Digit\r\n";
	GetClientRect(hwnd, &ti.rect);
	SendMessage(toolTip, TTM_ADDTOOL, NULL, (LPARAM)&ti);

	return TRUE;
}

BOOL BrowseForPath(char *pathToBrowse)
{
	LPMALLOC shMalloc;
	BOOL changed = false;
	LPITEMIDLIST idList;
	BROWSEINFOW bi;

	//stupid shell
	if(SHGetMalloc( &shMalloc) != S_OK)
		return FALSE;

	ZeroMemory(&idList, sizeof(idList));
	ZeroMemory(&bi, sizeof(bi));

	char tmp[MAX_PATH];
	strncpy(tmp, pathToBrowse, MAX_PATH);

	bi.hwndOwner = MainWindow->getHWnd();
	bi.lpszTitle = L"Choose a Folder";
	bi.ulFlags = BIF_NONEWFOLDERBUTTON | BIF_USENEWUI;

	/*wanted to add a callback function for the folder initialization but it crashes everytime i do
	bi.lpfn = (BFFCALLBACK)InitialFolder;
	bi.lParam = (LPARAM)pathToBrowse;
	*/

	if( (idList = SHBrowseForFolderW(&bi)) )
	{
		changed = true;
		wchar_t wPathToBrowse[MAX_PATH];
		SHGetPathFromIDListW(idList, wPathToBrowse);
		strcpy(pathToBrowse,wcstombs((std::wstring)wPathToBrowse).c_str());
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
					SetDlgItemTextW(hDlg, IDC_ROMPATHEDIT, mbstowcs((std::string)path.pathToRoms).c_str());
			}
			break;
		case IDC_BROWSESRAM:
			{
				if(BrowseForPath(path.pathToBattery))
					SetDlgItemTextW(hDlg, IDC_SAVERAMPATHEDIT, mbstowcs((std::string)path.pathToBattery).c_str());
			}
			break;
		case IDC_BROWSESRAMIMPORT:
			{
				if(BrowseForPath(path.pathToSramImportExport))
					SetDlgItemTextW(hDlg, IDC_SRAMIMPORTPATHEDIT, mbstowcs((std::string)path.pathToSramImportExport).c_str());
			}
			break;
		case IDC_BROWSESTATES:
			{
				if(BrowseForPath(path.pathToStates))
					SetDlgItemTextW(hDlg, IDC_STATEPATHEDIT, mbstowcs((std::string)path.pathToStates).c_str());
			}
			break;
		case IDC_BROWSESTATESLOTS:
			{
				if(BrowseForPath(path.pathToStateSlots))
					SetDlgItemTextW(hDlg, IDC_STATESLOTPATHEDIT, mbstowcs((std::string)path.pathToStateSlots).c_str());
			}
			break;
		case IDC_BROWSESCREENSHOTS:
			{
				if(BrowseForPath(path.pathToScreenshots))
					SetDlgItemTextW(hDlg, IDC_SCREENSHOTPATHEDIT, mbstowcs((std::string)path.pathToScreenshots).c_str());
			}
			break;
		case IDC_BROWSEAVI:
			{
				if(BrowseForPath(path.pathToAviFiles))
					SetDlgItemTextW(hDlg, IDC_AVIPATHEDIT, mbstowcs((std::string)path.pathToAviFiles).c_str());
			}
			break;
		case IDC_BROWSECHEATS:
			{
				if(BrowseForPath(path.pathToCheats))
					SetDlgItemTextW(hDlg, IDC_CHEATPATHEDIT, mbstowcs((std::string)path.pathToCheats).c_str());
			}
			break;
		case IDC_R4TYPE1:
				CheckRadioButton(hDlg, IDC_R4TYPE1, IDC_R4TYPE2, IDC_R4TYPE1);
				path.r4Format = path.R4_CHEAT_DAT;
			break;
		case IDC_R4TYPE2:
				CheckRadioButton(hDlg, IDC_R4TYPE1, IDC_R4TYPE2, IDC_R4TYPE2);
				path.r4Format = path.R4_USRCHEAT_DAT;
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
		case IDC_AUTOLOADLUA:
				autoLoadLua = !autoLoadLua;
			break;
		case IDOK:

			wchar_t wtmp[MAX_PATH];
			GetDlgItemTextW(hDlg, IDC_ROMPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToRoms,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_SAVERAMPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToBattery,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_SRAMIMPORTPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToSramImportExport,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_STATEPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToStates,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_STATESLOTPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToStateSlots,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_SCREENSHOTPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToScreenshots,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_AVIPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToAviFiles,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_CHEATPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToCheats,wcstombs((std::wstring)wtmp).c_str()); 
			GetDlgItemTextW(hDlg, IDC_LUAPATHEDIT, wtmp, MAX_PATH); strcpy(path.pathToLua,wcstombs((std::wstring)wtmp).c_str()); 
			DoAssociations();
			WritePathSettings();
			EndDialog(hDlg, 0);
			break;
		case IDCANCEL:
			path.ReadPathSettings();
			EndDialog(hDlg, 0);
			break;
		case IDC_ASSOCIATE:
			associate = !associate;
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