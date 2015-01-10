/*
	Copyright (C) 2011-2015 DeSmuME team

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

#include "slot1_config.h"

#include <windowsx.h>
#include <shlobj.h>
#include <string>

#include "../slot1.h"
#include "../debug.h"
#include "../NDSSystem.h"
#include "../path.h"

#include "resource.h"
#include "main.h"

HWND wndConfigSlot1 = NULL;
NDS_SLOT1_TYPE temp_type_slot1 = NDS_SLOT1_NONE;
NDS_SLOT1_TYPE last_type_slot1 = NDS_SLOT1_NONE;
char tmp_fat_path[MAX_PATH] = {0};
char tmp_fs_path[MAX_PATH] = {0};
bool tmp_fat_path_type = false;
HWND OKbutton_slot1 = NULL;
bool _OKbutton_slot1 = false;
bool needReset_slot1 = true;

#define SLOT1_DEBUG_ID 20000
#define SLOT1_R4_ID 20001

INT CALLBACK Slot1_BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData) 
{
	TCHAR szDir[MAX_PATH];

	switch(uMsg)
	{
		case BFFM_INITIALIZED: 
			if (pData == SLOT1_DEBUG_ID)
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)tmp_fs_path);
			else
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)tmp_fat_path);
		break;

		case BFFM_SELCHANGED: 
			if (SHGetPathFromIDList((LPITEMIDLIST) lp, szDir))
			{
				SendMessage(hwnd,BFFM_SETSTATUSTEXT, 0, (LPARAM)szDir);
			}
		break;
	}
	return 0;
}


INT_PTR CALLBACK Slot1None(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton_slot1 = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

INT_PTR CALLBACK Slot1Debug(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton_slot1 = TRUE;

			SetWindowText(GetDlgItem(dialog, IDC_DIRECTORY_SCAN), tmp_fs_path);
			return TRUE;
		}

		
		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_BROWSE:
				{
					BROWSEINFO bp={0};

					bp.hwndOwner=dialog;
					bp.pidlRoot=NULL;
					bp.pszDisplayName=NULL;
					bp.lpszTitle="Select directory for game files";
					bp.ulFlags=BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
					bp.lParam = SLOT1_DEBUG_ID;
					bp.lpfn=Slot1_BrowseCallbackProc;
	
					LPITEMIDLIST tmp = SHBrowseForFolder((LPBROWSEINFO)&bp);
					if (tmp!=NULL) 
					{
						memset(tmp_fs_path, 0, sizeof(tmp_fs_path));
						SHGetPathFromIDList(tmp, tmp_fs_path);
						if (tmp_fs_path[strlen(tmp_fs_path)-1] != '\\')
							tmp_fs_path[strlen(tmp_fs_path)] = '\\';
						SetWindowText(GetDlgItem(dialog, IDC_DIRECTORY_SCAN), tmp_fs_path);
					}
					if (strlen(tmp_fs_path))
							EnableWindow(OKbutton_slot1, TRUE);
						else
							EnableWindow(OKbutton_slot1, FALSE);
					break;
				}
			}
		}
	}
	return FALSE;
}

INT_PTR CALLBACK Slot1R4(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			SetWindowText(GetDlgItem(dialog, IDC_PATH), tmp_fat_path);
			_OKbutton_slot1 = (tmp_fat_path!="");

			if (!tmp_fat_path_type)
			{
				CheckDlgButton(dialog, IDC_R4_FOLDER, BST_CHECKED);
				EnableWindow(GetDlgItem(dialog, IDC_BROWSE), true);
				EnableWindow(GetDlgItem(dialog, IDC_PATH), true);
			}
			else
			{
				CheckDlgButton(dialog, IDC_R4_ROM, BST_CHECKED);
				EnableWindow(GetDlgItem(dialog, IDC_BROWSE), false);
				EnableWindow(GetDlgItem(dialog, IDC_PATH), false);
			}
			return FALSE;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_BROWSE:
				{
					BROWSEINFO bp={0};

					bp.hwndOwner=dialog;
					bp.pidlRoot=NULL;
					bp.pszDisplayName=NULL;
					bp.lpszTitle="Select directory for FAT image building";
					bp.ulFlags=BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
					bp.lParam = SLOT1_R4_ID;
					bp.lpfn=Slot1_BrowseCallbackProc;
	
					LPITEMIDLIST tmp = SHBrowseForFolder((LPBROWSEINFO)&bp);
					if (tmp!=NULL) 
					{
						memset(tmp_fat_path, 0, sizeof(tmp_fat_path));
						SHGetPathFromIDList(tmp, tmp_fat_path);
						if (tmp_fat_path[strlen(tmp_fat_path)-1] != '\\')
							tmp_fat_path[strlen(tmp_fat_path)] = '\\';
						SetWindowText(GetDlgItem(dialog, IDC_PATH), tmp_fat_path);
					}
					if (strlen(tmp_fat_path))
							EnableWindow(OKbutton_slot1, TRUE);
						else
							EnableWindow(OKbutton_slot1, FALSE);
					break;
				}

				case IDC_R4_FOLDER:
					EnableWindow(GetDlgItem(dialog, IDC_BROWSE), true);
					EnableWindow(GetDlgItem(dialog, IDC_PATH), true);
					tmp_fat_path_type = false;
					return TRUE;

				case IDC_R4_ROM:
					EnableWindow(GetDlgItem(dialog, IDC_BROWSE), false);
					EnableWindow(GetDlgItem(dialog, IDC_PATH), false);
					tmp_fat_path_type = true;
					return TRUE;
			}
		}
	}
	return FALSE;
}

u32	Slot1_IDDs[NDS_SLOT1_COUNT] = {
	IDD_SLOT1_NONE,
	IDD_SLOT1_NONE,				// NDS_SLOT1_RETAIL_AUTO	- autodetect which kind of retail card to use 
	IDD_SLOT1_R4,				// NDS_SLOT1_R4,			- R4 flash card
	IDD_SLOT1_NONE,				// NDS_SLOT1_RETAIL_NAND	- Made in Ore/WarioWare D.I.Y.
	IDD_SLOT1_NONE,				// NDS_SLOT1_RETAIL_MCROM	- a standard MC (eeprom, flash, fram)
	IDD_SLOT1_DEBUG,			// NDS_SLOT1_RETAIL_DEBUG	- for romhacking and fan-made translations
};

DLGPROC Slot1_Procs[NDS_SLOT1_COUNT] = {
	Slot1None,
	Slot1None,					// NDS_SLOT1_RETAIL_AUTO	- autodetect which kind of retail card to use 
	Slot1R4,					// NDS_SLOT1_R4,			- R4 flash card
	Slot1None,  				// NDS_SLOT1_RETAIL_NAND	- Made in Ore/WarioWare D.I.Y.
	Slot1None,					// NDS_SLOT1_RETAIL_MCROM	- a standard MC (eeprom, flash, fram)
	Slot1Debug					// NDS_SLOT1_RETAIL_DEBUG	- for romhacking and fan-made translations
};


//==============================================================================
BOOL CALLBACK Slot1Box_Proc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			OKbutton_slot1 = GetDlgItem(dialog, IDOK);
			for(int i = 0; i < NDS_SLOT1_COUNT; i++)
				ComboBox_AddString(GetDlgItem(dialog, IDC_ADDONS_LIST), slot1_List[i]->info()->name());
			ComboBox_SetCurSel(GetDlgItem(dialog, IDC_ADDONS_LIST), temp_type_slot1);
			SetWindowText(GetDlgItem(dialog, IDC_ADDONS_INFO), slot1_List[temp_type_slot1]->info()->descr());

			_OKbutton_slot1 = false;
			wndConfigSlot1 = CreateDialogW(hAppInst, MAKEINTRESOURCEW(Slot1_IDDs[temp_type_slot1]), dialog, (DLGPROC)Slot1_Procs[temp_type_slot1]);
			//SetWindowPos(GetDlgItem(dialog, IDC_ADDONS_INFO),HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
			//EnableWindow(GetDlgItem(dialog, IDC_ADDONS_INFO),FALSE);
			//EnableWindow(GetDlgItem(dialog, IDC_ADDONS_INFO),TRUE);
			if ( (temp_type_slot1 == 0) || (_OKbutton_slot1) )
				EnableWindow(OKbutton_slot1, TRUE);
			else
				EnableWindow(OKbutton_slot1, FALSE);

			return TRUE;
		}
	
		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
					{
						int Msg = IDYES;
						//zero 30-aug-2012 - do we really need to reset whenever we change the slot 1 device? why should resetting matter? if thats what you want to do, then do it.
						//if (romloaded && (needReset_slot1 || (temp_type_slot1!=slot1_device_type)) )
						//{
						//	Msg = MessageBox(dialog, 
						//			"After change slot1 device game will reset!\nAre you sure to continue?", "DeSmuME",
						//			MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
						//}
						if (Msg == IDYES)
						{
							if (wndConfigSlot1) DestroyWindow(wndConfigSlot1);
							EndDialog(dialog, TRUE);
						}
					}
				return TRUE;
				case IDCANCEL:
					if (wndConfigSlot1) DestroyWindow(wndConfigSlot1);
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_ADDONS_LIST:
					if (HIWORD(wparam) == CBN_SELENDOK)
					{
						temp_type_slot1 = (NDS_SLOT1_TYPE)ComboBox_GetCurSel(GetDlgItem(dialog, IDC_ADDONS_LIST));
						if (temp_type_slot1 != last_type_slot1)
						{
							if (wndConfigSlot1) 
							{
								DestroyWindow(wndConfigSlot1);
								wndConfigSlot1 = NULL;
							}
							_OKbutton_slot1 = false;
							wndConfigSlot1=CreateDialogW(hAppInst, 
								MAKEINTRESOURCEW(Slot1_IDDs[temp_type_slot1]), dialog, 
								(DLGPROC)Slot1_Procs[temp_type_slot1]);
							if ( (temp_type_slot1 == 0) || (_OKbutton_slot1) )
								EnableWindow(OKbutton_slot1, TRUE);
							else
								EnableWindow(OKbutton_slot1, FALSE);
							SetWindowText(GetDlgItem(dialog, IDC_ADDONS_INFO), slot1_List[temp_type_slot1]->info()->descr());
							last_type_slot1 = temp_type_slot1;
						}
					}
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

void slot1Dialog(HWND hwnd)
{
	strcpy(tmp_fat_path, slot1_GetFatDir().c_str());
	strcpy(tmp_fs_path, path.getpath(path.SLOT1D).c_str());
	temp_type_slot1 = slot1_GetCurrentType();
	last_type_slot1 = temp_type_slot1;
	tmp_fat_path_type = slot1_R4_path_type;
	_OKbutton_slot1 = false;
	needReset_slot1 = true;

	u32 res=DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_SLOT1CONFIG), hwnd, (DLGPROC)Slot1Box_Proc);
	if (res)
	{
		switch (temp_type_slot1)
		{
			case NDS_SLOT1_NONE:
				if (temp_type_slot1 != slot1_GetCurrentType())
					needReset_slot1 = true;
				else
					needReset_slot1 = false;
				break;
			case NDS_SLOT1_RETAIL_AUTO:
			case NDS_SLOT1_RETAIL_MCROM:
				break;
			case NDS_SLOT1_R4:
				WritePrivateProfileBool("Slot1","FAT_path_type",tmp_fat_path_type,IniName);
				if (tmp_fat_path_type)
				{
					slot1_SetFatDir(slot1_GetFatDir(), true);
				}
				else
				{
					slot1_SetFatDir(tmp_fat_path, false);
					WritePrivateProfileString("Slot1","FAT_path",tmp_fat_path,IniName);
				}
				break;
			case NDS_SLOT1_RETAIL_NAND:
				break;
			case NDS_SLOT1_RETAIL_DEBUG:
				if (strlen(tmp_fs_path))
				{
					path.setpath(path.SLOT1D, tmp_fs_path);
					WritePrivateProfileString(SECTION, SLOT1DKEY, path.pathToSlot1D, IniName);
				}
				break;
			default:
				return;
		}
		
		slot1_Change((NDS_SLOT1_TYPE)temp_type_slot1);
		WritePrivateProfileInt("Slot1", "id", slot1_List[temp_type_slot1]->info()->id(), IniName);
		
		return;
	}
}