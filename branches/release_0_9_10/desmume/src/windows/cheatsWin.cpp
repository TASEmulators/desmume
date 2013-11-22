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

#include "cheatsWin.h"
#include <commctrl.h>
#include "../cheatSystem.h"
#include "resource.h"
#include "../debug.h"
#include "../utils/xstring.h"
#include "../path.h"
#include "../NDSSystem.h"
#include "../version.h"

extern u8	CheatsR4Type = 0;

static const char *HEX_Valid = "Oo0123456789ABCDEFabcdef";
static const char *DEC_Valid = "Oo0123456789";
static	u8		searchType = 0;
static	u8		searchSize = 0;
static	u8		searchSign = 0;
static	u8		searchStep = 0;
static	u8		searchComp = 0;
static	HWND	hBRestart = NULL;
static	HWND	hBView = NULL;
static	HWND	hBSearch = NULL;
static	u32		exactVal = 0;
static	u32		searchNumberResults = 0;

static	u32		searchAddAddress = 0;
static	u32		searchAddValue = 0;
static	u8		searchAddMode = 0;
static	u8		searchAddFreeze = 1;
static	u8		searchAddSize = 0;
static	const char*	searchAddDesc = 0;
static	char	editBuf[3][75] = { 0 };
static	u32		cheatEditPos = 0;
static	u8		cheatAddPasteCheck = 0;
static	u8		cheatXXtype = 0;
static	u8		cheatXXaction = 0;

static	HWND	searchWnd = NULL;
static	HWND	searchListView = NULL;
static	HWND	cheatListView = NULL;
static	HWND	exportListView = NULL;
static	LONG_PTR	oldEditProc = NULL;
static	LONG_PTR	oldEditProcHEX = NULL;

CHEATS_LIST		tempCheat;

u32		searchIDDs[2][4] = {
	{ IDD_CHEAT_SEARCH_MAIN, IDD_CHEAT_SEARCH_EXACT, IDD_CHEAT_SEARCH_RESULT, NULL },
	{ IDD_CHEAT_SEARCH_MAIN, IDD_CHEAT_SEARCH_RESULT, IDD_CHEAT_SEARCH_COMP, IDD_CHEAT_SEARCH_RESULT}
};

u32	searchSizeIDDs[4] = { IDC_RADIO1, IDC_RADIO2, IDC_RADIO3, IDC_RADIO4 };
u32	searchSignIDDs[2] = { IDC_RADIO5, IDC_RADIO6 };
u32	searchTypeIDDs[2] = { IDC_RADIO7, IDC_RADIO8 };
u32	searchCompIDDs[4] = { IDC_RADIO1, IDC_RADIO2, IDC_RADIO3, IDC_RADIO4 };

u32	searchRangeIDDs[4] = { IDC_STATIC_S1, IDC_STATIC_S2, IDC_STATIC_S3, IDC_STATIC_S4 };
char *searchRangeText[2][4] = { {"[0..255]", "[0..65536]", "[0..16777215]", "[0..4294967295]"},
								{"[-128..127]", "[-32168..32767]", "[-8388608..8388607]", "[-2147483648..2147483647]"}};

u32 searchRange[4][2] = { 
							{ 0, 255 },
							{ 0, 65535 },
							{ 0, 16777215 },
							{ 0, 4294967295 }
						};

//========================================= Export
static CHEATSEXPORT	*cheatsExport = NULL;
bool CheatsExportDialog(HWND hwnd);

void generateAR(HWND dialog, u32 addr, u32 val, u8 size)
{
	// Action Replay code generate
	if (size > 3) size = 3;
	char buf[64] = {0};
	sprintf(buf, "%X%07X %08X", 3-size, addr | 0x02000000, val);
	SetWindowText(GetDlgItem(dialog, IDC_AR_CODE), buf);
}

LONG_PTR CALLBACK EditValueProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CHAR)
	{
		switch (wParam)
		{
			case '-':
				{
					u32 pos = 0;
					SendMessage(hwnd, EM_GETSEL, (WPARAM)&pos, NULL);
					if (pos != 0) wParam = 0;
				}
				break;

			case VK_BACK:
			case 0x3: // ^C
			case 0x18: // ^X
			case 0x1a: // ^Z
				break;
			case 0x16: // ^V
				cheatAddPasteCheck = true;
				break;
			default:
				if (strchr(DEC_Valid, wParam))
				{
					if(wParam=='o' || wParam=='O') wParam='0';
					break;
				}
				wParam = 0;
				break;
		}
        
	}

	return CallWindowProc((WNDPROC)oldEditProc, hwnd, msg, wParam, lParam);
}

LONG_PTR CALLBACK EditValueHEXProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CHAR)
	{
		switch (wParam)
		{
			case VK_BACK:
			case 0x3: // ^C
			case 0x18: // ^X
			case 0x1a: // ^Z
				break;
			case 0x16: // ^V
				cheatAddPasteCheck = true;
				break;
			default:
				if (strchr(HEX_Valid, wParam))
				{
					if(wParam=='o' || wParam=='O') wParam='0';
					break;
				}
				wParam = 0;
				break;
		}
        
	}

	return CallWindowProc((WNDPROC)oldEditProcHEX, hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK CheatsAddProc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	static LONG_PTR saveOldEditProc = NULL;

	switch(msg)
	{
		case WM_INITDIALOG:
			{	
				memset(editBuf, 0, sizeof(editBuf));
				memset(&tempCheat, 0, sizeof(tempCheat));
				saveOldEditProc = oldEditProc;
				SendMessage(GetDlgItem(dialog, IDC_EDIT1), EM_SETLIMITTEXT, 6, 0);
				SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_SETLIMITTEXT, 11, 0);
				SendMessage(GetDlgItem(dialog, IDC_EDIT3), EM_SETLIMITTEXT, 75, 0);
				oldEditProcHEX = SetWindowLongPtr(GetDlgItem(dialog, IDC_EDIT1), GWLP_WNDPROC, (LONG_PTR)EditValueHEXProc);
				oldEditProc = SetWindowLongPtr(GetDlgItem(dialog, IDC_EDIT2), GWLP_WNDPROC, (LONG_PTR)EditValueProc);

				if (searchAddMode == 1 || searchAddMode == 2)
				{
					char buf[12];
					searchAddAddress &= 0x00FFFFFF;
					wsprintf(buf, "%06X", searchAddAddress);
					SetWindowText(GetDlgItem(dialog, IDC_EDIT1), buf);
					wsprintf(buf, "%i", searchAddValue);
					SetWindowText(GetDlgItem(dialog, IDC_EDIT2), buf);
					EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
					if (searchAddMode == 1)
					{
						EnableWindow(GetDlgItem(dialog, IDC_EDIT1), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_RADIO1), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_RADIO2), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_RADIO3), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_RADIO4), FALSE);
						EnableWindow(GetDlgItem(dialog, IDC_RADIO8), FALSE);
					}
				}
				else
				{
					SetWindowText(GetDlgItem(dialog, IDC_EDIT2), "0");
					CheckRadioButton(dialog, IDC_RADIO1, IDC_RADIO4, IDC_RADIO1);
				}

				memset(editBuf, 0, sizeof(editBuf));
				if(searchAddDesc)
				{
					strncpy(editBuf[2], searchAddDesc, sizeof(editBuf[2]) - 1);
					SetWindowText(GetDlgItem(dialog, IDC_EDIT3), editBuf[2]);
				}
				searchAddDesc = 0;

				GetWindowText(GetDlgItem(dialog, IDC_EDIT1), editBuf[0], 10);
				GetWindowText(GetDlgItem(dialog, IDC_EDIT2), editBuf[1], 12);
				
				CheckDlgButton(dialog, IDC_CHECK1, BST_CHECKED);
				CheckRadioButton(dialog, searchSizeIDDs[0], searchSizeIDDs[ARRAY_SIZE(searchSizeIDDs) - 1], searchSizeIDDs[searchAddSize]);

				if(searchAddMode == 2)
				{
					SetFocus(GetDlgItem(dialog, IDC_EDIT2));
					SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_SETSEL, 0, -1);
				}

				CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);
			}
		return searchAddMode != 2;

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
				{
					u32 tmp_addr = 0;
					sscanf_s(editBuf[0], "%x", &tmp_addr);
					
					if (cheats->add(searchAddSize, tmp_addr, atol(editBuf[1]), editBuf[2], searchAddFreeze))
					{
						if ((searchAddMode == 0) || (cheats->save() && (searchAddMode == 1 || searchAddMode == 2)))
						{
							oldEditProc = saveOldEditProc;
							searchAddAddress = tmp_addr;
							searchAddValue = strtoul(editBuf[1],NULL,10);

							EndDialog(dialog, TRUE);
						}
					}
				}
				return TRUE;

				case IDCANCEL:
					oldEditProc = saveOldEditProc;
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_EDIT1:		// address
				{
					if (HIWORD(wparam) == EN_UPDATE)
					{
						GetWindowText(GetDlgItem(dialog, IDC_EDIT1), editBuf[0], 8);

						u32 val = 0;
						sscanf_s(editBuf[0], "%x", &val);
						val &= 0x00FFFFFF;

						if(cheatAddPasteCheck)
						{
							cheatAddPasteCheck = 0;
							char temp [12];
							sprintf(temp, "%06X", val);
							if(strcmp(editBuf[0], temp))
							{
								strcpy(editBuf[0], temp);
								int sel1=-1, sel2=0;
								SendMessage(GetDlgItem(dialog , IDC_EDIT1), EM_GETSEL, (WPARAM)&sel1, (LPARAM)&sel2);
								SetWindowText(GetDlgItem(dialog, IDC_EDIT1), editBuf[0]);
								SendMessage(GetDlgItem(dialog, IDC_EDIT1), EM_SETSEL, sel1, sel2);
							}
						}

						CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);
					}
					return TRUE;
				}

				case IDC_EDIT2:		// value
				{
					if (HIWORD(wparam) == EN_UPDATE)
					{
						GetWindowText(GetDlgItem(dialog, IDC_EDIT2), editBuf[1], 12);
						int parseOffset = 0;
						if(editBuf[1][0] && editBuf[1][1] == '-')
							parseOffset = 1; // typed something in front of -
						u32 val = strtoul(editBuf[1]+parseOffset,NULL,10);

						if(cheatAddPasteCheck || parseOffset)
						{
							cheatAddPasteCheck = 0;
							val &= searchRange[searchAddSize][1];
							char temp [12];
							sprintf(temp, "%u", val);
							if(strcmp(editBuf[1], temp))
							{
								strcpy(editBuf[1], temp);
								int sel1=-1, sel2=0;
								SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_GETSEL, (WPARAM)&sel1, (LPARAM)&sel2);
								SetWindowText(GetDlgItem(dialog, IDC_EDIT2), editBuf[1]);
								SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_SETSEL, sel1, sel2);
							}
						}

						CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);

					}
					return TRUE;
				}

				case IDC_EDIT3:		// description
				{
					if (HIWORD(wparam) == EN_UPDATE)
						GetWindowText(GetDlgItem(dialog, IDC_EDIT3), editBuf[2], 75);
					return TRUE;
				}

				case IDC_RADIO1:		// 1 byte
					searchAddSize = 0;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);
				return TRUE;
				case IDC_RADIO2:		// 2 bytes
					searchAddSize = 1;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);
				return TRUE;
				case IDC_RADIO3:		// 3 bytes
					searchAddSize = 2;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);
				return TRUE;
				case IDC_RADIO4:		// 4 bytes
					searchAddSize = 3;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],searchAddSize);
				return TRUE;

				case IDC_CHECK1:
				{
					if (IsDlgButtonChecked(dialog, IDC_CHECK1) == BST_CHECKED)
						searchAddFreeze = 1;
					else
						searchAddFreeze = 0;
				}
			}
		}
	}
	return FALSE;
}

INT_PTR CALLBACK CheatsEditProc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	static LONG_PTR	saveOldEditProc = NULL;
	char			buf[100] = {0}, buf2[100] = {0};

	switch(msg)
	{
		case WM_INITDIALOG:
			{	
				memset(editBuf, 0, sizeof(editBuf));
				memset(&tempCheat, 0, sizeof(tempCheat));
				saveOldEditProc = oldEditProc;
				SendMessage(GetDlgItem(dialog, IDC_EDIT1), EM_SETLIMITTEXT, 6, 0);
				SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_SETLIMITTEXT, 10, 0);
				SendMessage(GetDlgItem(dialog, IDC_EDIT3), EM_SETLIMITTEXT, 75, 0);
				oldEditProcHEX = SetWindowLongPtr(GetDlgItem(dialog, IDC_EDIT1), GWLP_WNDPROC, (LONG_PTR)EditValueHEXProc);
				oldEditProc = SetWindowLongPtr(GetDlgItem(dialog, IDC_EDIT2), GWLP_WNDPROC, (LONG_PTR)EditValueProc);

				cheats->get(&tempCheat, cheatEditPos);
				
				memset(buf, 0, 100);
				memset(buf2, 0, 100);
				tempCheat.code[0][0] &= 0x00FFFFFF;
				wsprintf(buf, "%06X", tempCheat.code[0][0]);
				SetWindowText(GetDlgItem(dialog, IDC_EDIT1), buf);
				wsprintf(buf, "%i", tempCheat.code[0][1]);
				SetWindowText(GetDlgItem(dialog, IDC_EDIT2), buf);
				strcpy(buf, tempCheat.description);
				SetWindowText(GetDlgItem(dialog, IDC_EDIT3), buf);
				EnableWindow(GetDlgItem(dialog, IDOK), TRUE);

				GetWindowText(GetDlgItem(dialog, IDC_EDIT1), editBuf[0], 10);
				GetWindowText(GetDlgItem(dialog, IDC_EDIT2), editBuf[1], 12);

				CheckDlgButton(dialog, IDC_CHECK1, tempCheat.enabled?BST_CHECKED:BST_UNCHECKED);
				CheckRadioButton(dialog, searchSizeIDDs[0], searchSizeIDDs[ARRAY_SIZE(searchSizeIDDs) - 1], searchSizeIDDs[tempCheat.size]);
				SetWindowText(GetDlgItem(dialog, IDOK), "Update");

				generateAR(dialog, tempCheat.code[0][0], tempCheat.code[0][1], searchSizeIDDs[tempCheat.size]);
			}
		return TRUE;

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
				{
					if (cheats->update(tempCheat.size, tempCheat.code[0][0], tempCheat.code[0][1], tempCheat.description, tempCheat.enabled, cheatEditPos))
					{
						oldEditProc = saveOldEditProc;
						EndDialog(dialog, TRUE);
					}
				}
				return TRUE;

				case IDCANCEL:
					oldEditProc = saveOldEditProc;
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_EDIT1:		// address
				{
					if (HIWORD(wparam) == EN_UPDATE)
					{
						GetWindowText(GetDlgItem(dialog, IDC_EDIT1), editBuf[0], 10);
						
						u32 val = 0;
						sscanf_s(editBuf[0], "%x", &val);
						val &= 0x00FFFFFF;
						CheatAddVerify(dialog,editBuf[0],editBuf[1],tempCheat.size);
						tempCheat.code[0][0] = val;
					}
					return TRUE;
				}

				case IDC_EDIT2:		// value
				{
					if (HIWORD(wparam) == EN_UPDATE)
					{
						GetWindowText(GetDlgItem(dialog, IDC_EDIT2), editBuf[1], 12);
						
						int parseOffset = 0;
						if(editBuf[1][0] && editBuf[1][1] == '-')
							parseOffset = 1; // typed something in front of -
						u32 val = strtoul(editBuf[1]+parseOffset,NULL,10);

						if(cheatAddPasteCheck || parseOffset)
						{
							cheatAddPasteCheck = 0;
							val &= searchRange[tempCheat.size][1];
							char temp [12];
							sprintf(temp, "%u", val);
							if(strcmp(editBuf[1], temp))
							{
								strcpy(editBuf[1], temp);
								int sel1=-1, sel2=0;
								SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_GETSEL, (WPARAM)&sel1, (LPARAM)&sel2);
								SetWindowText(GetDlgItem(dialog, IDC_EDIT2), editBuf[1]);
								SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_SETSEL, sel1, sel2);
							}
						}

						CheatAddVerify(dialog,editBuf[0],editBuf[1],tempCheat.size);
						tempCheat.code[0][1] = val;
					}
					return TRUE;
				}

				case IDC_EDIT3:		// description
				{
					if (HIWORD(wparam) == EN_UPDATE)
						GetWindowText(GetDlgItem(dialog, IDC_EDIT3), tempCheat.description, 75);
					return TRUE;
				}

				case IDC_RADIO1:		// 1 byte
					tempCheat.size = 0;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],tempCheat.size);
				return TRUE;
				case IDC_RADIO2:		// 2 bytes
					tempCheat.size = 1;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],tempCheat.size);
				return TRUE;
				case IDC_RADIO3:		// 3 bytes
					tempCheat.size = 2;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],tempCheat.size);
				return TRUE;
				case IDC_RADIO4:		// 4 bytes
					tempCheat.size = 3;
					CheatAddVerify(dialog,editBuf[0],editBuf[1],tempCheat.size);
				return TRUE;

				case IDC_CHECK1:		// freeze
				{
					if (IsDlgButtonChecked(dialog, IDC_CHECK1) == BST_CHECKED)
						tempCheat.enabled = 1;
					else
						tempCheat.enabled = 0;
				}
			}
		}
	}
	return FALSE;
}
//============================================================================== Action Replay
INT_PTR CALLBACK CheatsAdd_XX_Proc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			memset(editBuf, 0, sizeof(editBuf));
			SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_FMTLINES, (WPARAM)TRUE, (LPARAM)0);

			if (cheatXXtype == 0)
			{
				if (cheatXXaction == 0)		// add
				{
					memset(&tempCheat, 0, sizeof(tempCheat));
					SetWindowText(dialog, "Add Action Replay code");
					tempCheat.enabled = TRUE;
				}
				else						// edit
				{
					SetWindowText(dialog, "Edit Action Replay code");
				}
			}
			else
			{
				if (cheatXXaction == 0)		// add
				{
					memset(&tempCheat, 0, sizeof(tempCheat));
					SetWindowText(dialog, "Add Codebreaker code");
					tempCheat.enabled = TRUE;
				}
				else						// edit
				{
					SetWindowText(dialog, "Edit Codebreaker code");
				}
			}

			SendMessage(GetDlgItem(dialog, IDC_EDIT2), EM_SETLIMITTEXT, sizeof(tempCheat.code) * 2, 0);
			SendMessage(GetDlgItem(dialog, IDC_EDIT3), EM_SETLIMITTEXT, sizeof(tempCheat.description), 0);

			if (cheatXXaction != 0)
			{
				char buf[sizeof(tempCheat.code)*2] = { 0 };
				memset(buf, 0, sizeof(buf));

				cheats->getXXcodeString(tempCheat, buf);
				std::string bufstr = mass_replace(buf,"\n","\r\n");
				SetWindowText(GetDlgItem(dialog, IDC_EDIT2), bufstr.c_str());
				SetWindowText(GetDlgItem(dialog, IDC_EDIT3), tempCheat.description);

				EnableWindow(GetDlgItem(dialog, IDOK), (strlen(buf) > 16)?TRUE:FALSE);
				SetWindowText(GetDlgItem(dialog, IDOK), "Update");
			}
			CheckDlgButton(dialog, IDC_CHECK1, tempCheat.enabled?BST_CHECKED:BST_UNCHECKED);	
		}
		return TRUE;

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
				{
					char buf[sizeof(tempCheat.code)*2] = { 0 };

					memset(buf, 0, sizeof(buf));
					GetWindowText(GetDlgItem(dialog, IDC_EDIT2), buf, sizeof(buf));

					if (cheatXXtype == 0)		// Action Replay
					{
						if (cheatXXaction == 0)		// add
						{
							if (!cheats->add_AR(buf, tempCheat.description, tempCheat.enabled))
							{
								MessageBox(dialog, "Syntax error in Action Replay code.\nTry again", "DeSmuME",
											MB_OK | MB_ICONERROR);
								return FALSE;
							}
						}
						else						// edit
						{
							if (!cheats->update_AR(buf, tempCheat.description, tempCheat.enabled, cheatEditPos))
							{
								MessageBox(dialog, "Syntax error in Action Replay code.\nTry again", "DeSmuME",
											MB_OK | MB_ICONERROR);
								return FALSE;
							}
						}
					}
					else
					{
						if (cheatXXaction == 0)		// add
						{
							if (!cheats->add_CB(buf, tempCheat.description, tempCheat.enabled))
							{
								MessageBox(dialog, "Syntax error in Codebreaker code.\nTry again", "DeSmuME",
											MB_OK | MB_ICONERROR);
								return FALSE;
							}
						}
						else						// edit
						{
							if (!cheats->update_CB(buf, tempCheat.description, tempCheat.enabled, cheatEditPos))
							{
								MessageBox(dialog, "Syntax error in Codebreaker code.\nTry again", "DeSmuME",
											MB_OK | MB_ICONERROR);
								return FALSE;
							}
						}
					}
					EndDialog(dialog, TRUE);
				}
				return TRUE;

				case IDCANCEL:
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_EDIT2:					// code
					if (HIWORD(wparam) == EN_UPDATE)
					{
						char buf[sizeof(tempCheat.code)*2] = { 0 };

						memset(buf, 0, sizeof(buf));
						GetWindowText(GetDlgItem(dialog, IDC_EDIT2), buf, sizeof(buf));
						if (strlen(buf) < 17)		// min size of code "CXXXXXXX YYYYYYYY"
						{
							EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
							return TRUE;
						}
						EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
					}
				return TRUE;

				case IDC_EDIT3:					// description
					if (HIWORD(wparam) == EN_UPDATE)
					{
						memset(tempCheat.description, 0, sizeof(tempCheat.description));
						GetWindowText(GetDlgItem(dialog, IDC_EDIT3), tempCheat.description, sizeof(tempCheat.description));
					}
				return TRUE;

				case IDC_CHECK1:
					if (IsDlgButtonChecked(dialog, IDC_CHECK1) == BST_CHECKED)
						tempCheat.enabled = 1;
					else
						tempCheat.enabled = 0;
				return TRUE;
			}
		}
	}
	return FALSE;
}
//==============================================================================
INT_PTR CALLBACK CheatsListBox_Proc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			// TODO: Codebreaker
			ShowWindow(GetDlgItem(dialog, IDC_BADD_CB), SW_HIDE);// hide button

			LV_COLUMN lvColumn;
			u32 address = 0;
			u32 val = 0;

			cheatListView = GetDlgItem(dialog, IDC_LIST1);

			ListView_SetExtendedListViewStyle(cheatListView, LVS_EX_FULLROWSELECT | LVS_EX_TWOCLICKACTIVATE | LVS_EX_CHECKBOXES);
			
			memset(&lvColumn,0,sizeof(LV_COLUMN));
			lvColumn.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT;
			lvColumn.fmt=LVCFMT_CENTER;
			lvColumn.cx=20;
			lvColumn.pszText="";
			ListView_InsertColumn(cheatListView, 0, &lvColumn);
			lvColumn.fmt=LVCFMT_LEFT;
			lvColumn.cx=84;
			lvColumn.pszText="Address";
			ListView_InsertColumn(cheatListView, 1, &lvColumn);
			lvColumn.cx=100;
			lvColumn.pszText="Value";
			ListView_InsertColumn(cheatListView, 2, &lvColumn);
			lvColumn.cx=245;
			lvColumn.pszText="Description";
			ListView_InsertColumn(cheatListView, 3, &lvColumn);
			lvColumn.fmt=LVCFMT_CENTER;

			LVITEM lvi;
			memset(&lvi,0,sizeof(LVITEM));
			lvi.mask = LVIF_TEXT|LVIF_STATE;
			lvi.iItem = INT_MAX;

			cheats->getListReset();
			SendMessage(cheatListView, WM_SETREDRAW, (WPARAM)FALSE,0);
			while (cheats->getList(&tempCheat))
			{
				char buf[256];
				lvi.pszText= "";
				switch (tempCheat.type)
				{
					case 0:					// Internal
					{
						u32 row = ListView_InsertItem(cheatListView, &lvi);
						ListView_SetCheckState(cheatListView, row, tempCheat.enabled);
						wsprintf(buf, "0x02%06X", tempCheat.code[0][0]);
						ListView_SetItemText(cheatListView, row, 1, buf);
						ltoa(tempCheat.code[0][1], buf, 10);
						ListView_SetItemText(cheatListView, row, 2, buf);
						ListView_SetItemText(cheatListView, row, 3, tempCheat.description);
						break;
					}

					case 1:					// Action Replay
					{
						u32 row = ListView_InsertItem(cheatListView, &lvi);
						ListView_SetCheckState(cheatListView, row, tempCheat.enabled);
						ListView_SetItemText(cheatListView, row, 1, "Action");
						ListView_SetItemText(cheatListView, row, 2, "Replay");
						ListView_SetItemText(cheatListView, row, 3, tempCheat.description);
						break;
					}

					case 2:					// Codebreaker
					{
						u32 row = ListView_InsertItem(cheatListView, &lvi);
						ListView_SetCheckState(cheatListView, row, tempCheat.enabled);
						ListView_SetItemText(cheatListView, row, 1, "Code");
						ListView_SetItemText(cheatListView, row, 2, "breaker");
						ListView_SetItemText(cheatListView, row, 3, tempCheat.description);
						break;
					}
				}
			}
			SendMessage(cheatListView, WM_SETREDRAW, (WPARAM)TRUE,0);

			EnableWindow(GetDlgItem(dialog, IDOK), FALSE);

			ListView_SetItemState(cheatListView,0, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
			SetFocus(cheatListView);
			return TRUE;
		}

		case WM_NOTIFY:
			if (wparam == IDC_LIST1)
			{
				LPNMHDR tmp_msg = (LPNMHDR)lparam;
				switch (tmp_msg->code)
				{
					case LVN_ITEMACTIVATE:
						SendMessage(dialog, WM_COMMAND, IDC_BEDIT, 0);
					break;

					case LVN_ITEMCHANGED:
					{
						NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)lparam;
						if(pNMListView->uNewState & LVIS_FOCUSED ||
							(pNMListView->uOldState ^ pNMListView->uNewState) & LVIS_SELECTED) // on selection changed
						{
							int selCount = ListView_GetSelectedCount(cheatListView);
							EnableWindow(GetDlgItem(dialog, IDC_BEDIT), (selCount == 1) ? TRUE : FALSE);
							EnableWindow(GetDlgItem(dialog, IDC_BREMOVE), (selCount >= 1) ? TRUE : FALSE);
						}

						UINT oldStateImage = pNMListView->uOldState & LVIS_STATEIMAGEMASK;
						UINT newStateImage = pNMListView->uNewState & LVIS_STATEIMAGEMASK;
						if(oldStateImage != newStateImage &&
							(oldStateImage == INDEXTOSTATEIMAGEMASK(1) || oldStateImage == INDEXTOSTATEIMAGEMASK(2)) &&
							(newStateImage == INDEXTOSTATEIMAGEMASK(1) || newStateImage == INDEXTOSTATEIMAGEMASK(2))) // on checked changed
						{
							bool checked = (newStateImage == INDEXTOSTATEIMAGEMASK(2));
							cheatEditPos = pNMListView->iItem;
							cheats->get(&tempCheat, cheatEditPos);
							if ((bool)tempCheat.enabled != checked)
							{
								tempCheat.enabled = checked;
								switch (tempCheat.type)
								{
									case 0:		// internal
										cheats->update(tempCheat.size, tempCheat.code[0][0], tempCheat.code[0][1], tempCheat.description, tempCheat.enabled, cheatEditPos);
									break;

									case 1:		// Action Replay
										cheats->update_AR(NULL, NULL, tempCheat.enabled, cheatEditPos);
									break;

									case 2:		// Codebreaker
										cheats->update_CB(NULL, NULL, tempCheat.enabled, cheatEditPos);
									break;
								}
								EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
							}
						}
					}
					break;
				}

				return TRUE;
			}
		return FALSE;

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
					if (cheats->save())
						EndDialog(dialog, TRUE);
					else
						MessageBox(dialog, "Can't save cheats to file.\nCheck your path (Menu->Config->Path Settings->\"Cheats\")","Error",MB_OK);
				return TRUE;
				case IDCANCEL:
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_BADD:
				{
					searchAddAddress = 0;;
					searchAddValue = 0;
					searchAddMode = 0;
					searchAddFreeze = 1;
					if (DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_ADD), dialog, (DLGPROC) CheatsAddProc))
					{
						LVITEM lvi;
						char buf[256];

						memset(&lvi,0,sizeof(LVITEM));
						lvi.mask = LVIF_TEXT|LVIF_STATE;
						lvi.iItem = INT_MAX;

						u32 row = ListView_InsertItem(cheatListView, &lvi);
						wsprintf(buf, "0x02%06X", searchAddAddress);
						ListView_SetItemText(cheatListView, row, 1, buf);
						ltoa(searchAddValue, buf, 10);
						ListView_SetItemText(cheatListView, row, 2, buf);
						ListView_SetItemText(cheatListView, row, 3, editBuf[2]);
						ListView_SetCheckState(cheatListView, row, searchAddFreeze);
						EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
					}
				}
				return TRUE;

				case IDC_BADD_AR:
				{
					if (LOWORD(wparam) == IDC_BADD_AR)
						cheatXXtype = 0;
					else
						if (LOWORD(wparam) == IDC_BADD_CB)
							cheatXXtype = 1;
						else
							return TRUE;
					cheatXXaction = 0;				// 0 = add

					if (DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_ADD_XX_CODE), dialog, (DLGPROC) CheatsAdd_XX_Proc))
					{
						LVITEM lvi;

						memset(&lvi,0,sizeof(LVITEM));
						lvi.mask = LVIF_TEXT|LVIF_STATE;
						lvi.iItem = INT_MAX;

						u32 row = ListView_InsertItem(cheatListView, &lvi);
						if (cheatXXtype == 0)
						{
							ListView_SetItemText(cheatListView, row, 1, "Action");
							ListView_SetItemText(cheatListView, row, 2, "Replay");
						}
						else
						{
							ListView_SetItemText(cheatListView, row, 1, "Code");
							ListView_SetItemText(cheatListView, row, 2, "breaker");
						}
						ListView_SetItemText(cheatListView, row, 3, tempCheat.description);
						ListView_SetCheckState(cheatListView, row, tempCheat.enabled);

						EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
					}
				}
				return TRUE;

				case IDC_BEDIT:
				{
					cheatEditPos = ListView_GetNextItem(cheatListView, -1, LVNI_SELECTED|LVNI_FOCUSED);
					if (cheatEditPos > cheats->getSize()) return TRUE;

					cheats->get(&tempCheat, cheatEditPos);
					switch (tempCheat.type)
					{
						case 0:				// internal
							if (DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_ADD), dialog, (DLGPROC) CheatsEditProc))
							{
								char buf[256];
								cheats->get(&tempCheat, cheatEditPos);
								ListView_SetCheckState(cheatListView, cheatEditPos, tempCheat.enabled);
								wsprintf(buf, "0x02%06X", tempCheat.code[0][0]);
								ListView_SetItemText(cheatListView, cheatEditPos, 1, buf);
								ltoa(tempCheat.code[0][1], buf, 10);
								ListView_SetItemText(cheatListView, cheatEditPos, 2, buf);
								ListView_SetItemText(cheatListView, cheatEditPos, 3, tempCheat.description);
								EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
							}
						break;

						case 1:				// Action replay
						case 2:				// Codebreaker
							if (tempCheat.type == 1)
								cheatXXtype = 0;
							else
								cheatXXtype = 1;
							cheatXXaction = 1;		// 1 = edit

							if (DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_ADD_XX_CODE), dialog, (DLGPROC) CheatsAdd_XX_Proc))
							{
								cheats->get(&tempCheat, cheatEditPos);
								ListView_SetCheckState(cheatListView, cheatEditPos, tempCheat.enabled);

								if (cheatXXtype == 0)
								{
									ListView_SetItemText(cheatListView, cheatEditPos, 1, "Action");
									ListView_SetItemText(cheatListView, cheatEditPos, 2, "Replay");
								}
								else
								{
									ListView_SetItemText(cheatListView, cheatEditPos, 1, "Code");
									ListView_SetItemText(cheatListView, cheatEditPos, 2, "breaker");
								}

								ListView_SetItemText(cheatListView, cheatEditPos, 3, tempCheat.description);
								EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
							}
						break;
					}
				}
				return TRUE;

				case IDC_BREMOVE:
				{
					while(true)
					{
						int tmp_pos = ListView_GetNextItem(cheatListView, -1, LVNI_ALL | LVNI_SELECTED);
						if (tmp_pos == -1)
							break;

						if (cheats->remove(tmp_pos))
							ListView_DeleteItem(cheatListView, tmp_pos);
					}
					EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
				}
				return TRUE;

				case IDC_EXPORT:
					if (CheatsExportDialog(dialog))
						EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

void CheatsListDialog(HWND hwnd)
{
	CHEATS save = *cheats;
	tempCheat = CHEATS_LIST();
	u32 res=DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_LIST), hwnd, (DLGPROC) CheatsListBox_Proc);
	if (res)
	{
	}
	else
	{
		*cheats = save;
	}
}

void CheatsAddDialog(HWND parentHwnd, u32 address, u32 value, u8 size, const char* description)
{
	searchAddAddress = address;
	searchAddValue = value;
	searchAddMode = 2;
	searchAddFreeze = 1;
	searchAddSize = size - 1;
	searchAddDesc = description;

	if (DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_ADD), parentHwnd, (DLGPROC) CheatsAddProc))
	{
		// disabled here for now because it's modal and that's annoying.
		//CheatsListDialog(listParentHwnd);
		//
		//char buf[256];
		//cheats->get(&tempCheat, cheatEditPos);
		//ListView_SetCheckState(cheatListView, cheatEditPos, 0, tempCheat.enabled);
		//wsprintf(buf, "0x02%06X", tempCheat.code[0][0]);
		//ListView_SetItemText(cheatListView, cheatEditPos, 1, buf);
		//ltoa(tempCheat.code[0][1], buf, 10);
		//ListView_SetItemText(cheatListView, cheatEditPos, 2, buf);
		//ListView_SetItemText(cheatListView, cheatEditPos, 3, tempCheat.description);
	}
}


// ================================================================================== Search
INT_PTR CALLBACK CheatsSearchExactWnd(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			EnableWindow(hBRestart, TRUE);
			if (searchNumberResults)
				EnableWindow(hBView, TRUE);
			else
				EnableWindow(hBView, FALSE);
			EnableWindow(hBSearch, FALSE);
			
			SendMessage(GetDlgItem(dialog, IDC_EVALUE), EM_SETLIMITTEXT, 10, 0);
			SetWindowText(GetDlgItem(dialog, IDC_STATIC_RANGE), searchRangeText[searchSign][searchSize]);
			oldEditProc = SetWindowLongPtr(GetDlgItem(dialog, IDC_EVALUE), GWLP_WNDPROC, (LONG_PTR)EditValueProc);
			char buf[256];
			memset(buf, 0, 256);
			ltoa(searchNumberResults, buf, 10);
			SetWindowText(GetDlgItem(dialog, IDC_SNUMBER), buf);
			SetFocus(GetDlgItem(dialog, IDC_EVALUE));
			return TRUE;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_EVALUE:
				{
					if (HIWORD(wparam) == EN_UPDATE)
					{
						char buf[12];
						GetWindowText(GetDlgItem(dialog, IDC_EVALUE), buf, 10);
						if (!strlen(buf))
						{
							EnableWindow(hBSearch, FALSE);
							return TRUE;
						}
						
						u32 val = atol(buf);
						if (val > searchRange[searchSize][1])
						{
							EnableWindow(hBSearch, FALSE);
							return TRUE;
						}
						EnableWindow(hBSearch, TRUE);
						exactVal = val;
					}
					return TRUE;
				}
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR CALLBACK CheatsSearchCompWnd(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			EnableWindow(hBRestart, TRUE);
			if (searchNumberResults)
				EnableWindow(hBView, TRUE);
			else
				EnableWindow(hBView, FALSE);
			EnableWindow(hBSearch, TRUE);

			CheckRadioButton(dialog, searchCompIDDs[0], searchCompIDDs[ARRAY_SIZE(searchCompIDDs) - 1], searchCompIDDs[searchComp]);

			char buf[256];
			ltoa(searchNumberResults, buf, 10);
			SetWindowText(GetDlgItem(dialog, IDC_SNUMBER), buf);
			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_RADIO1: searchComp = 0; break;
				case IDC_RADIO2: searchComp = 1; break;
				case IDC_RADIO3: searchComp = 2; break;
				case IDC_RADIO4: searchComp = 3; break;
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR CALLBACK CheatsSearchResultWnd(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			EnableWindow(hBRestart, TRUE);
			if (searchNumberResults)
				EnableWindow(hBView, TRUE);
			else
				EnableWindow(hBView, FALSE);
			EnableWindow(hBSearch, FALSE);
			char buf[256];
			ltoa(searchNumberResults, buf, 10);
			SetWindowText(GetDlgItem(dialog, IDC_SNUMBER), buf);
			return TRUE;
		}
	}
	return FALSE;
}

INT_PTR CALLBACK CheatsSearchViewWnd(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
			{
				LV_COLUMN lvColumn;
				u32 address = 0;
				u32 val = 0;

				searchListView = GetDlgItem(dialog, IDC_LIST);

				ListView_SetExtendedListViewStyle(searchListView, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
				
				memset(&lvColumn,0,sizeof(LV_COLUMN));
				lvColumn.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT;
				lvColumn.fmt=LVCFMT_LEFT;
				lvColumn.cx=94;
				lvColumn.pszText="Address";
				ListView_InsertColumn(searchListView, 0, &lvColumn);
				lvColumn.cx=130;
				lvColumn.pszText="Value";
				ListView_InsertColumn(searchListView, 1, &lvColumn);

				LVITEM lvi;
				memset(&lvi,0,sizeof(LVITEM));
				lvi.mask = LVIF_TEXT|LVIF_STATE;
				lvi.iItem = INT_MAX;

				cheatSearch->getListReset();
				SendMessage(searchListView, WM_SETREDRAW, (WPARAM)FALSE,0);
				while (cheatSearch->getList(&address, &val))
				{
					char buf[256];
					wsprintf(buf, "0x02%06X", address);
					lvi.pszText= buf;
					u32 row = SendMessage(searchListView, LVM_INSERTITEM, 0, (LPARAM)&lvi);
					_ltoa(val, buf, 10);
					ListView_SetItemText(searchListView, row, 1, buf);
				}
				SendMessage(searchListView, WM_SETREDRAW, (WPARAM)TRUE,0);
				ListView_SetItemState(searchListView,0, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
				SetFocus(searchListView);
			}
		return TRUE;

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDCANCEL:
					ListView_DeleteAllItems(searchListView);
					EndDialog(dialog, FALSE);
				return TRUE;
				case IDC_BADD:
					{
						u32 val = 0;
						char buf[12];
						u32 pos = ListView_GetNextItem(searchListView, -1, LVNI_SELECTED|LVNI_FOCUSED);
						ListView_GetItemText(searchListView, pos, 0, buf, 12);
						sscanf_s(buf, "%x", &val);
						searchAddAddress = val & 0x00FFFFFF;
						ListView_GetItemText(searchListView, pos, 1, buf, 12);
						searchAddValue = atol(buf);
						searchAddMode = 1;
						searchAddSize = searchSize;
						DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_ADD), dialog, (DLGPROC) CheatsAddProc);
					}
				return TRUE;
			}
		}
	}
	return FALSE;
}

INT_PTR CALLBACK CheatsSearchMainWnd(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			CheckRadioButton(dialog, searchSizeIDDs[0], searchSizeIDDs[ARRAY_SIZE(searchSizeIDDs) - 1], searchSizeIDDs[searchSize]);
			CheckRadioButton(dialog, searchSignIDDs[0], searchSignIDDs[ARRAY_SIZE(searchSignIDDs) - 1], searchSignIDDs[searchSign]);
			CheckRadioButton(dialog, searchTypeIDDs[0], searchTypeIDDs[ARRAY_SIZE(searchTypeIDDs) - 1], searchTypeIDDs[searchType]);
			for (int i = 0; i < 4; i++)
				SetWindowText(GetDlgItem(dialog, searchRangeIDDs[i]), searchRangeText[searchSign][i]);
			EnableWindow(hBRestart, FALSE);
			EnableWindow(hBView, FALSE);
			EnableWindow(hBSearch, TRUE);
			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDC_RADIO1:		// 1 byte
					searchSize = 0;
				return TRUE;
				case IDC_RADIO2:		// 2 bytes
					searchSize = 1;
				return TRUE;
				case IDC_RADIO3:		// 3 bytes
					searchSize = 2;
				return TRUE;
				case IDC_RADIO4:		// 4 bytes
					searchSize = 3;
				return TRUE;

				case IDC_RADIO5:		// unsigned
					searchSign = 0;
					for (int i = 0; i < 4; i++)
						SetWindowText(GetDlgItem(dialog, searchRangeIDDs[i]), searchRangeText[searchSign][i]);
				return TRUE;

				case IDC_RADIO6:		//signed
					searchSign = 1;
					for (int i = 0; i < 4; i++)
						SetWindowText(GetDlgItem(dialog, searchRangeIDDs[i]), searchRangeText[searchSign][i]);
				return TRUE;

				case IDC_RADIO7:		// exact value search
					searchType = 0;
				return TRUE;

				case IDC_RADIO8:		// comparative search
					searchType = 1;
				return TRUE;
			}
			return TRUE;
		}
	}
	return FALSE;
}

DLGPROC CheatsSearchSubWnds[2][4] = {
	{ CheatsSearchMainWnd, CheatsSearchExactWnd, CheatsSearchResultWnd, NULL },
	{ CheatsSearchMainWnd, CheatsSearchResultWnd, CheatsSearchCompWnd, CheatsSearchResultWnd }
};

//==============================================================================
INT_PTR CALLBACK CheatsSearchProc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG: 
		{
			hBRestart = GetDlgItem(dialog, IDC_BRESTART);
			hBView =	GetDlgItem(dialog, IDC_BVIEW);
			hBSearch =	GetDlgItem(dialog, IDC_BSEARCH);
			
			searchWnd=CreateDialogW(hAppInst, MAKEINTRESOURCEW(searchIDDs[searchType][searchStep]), 
										dialog, (DLGPROC)CheatsSearchSubWnds[searchType][searchStep]);
			return TRUE;
		}
	
		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
				case IDCANCEL:
					if (searchWnd) DestroyWindow(searchWnd);
					EndDialog(dialog, FALSE);
				return TRUE;

				case IDC_BVIEW:
					DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_SEARCH_VIEW), dialog, (DLGPROC)CheatsSearchViewWnd);
				return TRUE;

				case IDC_BSEARCH:
					if (searchStep == 0)
						cheatSearch->start(searchType, searchSize, searchSign);
					if (searchType == 0)
					{
						if (searchStep == 1)
							searchNumberResults = cheatSearch->search((u32)exactVal);
					}
					else
					{
						if (searchStep == 2)
							searchNumberResults = cheatSearch->search((u8)searchComp);
					}

					searchStep++;
					if (searchWnd) DestroyWindow(searchWnd);
					searchWnd=CreateDialogW(hAppInst, MAKEINTRESOURCEW(searchIDDs[searchType][searchStep]), 
										dialog, (DLGPROC)CheatsSearchSubWnds[searchType][searchStep]);
					if (searchType == 0)
					{
						if (searchStep == 2) searchStep = 1;
					}
					else
					{
						if (searchStep == 1) searchStep = 2;
						if (searchStep == 3) searchStep = 2;
					}
				return TRUE;

				case IDC_BRESTART:
					cheatSearch->close();
					searchStep = 0;
					searchNumberResults = 0;
					if (searchWnd) DestroyWindow(searchWnd);
					searchWnd=CreateDialogW(hAppInst, MAKEINTRESOURCEW(searchIDDs[searchType][searchStep]), 
										dialog, (DLGPROC)CheatsSearchSubWnds[searchType][searchStep]);
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

void CheatsSearchDialog(HWND hwnd)
{
	DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_SEARCH), hwnd, (DLGPROC) CheatsSearchProc);
}

void CheatsSearchReset()
{
	searchType = 0;
	searchSize = 0;
	searchSign = 0;
	searchStep = 0;
	searchComp = 0;
	searchAddSize = 0;
	exactVal = 0;
	searchNumberResults = 0;
}

void CheatAddVerify(HWND dialog,char* addre, char* valu,u8 size)
{
	u32 fix = 0;
	sscanf_s(addre, "%x", &fix);
	fix &= 0x00FFFFFF;

	int parseOffset = 0;
	if(valu[0] && valu[1] == '-')
		parseOffset = 1; // typed something in front of -
	u32 fix2 = strtoul(valu+parseOffset,NULL,10);

	if ( (strlen(addre) < 6) || (!strlen(valu)) || fix > 0x400000
		|| (fix2 > searchRange[size][1] && !(valu[0] == '-' && u32(-s32(fix2))-1 <= searchRange[size][1]/2)) )
	{
		EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
	}
	else
		EnableWindow(GetDlgItem(dialog, IDOK), TRUE);

	generateAR(dialog, fix, fix2, size);
}


// ============================================================= Export
INT_PTR CALLBACK CheatsExportProc(HWND dialog, UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			SetWindowText(GetDlgItem(dialog, IDC_CDATE), (LPCSTR)cheatsExport->date);
			if (cheatsExport->gametitle && strlen((char*)cheatsExport->gametitle) > 0)
			{
				char buf[512] = {0};
				GetWindowText(dialog, &buf[0], sizeof(buf));
				strcat(buf, ": ");
				strcat(buf, (char*)cheatsExport->gametitle);
				SetWindowText(dialog, (LPCSTR)buf);
			}
			LV_COLUMN lvColumn;
			exportListView = GetDlgItem(dialog, IDC_LIST_CHEATS);
			ListView_SetExtendedListViewStyle(exportListView, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_CHECKBOXES);
			
			memset(&lvColumn,0,sizeof(LV_COLUMN));
			lvColumn.mask=LVCF_FMT|LVCF_TEXT|LVCF_WIDTH;
			lvColumn.fmt=LVCFMT_LEFT;
			lvColumn.cx=1000;
			lvColumn.pszText="Cheats";
			ListView_InsertColumn(exportListView, 0, &lvColumn);

			LVITEM lvi;
			memset(&lvi,0,sizeof(LVITEM));
			lvi.mask = LVIF_TEXT|LVIF_STATE;
			lvi.iItem = INT_MAX;

			SendMessage(exportListView, WM_SETREDRAW, (WPARAM)FALSE,0);
			for (u32 i = 0; i < cheatsExport->getCheatsNum(); i++)
			{
				CHEATS_LIST *tmp = (CHEATS_LIST*)cheatsExport->getCheats();
				lvi.pszText= tmp[i].description;
				SendMessage(exportListView, LVM_INSERTITEM, 0, (LPARAM)&lvi);
			}
			if (gameInfo.crc != cheatsExport->CRC)
			{
				//MessageBox(dialog, "WARNING!\n Checksums not matching.",EMU_DESMUME_NAME_AND_VERSION(), MB_OK);// | MB_ICON_ERROR);
			}
			SendMessage(exportListView, WM_SETREDRAW, (WPARAM)TRUE,0);
			ListView_SetItemState(exportListView,0, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
			SetFocus(exportListView);
			break;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wparam))
			{
				case IDOK:
					{
						u32 count = ListView_GetItemCount(exportListView);
						if (count > 0)
						{
							bool done = false;
							CHEATS_LIST *tmp = (CHEATS_LIST*)cheatsExport->getCheats();
							for (u32 i = 0; i < count; i++)
							{
								if (!ListView_GetCheckState(exportListView, i)) continue;
								cheats->add_AR_Direct(tmp[i]);

								LVITEM lvi;

								memset(&lvi,0,sizeof(LVITEM));
								lvi.mask = LVIF_TEXT|LVIF_STATE;
								lvi.iItem = INT_MAX;
								lvi.pszText= " ";
								u32 row = ListView_InsertItem(cheatListView, &lvi);
								ListView_SetItemText(cheatListView, row, 1, "Action");
								ListView_SetItemText(cheatListView, row, 2, "Replay");
								ListView_SetItemText(cheatListView, row, 3, tmp[i].description);
								done = true;
							}
							if (done) EndDialog(dialog, TRUE);
						}
					}
					break;

				case IDCANCEL:
					EndDialog(dialog, FALSE);
					break;
			}
			break;
		}
	}
	return FALSE;
}

bool CheatsExportDialog(HWND hwnd)
{
	bool res = false;
	cheatsExport = new CHEATSEXPORT();
	if (!cheatsExport) return false;
	
	char buf[MAX_PATH] = {0};
	strcpy(buf, path.getpath(path.CHEATS).c_str());
	if (path.r4Format == path.R4_CHEAT_DAT)
		strcat(buf,"cheat.dat");
	else
		if (path.r4Format == path.R4_USRCHEAT_DAT)
			strcat(buf,"usrcheat.dat");
		else return false;
	if (cheatsExport->load(buf))
	{
		if (cheatsExport->getCheatsNum() > 0)
			res = DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_CHEAT_EXPORT), hwnd, (DLGPROC) CheatsExportProc);
		else
			MessageBox(hwnd, "Cheats for this game in database not founded.", "DeSmuME", MB_OK | MB_ICONERROR);
	}
	else
	{
		char buf2[512] = {0};
		if (cheatsExport->getErrorCode() == 1)
			sprintf(buf2, "Error loading cheats database. File not found\n\"%s\"\nCheck your path (Menu->Config->Path Settings->\"Cheats\")\n\nYou can download it from http://www.codemasters-project.net/vb/forumdisplay.php?44-Nintendo-DS", buf);
		else
			if (cheatsExport->getErrorCode() == 2)
				sprintf(buf2, "File \"%s\" is not R4 cheats database.\nWrong file format!", buf);
			else
				if (cheatsExport->getErrorCode() == 3)
					sprintf(buf2, "Serial \"%s\" not found in database.", gameInfo.ROMserial);
				else
					if (cheatsExport->getErrorCode() == 4)
						sprintf(buf2, "Error export from database");
					else
						sprintf(buf2, "Unknown error!!!");
		MessageBox(hwnd, buf2, "DeSmuME", MB_OK | MB_ICONERROR);				
	}

	cheatsExport->close();
	delete cheatsExport;
	cheatsExport = NULL;

	return res;
}

