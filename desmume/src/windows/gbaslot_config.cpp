/*  Copyright (C) 2009 DeSmuME team

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
#include "inputdx.h"
#include <shlobj.h>

WNDCLASSEX	wc;
HWND		wndConfig = NULL;
u8			temp_type = 0;
u8			last_type = 0;
char		tmp_cflash_filename[MAX_PATH] = { 0 };
char		tmp_cflash_path[MAX_PATH] = { 0 };
char		tmp_gbagame_filename[MAX_PATH] = { 0 };
ADDON_CFLASH_MODE	tmp_CFlashMode = ADDON_CFLASH_MODE_RomPath;
HWND		OKbutton = NULL;
bool		_OKbutton = false;
SGuitar		tmp_Guitar;
bool		needReset = true;

std::string CFlashPath, CFlashName;

INT_PTR CALLBACK GbaSlotNone(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
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

INT_PTR CALLBACK GbaSlotCFlash(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			switch (tmp_CFlashMode)
			{
				case ADDON_CFLASH_MODE_Path:
					SetFocus(GetDlgItem(dialog,IDC_RFOLDER));
					CheckDlgButton(dialog, IDC_RFOLDER, BST_CHECKED);
					EnableWindow(GetDlgItem(dialog, IDC_PATH), TRUE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), TRUE);
					EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), FALSE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), FALSE);
					if (strlen(tmp_cflash_path)) _OKbutton = TRUE;
				break;

				case ADDON_CFLASH_MODE_File:
					SetFocus(GetDlgItem(dialog,IDC_RFILE));
					CheckDlgButton(dialog, IDC_RFILE, BST_CHECKED);
					EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), TRUE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), TRUE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
					EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
					if (strlen(tmp_cflash_filename)) _OKbutton = TRUE;
				break;

				case ADDON_CFLASH_MODE_RomPath:
					SetFocus(GetDlgItem(dialog,IDC_PATHDESMUME));
					CheckDlgButton(dialog, IDC_PATHDESMUME, BST_CHECKED);
					EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
					EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), FALSE);
					EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), FALSE);
					_OKbutton = TRUE;
				break;
			}
			SetWindowText(GetDlgItem(dialog, IDC_PATHIMG), tmp_cflash_filename);
			SetWindowText(GetDlgItem(dialog, IDC_PATH), tmp_cflash_path);
			return FALSE;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_BBROWSE:
				{
					int filterSize = 0, i = 0;
                    OPENFILENAME ofn;
                    char filename[MAX_PATH] = "";

                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = dialog;

					const char *fileFilter = "Compact Flash image (*.img)\0*.img\0Any file (*.*)\0*.*\0";
					
                    ofn.lpstrFilter = fileFilter;
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFile =  filename;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrDefExt = "img";
					ofn.Flags = OFN_NOCHANGEDIR | OFN_CREATEPROMPT | OFN_PATHMUSTEXIST;
                    
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
					bp.ulFlags=BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
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
						tmp_CFlashMode = ADDON_CFLASH_MODE_File;
						EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), TRUE);
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), TRUE);

						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
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
						tmp_CFlashMode = ADDON_CFLASH_MODE_Path;
						EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), FALSE);

						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), TRUE);
						EnableWindow(GetDlgItem(dialog, IDC_PATH), TRUE);
						if (!strlen(tmp_cflash_path))
							EnableWindow(OKbutton, FALSE);
					}
					break;
				}

				case IDC_PATHDESMUME:
				{
					if (HIWORD(wparam) == BN_CLICKED)
					{
						tmp_CFlashMode = ADDON_CFLASH_MODE_RomPath;
						EnableWindow(GetDlgItem(dialog, IDC_PATHIMG), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE), FALSE);

						EnableWindow(GetDlgItem(dialog, IDC_BBROWSE2), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_PATH), FALSE);
						EnableWindow(OKbutton, TRUE);
					}
					break;
				}
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR CALLBACK GbaSlotRumblePak(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
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

INT_PTR CALLBACK GbaSlotGBAgame(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
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
                            char filename[MAX_PATH] = "";
                            
                            ZeroMemory(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = dialog;

							// TODO: add another gba file formats and archs (??wtf??)
							const char* fileFilter =	"GameBoy Advance ROM (*.gba)\0*.gba\0"
														"NDS ROM (for nitroFS roms) (*.nds)\0*.nds\0"
														"Any file (*.*)\0*.*\0";
							
                            ofn.lpstrFilter = fileFilter;
                            ofn.nFilterIndex = 1;
                            ofn.lpstrFile =  filename;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.lpstrDefExt = "gba";
							ofn.Flags = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;
                            
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

INT_PTR CALLBACK GbaSlotGuitarGrip(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	int which = 0;

	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton = TRUE;
			SendDlgItemMessage(dialog,IDC_GGREEN,WM_USER+44,tmp_Guitar.GREEN,0);
			SendDlgItemMessage(dialog,IDC_GRED,WM_USER+44,tmp_Guitar.RED,0);
			SendDlgItemMessage(dialog,IDC_GYELLOW,WM_USER+44,tmp_Guitar.YELLOW,0);
			SendDlgItemMessage(dialog,IDC_GBLUE,WM_USER+44,tmp_Guitar.BLUE,0);
			if (temp_type != addon_type)
				needReset = true;
			else
				needReset = false;

			return TRUE;
		}

		case WM_USER+46:
			SendDlgItemMessage(dialog,IDC_GGREEN,WM_USER+44,tmp_Guitar.GREEN,0);
			SendDlgItemMessage(dialog,IDC_GRED,WM_USER+44,tmp_Guitar.RED,0);
			SendDlgItemMessage(dialog,IDC_GYELLOW,WM_USER+44,tmp_Guitar.YELLOW,0);
			SendDlgItemMessage(dialog,IDC_GBLUE,WM_USER+44,tmp_Guitar.BLUE,0);
		return TRUE;

		case WM_USER+43:
			//MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
			which = GetDlgCtrlID((HWND)lparam);
			switch(which)
			{
			case IDC_GGREEN:
				tmp_Guitar.GREEN = wparam;

				break;
			case IDC_GRED:
				tmp_Guitar.RED = wparam;

				break;
			case IDC_GYELLOW:
				tmp_Guitar.YELLOW = wparam;

				break;
			case IDC_GBLUE:
				tmp_Guitar.BLUE = wparam;
				break;
			}

			SendDlgItemMessage(dialog,IDC_GGREEN,WM_USER+44,tmp_Guitar.GREEN,0);
			SendDlgItemMessage(dialog,IDC_GRED,WM_USER+44,tmp_Guitar.RED,0);
			SendDlgItemMessage(dialog,IDC_GYELLOW,WM_USER+44,tmp_Guitar.YELLOW,0);
			SendDlgItemMessage(dialog,IDC_GBLUE,WM_USER+44,tmp_Guitar.BLUE,0);
			PostMessage(dialog,WM_NEXTDLGCTL,0,0);
		return true;
	}
	return FALSE;
}

u32		GBAslot_IDDs[NDS_ADDON_COUNT] = {
	IDD_GBASLOT_NONE,
	IDD_GBASLOT_CFLASH,
	IDD_GBASLOT_RUMBLEPAK,
	IDD_GBASLOT_GBAGAME,
	IDD_GBASLOT_GUITARGRIP,
	// todo
	IDD_GBASLOT_NONE
};

DLGPROC GBAslot_Procs[NDS_ADDON_COUNT] = {
	GbaSlotNone,
	GbaSlotCFlash,
	GbaSlotRumblePak,
	GbaSlotGBAgame,
	//todo
	GbaSlotGuitarGrip,
	GbaSlotNone
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
						if (romloaded && (needReset || (temp_type!=addon_type)) )
						{
							Msg = MessageBox(dialog, 
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
	strcpy(tmp_cflash_filename, CFlashName.c_str());
	strcpy(tmp_cflash_path, CFlashPath.c_str());
	strcpy(tmp_gbagame_filename, GBAgameName);
	memcpy(&tmp_Guitar, &Guitar, sizeof(Guitar));
	tmp_CFlashMode = CFlash_Mode;
	_OKbutton = false;
	needReset = true;
	u32 res=DialogBox(hAppInst, MAKEINTRESOURCE(IDD_GBASLOT), hwnd, (DLGPROC) GbaSlotBox_Proc);
	if (res)
	{
		switch (temp_type)
		{
			case NDS_ADDON_NONE:
				if (temp_type != addon_type)
					needReset = true;
				else
					needReset = false;
				break;
			case NDS_ADDON_CFLASH:
				CFlash_Mode = tmp_CFlashMode;
				WritePrivateProfileInt("GBAslot.CFlash","fileMode",CFlash_Mode,IniName);
				WritePrivateProfileString("GBAslot.CFlash","path",tmp_cflash_path,IniName);
				WritePrivateProfileString("GBAslot.CFlash","filename",tmp_cflash_filename,IniName);
				if(CFlash_Mode == ADDON_CFLASH_MODE_Path)
					CFlash_Path = tmp_cflash_path;
				else if(CFlash_Mode == ADDON_CFLASH_MODE_RomPath)
					CFlash_Path = "";
				else
					CFlash_Path = tmp_cflash_filename;
				needReset = true;
				break;
			case NDS_ADDON_RUMBLEPAK:
				if (temp_type != addon_type)
					needReset = true;
				else
					needReset = false;
				break;
			case NDS_ADDON_GBAGAME:
				strcpy(GBAgameName, tmp_gbagame_filename);
				WritePrivateProfileString("GBAslot.GBAgame","filename",GBAgameName,IniName);
				needReset = true;
				break;
			case NDS_ADDON_GUITARGRIP:
				memcpy(&Guitar, &tmp_Guitar, sizeof(tmp_Guitar));
				Guitar.Enabled = true;
				WritePrivateProfileInt("GBAslot.GuitarGrip","green",Guitar.GREEN,IniName);
				WritePrivateProfileInt("GBAslot.GuitarGrip","red",Guitar.RED,IniName);
				WritePrivateProfileInt("GBAslot.GuitarGrip","yellow",Guitar.YELLOW,IniName);
				WritePrivateProfileInt("GBAslot.GuitarGrip","blue",Guitar.BLUE,IniName);
				if (temp_type != addon_type)
					needReset = true;
				else
					needReset = false;
				break;
			case NDS_ADDON_EXPMEMORY:
				break;
			default:
				return;
		}
		if (temp_type!=NDS_ADDON_GUITARGRIP) 
			Guitar.Enabled = false;
		WritePrivateProfileInt("GBAslot","type",temp_type,IniName);

		addon_type = temp_type;
		addonsChangePak(addon_type);
		if (romloaded && needReset)
			NDS_Reset();
		return;
	}
}