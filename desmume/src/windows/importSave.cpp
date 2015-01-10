/*
	Copyright 2006 Theo Berkau
	Copyright (C) 2006-2015 DeSmuME team

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

#include "importSave.h"

#include "../types.h"
#include "../path.h"
#include "../MMU.h"
#include "../NDSSystem.h"
#include "../utils/advanscene.h"

#include "resource.h"

char ImportSavFName[MAX_PATH] = {0};
u32 fileSaveSize = 0;
u32 fileSaveType = 0xFF;

BOOL CALLBACK ImportSizeSelect_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			char buf[256] = {0};

			if (advsc.isLoaded())
			{
				
				memset(&buf, 0, sizeof(buf));
				u8 sv = advsc.getSaveType();
				if (sv == 0xFF)
				{
					strcpy(buf, "Unknown");
					EnableWindow(GetDlgItem(hDlg, IDC_IMP_AUTO_ADVANSCENE), false);
				}
				else
					if (sv == 0xFE)
					{
						strcpy(buf, "None");
						EnableWindow(GetDlgItem(hDlg, IDC_IMP_AUTO_ADVANSCENE), false);
					}
					else
						strcpy(buf, save_types[sv + 1].descr);
				SetWindowText(GetDlgItem(hDlg, IDC_IMP_INFO_ADVANSCENE), buf);
			}
			else
				EnableWindow(GetDlgItem(hDlg, IDC_IMP_AUTO_ADVANSCENE), false);

			{
				u8 type = MMU_new.backupDevice.searchFileSaveType(MMU_new.backupDevice.info.size);
				if (type == 0xFF)
					SetWindowText(GetDlgItem(hDlg, IDC_IMP_INFO_CURRENT), "NA");
				else
					SetWindowText(GetDlgItem(hDlg, IDC_IMP_INFO_CURRENT), save_types[type + 1].descr);
			}

			SendDlgItemMessage(hDlg, IDC_IMP_AUTO_CURRENT, BM_SETCHECK, true, 0);

			for (u8 i = 1; i <= MAX_SAVE_TYPES; i++) 
			{
				SendDlgItemMessage(hDlg, IDC_IMP_MANUAL_SIZE, CB_ADDSTRING, 0, (LPARAM)save_types[i].descr);
			}
			SendDlgItemMessage(hDlg, IDC_IMP_MANUAL_SIZE, CB_SETCURSEL, MMU_new.backupDevice.info.type, 0);

			fileSaveSize = MMU_new.backupDevice.importDataSize(ImportSavFName);

			if (fileSaveSize > 0)
			{
				fileSaveType = MMU_new.backupDevice.searchFileSaveType(fileSaveSize);
				if (fileSaveType == 0xFF)
				{
					sprintf(buf, "%i bytes - ERROR", fileSaveSize);
					EnableWindow(GetDlgItem(hDlg, IDC_IMP_AUTO_FILE), false);
				}
				else
				{
					char sizeS[30] = {0};
					memset(&sizeS[0], 0, sizeof(sizeS));
					u32 ss = save_types[fileSaveType+1].size * 8 / 1024;
					if (ss >= 1024)
						sprintf(sizeS, "%i Mbit", ss / 1024);
					else
						sprintf(sizeS, "%i Kbit", ss);
					sprintf(buf, "%s - %i bytes", sizeS, fileSaveSize);
				}
			}
			else
			{
				strcpy(buf, "ERROR"); // TODO: disable OK button
				EnableWindow(GetDlgItem(hDlg, IDC_IMP_AUTO_FILE), false);
			}

			SetWindowText(GetDlgItem(hDlg, IDC_IMP_INFO_FILE), buf);

			SetFocus(GetDlgItem(hDlg, IDC_IMP_AUTO_CURRENT));
		}
			
		return false;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_IMP_MANUAL_SIZE:

					//when the user changes the desired size, automatically select manual radiobutton option
					if(HIWORD(wParam) == CBN_SELCHANGE)
					{
						CheckRadioButton(hDlg,IDC_IMP_AUTO_CURRENT,IDC_IMP_MANUAL,IDC_IMP_MANUAL);
					}
					break;

				case IDOK:
				{
					u32 res = 0;

					if (SendDlgItemMessage(hDlg, IDC_IMP_AUTO_CURRENT, BM_GETCHECK, 0, 0) == BST_CHECKED)
						res = MMU_new.backupDevice.info.type;
					else
						if (SendDlgItemMessage(hDlg, IDC_IMP_AUTO_FILE, BM_GETCHECK, 0, 0) == BST_CHECKED)
						{
							if (fileSaveSize == 0) break;
							if (fileSaveType == 0xFF) break;
							res = fileSaveType;
						}
						else
							if (SendDlgItemMessage(hDlg, IDC_IMP_AUTO_ADVANSCENE, BM_GETCHECK, 0, 0) == BST_CHECKED)
							{
								if (!advsc.isLoaded()) break;
								res = advsc.getSaveType();
								if (res > MAX_SAVE_TYPES) break;
							}
							else
								if (SendDlgItemMessage(hDlg, IDC_IMP_MANUAL, BM_GETCHECK, 0, 0) == BST_CHECKED)
									res = SendDlgItemMessage(hDlg, IDC_IMP_MANUAL_SIZE, CB_GETCURSEL, 0, 0);
								else
									break;
					EndDialog(hDlg, res);
				}
				break;

				case IDCANCEL:
					EndDialog(hDlg, MAX_SAVE_TYPES + 1);
					break;
			}
		}
	}
	return false;
}

bool importSave(HWND hwnd, HINSTANCE hAppInst)
{
	OPENFILENAME ofn;

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = "All supported types\0*.sav;*.duc;*.dss\0Raw/No$GBA Save format (*.sav)\0*.sav\0Action Replay DS Save (*.duc,*.dss)\0*.duc;*.dss\0\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile =  ImportSavFName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "sav";
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

	char buffer[MAX_PATH] = {0};
	ZeroMemory(buffer, sizeof(buffer));
	path.getpath(path.BATTERY, buffer);
	ofn.lpstrInitialDir = buffer;

	if(!GetOpenFileName(&ofn))
		return true;

	u32 res = DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_IMPORT_SAVE_SIZE), hwnd, (DLGPROC)ImportSizeSelect_Proc);
	if (res < MAX_SAVE_TYPES)
	{
		res = MMU_new.backupDevice.importData(ImportSavFName, save_types[res+1].size);
		if (res)
		{
			printf("Save was successfully imported\n");
			NDS_Reset(); // reboot game
		}
		else
			printf("Save was not successfully imported");
		return res;
	}

	return (res == (MAX_SAVE_TYPES + 1));
}