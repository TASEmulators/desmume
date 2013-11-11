/*
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

#include "gbaslot_config.h"
#include <windowsx.h>
#include "resource.h"
#include "main.h"
#include "debug.h"
#include "../slot2.h"
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
SPiano		tmp_Piano;
SPaddle		tmp_Paddle;

//these are the remembered preset values for directory and filename
//they are named very verbosely to distinguish them from the currently-configured values in addons.cpp
std::string win32_CFlash_cfgDirectory, win32_CFlash_cfgFileName;
UINT win32_CFlash_cfgMode;

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

					const char *fileFilter = "FAT image (*.img)\0*.img\0Any file (*.*)\0*.*\0";
					
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
					bp.lpszTitle="Select directory for FAT image building";
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

INT_PTR CALLBACK GbaSlotPaddle(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	int which = 0;

	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton = TRUE;
			SendDlgItemMessage(dialog,IDC_PINC,WM_USER+44,tmp_Paddle.INC,0);
			SendDlgItemMessage(dialog,IDC_PDEC,WM_USER+44,tmp_Paddle.DEC,0);

			return TRUE;
		}

		case WM_USER+46:
			SendDlgItemMessage(dialog,IDC_PINC,WM_USER+44,tmp_Paddle.INC,0);
			SendDlgItemMessage(dialog,IDC_PDEC,WM_USER+44,tmp_Paddle.DEC,0);
		return TRUE;

		case WM_USER+43:
			//MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
			which = GetDlgCtrlID((HWND)lparam);
			switch(which)
			{
			case IDC_PINC:
				tmp_Paddle.INC = wparam;

				break;
			case IDC_PDEC:
				tmp_Paddle.DEC = wparam;

				break;
			}

			SendDlgItemMessage(dialog,IDC_PINC,WM_USER+44,tmp_Paddle.INC,0);
			SendDlgItemMessage(dialog,IDC_PDEC,WM_USER+44,tmp_Paddle.DEC,0);
			PostMessage(dialog,WM_NEXTDLGCTL,0,0);
		return true;
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
														"NDS ROM (for nitroFS roms) (*.nds,*.srl)\0*.nds;*.srl\0"
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

INT_PTR CALLBACK GbaSlotPiano(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	int which = 0;

	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			_OKbutton = TRUE;
			SendDlgItemMessage(dialog,IDC_PIANO_C,WM_USER+44,tmp_Piano.C,0);
			SendDlgItemMessage(dialog,IDC_PIANO_CS,WM_USER+44,tmp_Piano.CS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_D,WM_USER+44,tmp_Piano.D,0);
			SendDlgItemMessage(dialog,IDC_PIANO_DS,WM_USER+44,tmp_Piano.DS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_E,WM_USER+44,tmp_Piano.E,0);
			SendDlgItemMessage(dialog,IDC_PIANO_F,WM_USER+44,tmp_Piano.F,0);
			SendDlgItemMessage(dialog,IDC_PIANO_FS,WM_USER+44,tmp_Piano.FS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_G,WM_USER+44,tmp_Piano.G,0);
			SendDlgItemMessage(dialog,IDC_PIANO_GS,WM_USER+44,tmp_Piano.GS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_A,WM_USER+44,tmp_Piano.A,0);
			SendDlgItemMessage(dialog,IDC_PIANO_AS,WM_USER+44,tmp_Piano.AS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_B,WM_USER+44,tmp_Piano.B,0);
			SendDlgItemMessage(dialog,IDC_PIANO_HIC,WM_USER+44,tmp_Piano.HIC,0);

			return TRUE;
		}

		case WM_USER+46:
			SendDlgItemMessage(dialog,IDC_PIANO_C,WM_USER+44,tmp_Piano.C,0);
			SendDlgItemMessage(dialog,IDC_PIANO_CS,WM_USER+44,tmp_Piano.CS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_D,WM_USER+44,tmp_Piano.D,0);
			SendDlgItemMessage(dialog,IDC_PIANO_DS,WM_USER+44,tmp_Piano.DS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_E,WM_USER+44,tmp_Piano.E,0);
			SendDlgItemMessage(dialog,IDC_PIANO_F,WM_USER+44,tmp_Piano.F,0);
			SendDlgItemMessage(dialog,IDC_PIANO_FS,WM_USER+44,tmp_Piano.FS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_G,WM_USER+44,tmp_Piano.G,0);
			SendDlgItemMessage(dialog,IDC_PIANO_GS,WM_USER+44,tmp_Piano.GS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_A,WM_USER+44,tmp_Piano.A,0);
			SendDlgItemMessage(dialog,IDC_PIANO_AS,WM_USER+44,tmp_Piano.AS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_B,WM_USER+44,tmp_Piano.B,0);
			SendDlgItemMessage(dialog,IDC_PIANO_HIC,WM_USER+44,tmp_Piano.HIC,0);
		return TRUE;

		case WM_USER+43:
			//MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
			which = GetDlgCtrlID((HWND)lparam);
			switch(which)
			{
			case IDC_PIANO_C: tmp_Piano.C = wparam; break;
			case IDC_PIANO_CS: tmp_Piano.CS = wparam; break;
			case IDC_PIANO_D: tmp_Piano.D = wparam; break;
			case IDC_PIANO_DS: tmp_Piano.DS = wparam; break;
			case IDC_PIANO_E: tmp_Piano.E = wparam; break;
			case IDC_PIANO_F: tmp_Piano.F = wparam; break;
			case IDC_PIANO_FS: tmp_Piano.FS = wparam; break;
			case IDC_PIANO_G: tmp_Piano.G = wparam; break;
			case IDC_PIANO_GS: tmp_Piano.GS = wparam; break;
			case IDC_PIANO_A: tmp_Piano.A = wparam; break;
			case IDC_PIANO_AS: tmp_Piano.AS = wparam; break;
			case IDC_PIANO_B: tmp_Piano.B = wparam; break;
			case IDC_PIANO_HIC: tmp_Piano.HIC = wparam; break;

			}

			SendDlgItemMessage(dialog,IDC_PIANO_C,WM_USER+44,tmp_Piano.C,0);
			SendDlgItemMessage(dialog,IDC_PIANO_CS,WM_USER+44,tmp_Piano.CS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_D,WM_USER+44,tmp_Piano.D,0);
			SendDlgItemMessage(dialog,IDC_PIANO_DS,WM_USER+44,tmp_Piano.DS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_E,WM_USER+44,tmp_Piano.E,0);
			SendDlgItemMessage(dialog,IDC_PIANO_F,WM_USER+44,tmp_Piano.F,0);
			SendDlgItemMessage(dialog,IDC_PIANO_FS,WM_USER+44,tmp_Piano.FS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_G,WM_USER+44,tmp_Piano.G,0);
			SendDlgItemMessage(dialog,IDC_PIANO_GS,WM_USER+44,tmp_Piano.GS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_A,WM_USER+44,tmp_Piano.A,0);
			SendDlgItemMessage(dialog,IDC_PIANO_AS,WM_USER+44,tmp_Piano.AS,0);
			SendDlgItemMessage(dialog,IDC_PIANO_B,WM_USER+44,tmp_Piano.B,0);
			SendDlgItemMessage(dialog,IDC_PIANO_HIC,WM_USER+44,tmp_Piano.HIC,0);
			PostMessage(dialog,WM_NEXTDLGCTL,0,0);
		return true;
	}
	return FALSE;
}

u32		GBAslot_IDDs[NDS_SLOT2_COUNT] = {
	IDD_GBASLOT_NONE,
	IDD_GBASLOT_NONE,
	IDD_GBASLOT_CFLASH,
	IDD_GBASLOT_RUMBLEPAK,
	IDD_GBASLOT_GBAGAME,
	IDD_GBASLOT_GUITARGRIP,
	IDD_GBASLOT_NONE, //expmem
	IDD_GBASLOT_PIANO,
	IDD_GBASLOT_PADDLE, //paddle
	IDD_GBASLOT_NONE, //PassME
};

DLGPROC GBAslot_Procs[NDS_SLOT2_COUNT] = {
	GbaSlotNone,
	GbaSlotNone,
	GbaSlotCFlash,
	GbaSlotRumblePak,
	GbaSlotGBAgame,
	GbaSlotGuitarGrip,
	GbaSlotNone,  //expmem
	GbaSlotPiano,
	GbaSlotPaddle,
	GbaSlotNone			// PassME
};


//==============================================================================
BOOL CALLBACK GbaSlotBox_Proc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			OKbutton = GetDlgItem(dialog, IDOK);
			for(int i = 0; i < NDS_SLOT2_COUNT; i++)
				ComboBox_AddString(GetDlgItem(dialog, IDC_ADDONS_LIST), slot2_List[i]->info()->name());
			ComboBox_SetCurSel(GetDlgItem(dialog, IDC_ADDONS_LIST), temp_type);
			SetWindowText(GetDlgItem(dialog, IDC_ADDONS_INFO), slot2_List[temp_type]->info()->descr());

			_OKbutton = false;
			wndConfig=CreateDialogW(hAppInst, MAKEINTRESOURCEW(GBAslot_IDDs[temp_type]), 
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
						if (wndConfig) DestroyWindow(wndConfig);
						EndDialog(dialog, TRUE);
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
							wndConfig=CreateDialogW(hAppInst, 
								MAKEINTRESOURCEW(GBAslot_IDDs[temp_type]), dialog, 
								(DLGPROC)GBAslot_Procs[temp_type]);
							if ( (temp_type == 0) || (_OKbutton) )
								EnableWindow(OKbutton, TRUE);
							else
								EnableWindow(OKbutton, FALSE);
							SetWindowText(GetDlgItem(dialog, IDC_ADDONS_INFO), slot2_List[temp_type]->info()->descr());
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
	temp_type = (u8)slot2_GetCurrentType();
	last_type = temp_type;
	strcpy(tmp_cflash_filename, win32_CFlash_cfgFileName.c_str());
	strcpy(tmp_cflash_path, win32_CFlash_cfgDirectory.c_str());
	strcpy(tmp_gbagame_filename, GBAgameName);
	memcpy(&tmp_Guitar, &Guitar, sizeof(Guitar));
	memcpy(&tmp_Piano, &Piano, sizeof(Piano));
	memcpy(&tmp_Paddle, &Paddle, sizeof(Paddle));
	tmp_CFlashMode = CFlash_Mode;
	_OKbutton = false;
	
	u32 res=DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_GBASLOT), hwnd, (DLGPROC) GbaSlotBox_Proc);
	if (res)
	{
		switch (temp_type)
		{
			case NDS_SLOT2_NONE:
			break;

			case NDS_SLOT2_AUTO:
			break;

			case NDS_SLOT2_CFLASH:
				//save current values for win32 configuration
				win32_CFlash_cfgMode = tmp_CFlashMode;
				win32_CFlash_cfgDirectory = tmp_cflash_path;
				win32_CFlash_cfgFileName = tmp_cflash_filename;
				WritePrivateProfileInt("Slot2.CFlash","fileMode",tmp_CFlashMode,IniName);
				WritePrivateProfileString("Slot2.CFlash","path",tmp_cflash_path,IniName);
				WritePrivateProfileString("Slot2.CFlash","filename",tmp_cflash_filename,IniName);

				WIN_InstallCFlash();
				break;
			case NDS_SLOT2_RUMBLEPAK:
				break;
			case NDS_SLOT2_PADDLE:
				memcpy(&Paddle, &tmp_Paddle, sizeof(tmp_Paddle));
				WritePrivateProfileInt("Slot2.Paddle","DEC",Paddle.DEC,IniName);
				WritePrivateProfileInt("Slot2.Paddle","INC",Paddle.INC,IniName);
				break;
			case NDS_SLOT2_GBACART:
				strcpy(GBAgameName, tmp_gbagame_filename);
				WritePrivateProfileString("Slot2.GBAgame","filename",GBAgameName,IniName);
				break;
			case NDS_SLOT2_GUITARGRIP:
				memcpy(&Guitar, &tmp_Guitar, sizeof(tmp_Guitar));
				WritePrivateProfileInt("Slot2.GuitarGrip","green",Guitar.GREEN,IniName);
				WritePrivateProfileInt("Slot2.GuitarGrip","red",Guitar.RED,IniName);
				WritePrivateProfileInt("Slot2.GuitarGrip","yellow",Guitar.YELLOW,IniName);
				WritePrivateProfileInt("Slot2.GuitarGrip","blue",Guitar.BLUE,IniName);
				break;
			case NDS_SLOT2_EASYPIANO:
				memcpy(&Piano, &tmp_Piano, sizeof(tmp_Piano));
				WritePrivateProfileInt("Slot2.Piano","C",Piano.C,IniName);
				WritePrivateProfileInt("Slot2.Piano","CS",Piano.CS,IniName);
				WritePrivateProfileInt("Slot2.Piano","D",Piano.D,IniName);
				WritePrivateProfileInt("Slot2.Piano","DS",Piano.DS,IniName);
				WritePrivateProfileInt("Slot2.Piano","E",Piano.E,IniName);
				WritePrivateProfileInt("Slot2.Piano","F",Piano.F,IniName);
				WritePrivateProfileInt("Slot2.Piano","FS",Piano.FS,IniName);
				WritePrivateProfileInt("Slot2.Piano","G",Piano.G,IniName);
				WritePrivateProfileInt("Slot2.Piano","GS",Piano.GS,IniName);
				WritePrivateProfileInt("Slot2.Piano","A",Piano.A,IniName);
				WritePrivateProfileInt("Slot2.Piano","AS",Piano.AS,IniName);
				WritePrivateProfileInt("Slot2.Piano","B",Piano.B,IniName);
				WritePrivateProfileInt("Slot2.Piano","HIC",Piano.HIC,IniName);
				break;
			case NDS_SLOT2_EXPMEMORY:
				break;
			case NDS_SLOT2_PASSME:
				break;
			default:
				return;
		}

		slot2_Change((NDS_SLOT2_TYPE)temp_type);

		WritePrivateProfileInt("Slot2", "id", slot2_List[(u8)slot2_GetCurrentType()]->info()->id(), IniName);

		Guitar.Enabled	= (slot2_GetCurrentType() == NDS_SLOT2_GUITARGRIP)?true:false;
		Piano.Enabled	= (slot2_GetCurrentType() == NDS_SLOT2_EASYPIANO)?true:false;
		Paddle.Enabled	= (slot2_GetCurrentType() == NDS_SLOT2_PADDLE)?true:false;
	}
}

