/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright (C) 2009 DeSmuME team

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

#include "gbaslot_config.h"
#include <windowsx.h>
#include "resource.h"
#include "debug.h"
#include "../addons.h"
#include "../NDSSystem.h"
#include <shlobj.h>

WNDCLASSEX	wc;
HWND		wndConfig;
u8			temp_type = 0;
u8			last_type;
char		tmp_cflash_filename[MAX_PATH];
char		tmp_cflash_path[MAX_PATH];
char		tmp_gbagame_filename[MAX_PATH];
u8			tmp_CFlashUsePath;
u8			tmp_CFlashUseRomPath;
HWND		OKbutton = NULL;
bool		_OKbutton = false;

BOOL CALLBACK GbaSlotNone(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CALLBACK GbaSlotCFlash(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			SetWindowText(GetDlgItem(dialog, IDC_PATHIMG), tmp_cflash_filename);
			SetWindowText(GetDlgItem(dialog, IDC_PATH), tmp_cflash_path);
			if (tmp_CFlashUsePath)
			{
				if (tmp_CFlashUseRomPath)
				{
					CheckDlgButton(dialog, IDC_PATHDESMUME, BST_CHECKED);
					EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
					_OKbutton = TRUE;
				}
				else
				{
					EnableWindow(GetDlgItem(dialog, IDC_PATH), TRUE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), TRUE);
					if (strlen(tmp_cflash_path)) _OKbutton = TRUE;
				}
				EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), FALSE);
				EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), FALSE);
				CheckDlgButton(dialog, IDC_RFOLDER, BST_CHECKED);
			}
			else
			{
				EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
				EnableWindow(GetDlgItem(dialog, IDC_PATHDESMUME), FALSE);
				EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
				if (strlen(tmp_cflash_filename)) _OKbutton = TRUE;
				CheckDlgButton(dialog, IDC_RFILE, BST_CHECKED);
			}
			return TRUE;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_BBROWSE:
				{
					int filterSize = 0, i = 0;
                    OPENFILENAME ofn;
                    char filename[MAX_PATH] = "",
						 fileFilter[512]="";
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = dialog;

					strncpy (fileFilter, "Compact Flash image (*.img)|*.img||",512 - strlen(fileFilter));
					strncat (fileFilter, "Any file (*.*)|*.*||",512 - strlen(fileFilter));
					
					filterSize = strlen(fileFilter);
					for (i = 0; i < filterSize; i++)
						if (fileFilter[i] == '|')	fileFilter[i] = '\0';
                    ofn.lpstrFilter = fileFilter;
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFile =  filename;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrDefExt = "img";
					ofn.Flags = OFN_NOCHANGEDIR | OFN_CREATEPROMPT;
                    
                    if(!GetOpenFileName(&ofn)) return FALSE;

					SetWindowText(GetDlgItem(dialog, IDC_PATHIMG), filename);
					strcpy(tmp_cflash_filename, filename);
					if (!strlen(tmp_cflash_filename))
						EnableWindow(OKbutton, FALSE);
					else
						EnableWindow(OKbutton, TRUE);
					return FALSE;
				}

				case IDC_BBROWSE2:
				{
					BROWSEINFO bp={0};

					bp.hwndOwner=dialog;
					bp.pidlRoot=NULL;
					bp.pszDisplayName=NULL;
					bp.lpszTitle="Select directory for Compact Flash";
					bp.ulFlags=BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
					bp.lpfn=NULL;
	
					LPITEMIDLIST tmp = SHBrowseForFolder((LPBROWSEINFO)&bp);
					if (tmp!=NULL) 
					{
						memset(tmp_cflash_path, 0, sizeof(tmp_cflash_path));
						SHGetPathFromIDList(tmp, tmp_cflash_path);
						if (tmp_cflash_path[strlen(tmp_cflash_path)-1] != '\\')
							tmp_cflash_path[strlen(tmp_cflash_path)] = '\\';
						SetWindowText(GetDlgItem(dialog, IDC_PATH), tmp_cflash_path);
					}
					if (strlen(tmp_cflash_path))
							EnableWindow(OKbutton, TRUE);
						else
							EnableWindow(OKbutton, FALSE);
					break;
				}

				case IDC_RFILE:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						tmp_CFlashUsePath = FALSE;
						EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), TRUE);
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), TRUE);

						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_PATHDESMUME), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);

						if (!strlen(tmp_cflash_filename))
							EnableWindow(OKbutton, FALSE);
					}
					break;
				}

				case IDC_RFOLDER:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						tmp_CFlashUsePath = TRUE;
						EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), FALSE);

						if (IsDlgButtonChecked(dialog, IDC_PATHDESMUME))
						{
							tmp_CFlashUseRomPath = TRUE;
							EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
							EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
							EnableWindow(OKbutton, TRUE);
						}
						else
						{
							tmp_CFlashUseRomPath = FALSE;
							EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), TRUE);
							EnableWindow(GetDlgItem(dialog, IDC_PATH), TRUE);
						}

						EnableWindow(GetDlgItem(dialog, IDC_PATHDESMUME), TRUE);
					}
					break;
				}

				case IDC_PATHDESMUME:
				{
					if (IsDlgButtonChecked(dialog, IDC_PATHDESMUME))
					{
						tmp_CFlashUseRomPath = TRUE;
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
						EnableWindow(OKbutton, TRUE);
					}
					else
					{
						tmp_CFlashUseRomPath = FALSE;
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), TRUE);
						EnableWindow(GetDlgItem(dialog, IDC_PATH), TRUE);
						if (strlen(tmp_cflash_path))
							EnableWindow(OKbutton, TRUE);
						else
							EnableWindow(OKbutton, FALSE);
					}
					break;
				}
			}
			break;
		}
	}
	return FALSE;
}

BOOL CALLBACK GbaSlotRumblePak(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CALLBACK GbaSlotGBAgame(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			SetWindowText(GetDlgItem(dialog, IDC_PATHGAME), tmp_gbagame_filename);
			if (strlen(tmp_gbagame_filename) > 0) _OKbutton = true;
			return TRUE;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_BBROWSE:
					{
							int filterSize = 0, i = 0;
                            OPENFILENAME ofn;
                            char filename[MAX_PATH] = "",
								 fileFilter[512]="";
                            
                            ZeroMemory(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = dialog;

							// TODO: add another gba file formats and archs
							strncpy (fileFilter, "GameBoy Advance ROM (*.gba)|*.gba||",512 - strlen(fileFilter));
							strncat (fileFilter, "Any file (*.*)|*.*||",512 - strlen(fileFilter));
							
							filterSize = strlen(fileFilter);
							for (i = 0; i < filterSize; i++)
								if (fileFilter[i] == '|')	fileFilter[i] = '\0';
                            ofn.lpstrFilter = fileFilter;
                            ofn.nFilterIndex = 1;
                            ofn.lpstrFile =  filename;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.lpstrDefExt = "gba";
							ofn.Flags = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                            
                            if(!GetOpenFileName(&ofn)) return FALSE;

							SetWindowText(GetDlgItem(dialog, IDC_PATHGAME), filename);
							strcpy(tmp_gbagame_filename, filename);
							if (!strlen(tmp_gbagame_filename))
								EnableWindow(OKbutton, FALSE);
							else
								EnableWindow(OKbutton, TRUE);
						return FALSE;
					}
			}
			break;
		}
	}
	return FALSE;
}

u32		GBAslot_IDDs[NDS_ADDON_COUNT] = {
	IDD_GBASLOT_NONE,
	IDD_GBASLOT_CFLASH,
	IDD_GBASLOT_RUMBLEPAK,
	IDD_GBASLOT_GBAGAME
};

DLGPROC GBAslot_Procs[NDS_ADDON_COUNT] = {
	GbaSlotNone,
	GbaSlotCFlash,
	GbaSlotRumblePak,
	GbaSlotGBAgame
};


//==============================================================================
BOOL CALLBACK GbaSlotBox_Proc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			OKbutton = GetDlgItem(dialog, IDOK);
			for(int i = 0; i < NDS_ADDON_COUNT; i++)
				ComboBox_AddString(GetDlgItem(dialog, IDC_ADDONS_LIST), addonList[i].name);
			ComboBox_SetCurSel(GetDlgItem(dialog, IDC_ADDONS_LIST), temp_type);
			u8 tmp_info[512];
			addonList[temp_type].info((char *)tmp_info);
			SetWindowText(GetDlgItem(dialog, IDC_ADDONS_INFO), (char *)tmp_info);

			_OKbutton = false;
			wndConfig=CreateDialog(hAppInst, MAKEINTRESOURCE(GBAslot_IDDs[temp_type]), 
										dialog, (DLGPROC)GBAslot_Procs[temp_type]);
			if ( (temp_type == 0) || (_OKbutton) )
				EnableWindow(OKbutton, TRUE);
			else
				EnableWindow(OKbutton, FALSE);
			return TRUE;
		}
	
		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
					{
						int Msg = IDYES;
						if (romloaded)
						{
							int Msg = MessageBox(dialog, 
									"After change GBA slot pak game will reset!\nAre you sure to continue?", "DeSmuME",
									MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
						}
						if (Msg == IDYES)
						{
							if (wndConfig) DestroyWindow(wndConfig);
							EndDialog(dialog, TRUE);
						}
					}
				return TRUE;
				case IDCANCEL:
					if (wndConfig) DestroyWindow(wndConfig);
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_ADDONS_LIST:
					if (HIWORD(wparam) == CBN_SELENDOK)
					{
						temp_type = ComboBox_GetCurSel(GetDlgItem(dialog, IDC_ADDONS_LIST));
						if (temp_type != last_type)
						{
							if (wndConfig) DestroyWindow(wndConfig);
							_OKbutton = false;
							wndConfig=CreateDialog(hAppInst, 
								MAKEINTRESOURCE(GBAslot_IDDs[temp_type]), dialog, 
								(DLGPROC)GBAslot_Procs[temp_type]);
							if ( (temp_type == 0) || (_OKbutton) )
								EnableWindow(OKbutton, TRUE);
							else
								EnableWindow(OKbutton, FALSE);
							u8 tmp_info[512];
							addonList[temp_type].info((char *)tmp_info);
							SetWindowText(GetDlgItem(dialog, IDC_ADDONS_INFO), (char *)tmp_info);
							last_type = temp_type;
						}
					}
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

void GBAslotDialog(HWND hwnd)
{
	temp_type = addon_type;
	last_type = temp_type;
	strcpy(tmp_cflash_filename, CFlashName);
	strcpy(tmp_cflash_path, CFlashPath);
	strcpy(tmp_gbagame_filename, GBAgameName);
	tmp_CFlashUseRomPath = CFlashUseRomPath;
	tmp_CFlashUsePath = CFlashUsePath;
	_OKbutton = false;
	u32 res=DialogBox(hAppInst, MAKEINTRESOURCE(IDD_GBASLOT), hwnd, (DLGPROC) GbaSlotBox_Proc);
	if (res)
	{
		switch (temp_type)
		{
			case NDS_ADDON_NONE:
				break;
			case NDS_ADDON_CFLASH:
				CFlashUsePath = tmp_CFlashUsePath;
				WritePrivateProfileInt("GBAslot.CFlash","usePath",CFlashUsePath,IniName);
				if (tmp_CFlashUsePath)
				{
					if (tmp_CFlashUseRomPath)
					{
						CFlashUseRomPath = tmp_CFlashUseRomPath;
						WritePrivateProfileInt("GBAslot.CFlash","useRomPath",CFlashUseRomPath,IniName);
						break;
					}
					strcpy(CFlashPath, tmp_cflash_path);
					WritePrivateProfileString("GBAslot.CFlash","path",CFlashPath,IniName);
					break;
				}
				strcpy(CFlashName, tmp_cflash_filename);
				WritePrivateProfileString("GBAslot.CFlash","filename",CFlashName,IniName);
				break;
			case NDS_ADDON_RUMBLEPAK:
				break;
			case NDS_ADDON_GBAGAME:
				strcpy(GBAgameName, tmp_gbagame_filename);
				WritePrivateProfileString("GBAslot.GBAgame","filename",GBAgameName,IniName);
				break;
			default:
				return;
		}
		WritePrivateProfileInt("GBAslot","type",temp_type,IniName);
		addon_type = temp_type;
		addonsChangePak(addon_type);
		if (romloaded)
			NDS_Reset();
	}
}