/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2008 DeSmuME team

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
#define DIRECTINPUT_VERSION 0x0800

#include "inputdx.h"
#include "directx\dinput.h"
#include "..\mem.h"
#include "..\debug.h"
#include "..\MMU.h"
#include "..\common.h"
#include "resource.h"
#include "NDSSystem.h"

// ==================================================== emu input
// =======================================================================
#define IDD_INPUT_TIMER 1000000

const	char	*DIkeysNames[0xEF] = 
{
	"N/A",
	// Main keyboard 0x01
	"ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "BSPACE", 
	"Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", 
	"LCtrl", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "\'", "GRAVE", "LShift", "\\", 
	"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "RShift", 
	// Numeric keypad 0x37
	"Num *", 
	// Main keyboard 0x38
	"LAlt", "SPACE", "~CAP", 
	"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", 
	// Numeric keypad 0x45
	"NumLock", "Scrool", "Num 7", "Num 8", "Num 9", 
	"Num -", "Num 4", "Num 5", "Num 6", "Num +", 
	"Num 1", "Num 2", "Num 3", "Num 0", "Num .", 
	// Reserved 0x54
	"", "", 
	// non US keyboards 0x56
	"oem", 
	// func keyboard 0x57
	"F11", "F12", 
	// Reserved 0x59
	"", "", "", "", "", "", "", "", "", "", "", 
	// PC98 0x64
	"F13", "F14", "F15", 
	// Reserved 0x67
	"", "", "", "", "", "", "", "", "", 
	// Japanise 0x70
	"KANA", 
	// Reserved 0x71
	"", "", 
	// Brazilian 0x73
	"ABNT_C1", 
	// Reserved 0x74
	"", "", "", "", "",  
	// Japanise 0x79
	"Convert", 
	// Reserved 0x7A
	"", 
	// Japanise 0x7B
	"Noconvert", 
	// Reserved 0x7C
	"", 
	// Japanise 0x7D
	"Yen", 
	// Brazilian 0x7E
	"ABNT_C2", 
	// Reserved 0x7F
	"", "", "", "", "", "", "", "", "", "", "", "", "", "", 
	// PC98 0x8D
	"N =",
	// Reserved 0x8E
	"", "", 
	// ext 0x90
	"PTrack",
	// PC98 0x91
	"AT", "Colon", "Underline", 
	// Japanise 0x94
	"Kanji", 
	// PC98 0x95
	"Stop", 
	// Japanise 0x96
	"AX", "Unlab", 
	// Reseved 0x98
	"", 
	// ext 0x99
	"NTrack", 
	// Reserved 0x9A
	"", "", 
	// Numeric keypad 0x9C
	"Num enter", "RCtrl", 
	// Reserved 0x9E
	"", "", 
	// ext 0xA0
	"Mute", "Calc", "Play", 
	// Reserved 0xA3
	"", 
	// ext 0xA4
	"Stop", 
	// Reserved 0xA5
	"", "", "", "", "", "", "", "", "", 
	// ext 0xAE
	"Vol-", 
	// Reserved 0xAF
	"", 
	// ext 0xB0
	"Vol+", 
	// Reserved 0xB1
	"", 
	// Web 0xB2
	"Web Home", 
	// Numeric keypad (PC98) 0xB3
	"Num ,",
	// Reserved 0xB4
	"", 
	// Numeric keypad 0xB5
	"Num /", 
	// Reserved 0xB6
	"", 
	// Main keyboard 0xB7
	"SysRq", "RAlt", 
	// Reserved 0xB9
	"", "", "", "", "", "", "", "", "", "", "", "",
	// Main keyboard 0xC5
	"Pause", 
	// Reserved 0xC6
	"", 
	// Main keyboard 0xC7
	"Home", "Up", "PgUp", 
	// Reserved 0xCA
	"", 
	// Main keyboard 0xCB
	"Left", 
	// Reserved 0xCC
	"", 
	// Main keyboard 0xCD
	"Right", 
	// Reserved 0xCE
	"", 
	// Main keyboard 0xCF
	"End", "Down", "PgDn", "Insert", "Delete", 
	// Reserved 0xD4
	"", "", "", "", "", "", "", "",
	// Main keyboard 0xDB
	"LWin", "RWin", "App", "Power", "Sleep", 
	// Reversed 0xE0
	"", "", "", 
	// Main keyboard 0xE3
	"Wake", 
	// Reversed 0xE4
	"", 
	// Web 0xE5
	"Web Search", "Web Favorites", "Web Refresh", "Web Stop", "Web Forward", "Web Back", 
	"My Computer", "Mail", "Media Select"
	// 0xEE
};
const	char	*DIJoyNames[0x04] = { "JUp", "JDown", "JLeft", "JRight" };

#define KEY_A 0
#define KEY_B 1
#define KEY_SELECT 2
#define KEY_START 3
#define KEY_RIGHT 4
#define KEY_LEFT 5
#define KEY_UP 6
#define KEY_DOWN 7
#define KEY_R 8
#define KEY_L 9
#define KEY_X 10
#define KEY_Y 11
#define KEY_DEBUG 12


char	*keyPadNames [MAXKEYPAD] = { "A", "B", "SELECT", "START", 
								"RIGHT", "LEFT", "UP", "DOWN", 
								"R", "L", "X", "Y", "DEBUG", "FOLD", "POWER" };

u16	keyPadDefs[MAXKEYPAD] = {DIK_X, DIK_Z, DIK_RSHIFT, DIK_RETURN, DIK_RIGHT,
							DIK_LEFT, DIK_UP, DIK_DOWN, DIK_W, DIK_Q,
							DIK_S, DIK_A, 0x00, DIK_BACKSPACE, DIK_PAUSE};

const int	inputIDs[15]={ IDC_EDIT06, IDC_EDIT05, IDC_EDIT11, IDC_EDIT12, IDC_EDIT03, IDC_EDIT02, IDC_EDIT01, 
							IDC_EDIT04, IDC_EDIT10, IDC_EDIT09, IDC_EDIT08, IDC_EDIT07, IDC_EDIT14, IDC_EDIT13,
							IDC_EDIT15};


u16				keyPad[15];
extern INPUTCLASS	*input;

// ==================================================== Config Input
INPUTCLASS		*inputCfg = NULL;
HWND			g_hWnd = NULL;
static int		pressed;
static bool		tab;
u16				tempKeyPad[MAXKEYPAD];

void InputConfigDIProc(BOOL paused, LPSTR buf)
{
	int t;
	int i;

	if (pressed == 0)
	{
		for (t=0; t<512; t++)
		{
			if (t == DIK_ESCAPE) continue;
			if (t == DIK_TAB) continue;
			if (t == DIK_LMENU) continue;
			if (t == DIK_F1) continue;
			if (t == DIK_F2) continue;
			if (t == DIK_F3) continue;
			if (t == DIK_F4) continue;
			if (t == DIK_F5) continue;
			if (t == DIK_F6) continue;
			if (t == DIK_F7) continue;
			if (t == DIK_F8) continue;
			if (t == DIK_F9) continue;
			if (t == DIK_F10) continue;
			if (t == DIK_F11) continue;
			if (t == DIK_F12) continue;
			if (t == DIK_NUMLOCK) continue;

			if (buf[t] & 0x80)
			{
				pressed = t;
				break;
			}
		}
	}
	else
	{
		if ((pressed == DIK_LSHIFT) && ((buf[DIK_TAB] & 0x80))) tab = true;
		if ((pressed == DIK_RSHIFT) && ((buf[DIK_TAB] & 0x80))) tab = true;

		if (!(buf[pressed] & 0x80))
		{
			if (!tab)
			{
				if (pressed>255)
				{
					if (pressed>255 && pressed<260)
					{
						SetWindowText(GetFocus(), DIJoyNames[pressed-256]);
					}
					else
					{
						char buf[20];
						memset(buf, 0, sizeof(buf));
						wsprintf(buf, "JB%02i", pressed-259);
						SetWindowText(GetFocus(), buf);
					}
				}
				else
				{
					SetWindowText(GetFocus(), DIkeysNames[pressed]);
				}
				for (i=0; i<MAXKEYPAD; i++)
					if (GetDlgCtrlID(GetFocus()) == inputIDs[i])
					{
						tempKeyPad[i] = pressed;

						HWND tmp = GetNextDlgTabItem(g_hWnd, GetDlgItem(g_hWnd,inputIDs[i]), false);
						if (GetDlgCtrlID(tmp) == IDOK || GetDlgCtrlID(tmp) == IDCANCEL)
							SetFocus(GetDlgItem(g_hWnd,inputIDs[6]));
						else
							SetFocus(tmp);
						break;
					}
			}
			tab = false;
			pressed = 0;
		}
	}
}

BOOL CALLBACK InputConfigDlgProc(   HWND hDlg, 
                              UINT uMessage, 
                              WPARAM wParam, 
                              LPARAM lParam)
{
	switch (uMessage)
	{
		case WM_INITDIALOG:
			g_hWnd = hDlg;
			for (int i=0; i<MAXKEYPAD; i++)
			{
				if (tempKeyPad[i]>255)
				{
					if (tempKeyPad[i]>255 && tempKeyPad[i]<260)
					{
						SetWindowText(GetDlgItem(hDlg, inputIDs[i]), DIJoyNames[tempKeyPad[i]-256]);
					}
					else
					{
						char buf[20];
						memset(buf, 0, sizeof(buf));
						wsprintf(buf, "JB%02i", tempKeyPad[i]-259);
						SetWindowText(GetDlgItem(hDlg, inputIDs[i]), buf);
					}
				}
				else
					SetWindowText(GetDlgItem(hDlg, inputIDs[i]), DIkeysNames[tempKeyPad[i]]);
			}
			
			if (!inputCfg->Init(hDlg, &InputConfigDIProc))
				printlog("Input config: Error initialize DirectInput\n");
			SetTimer(hDlg, IDD_INPUT_TIMER, 100, NULL);
			return true;

		case WM_TIMER:
			inputCfg->process();
			return true;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			 {
				 case IDOK:
					 if (GetFocus() == GetDlgItem(hDlg, IDOK))
					 {
						for (int t=0; t<MAXKEYPAD; t++)
						{
							char buf[64];
							memset(buf, 0, sizeof(buf));
							keyPad[t] = tempKeyPad[t];
							wsprintf(buf,"Key_%s", keyPadNames[t]);
							WritePrivateProfileInt("NDS_Input",buf,keyPad[t],IniName);
						}
						EndDialog(hDlg, IDOK);
						return false;
					}
					return true;

				 case IDCANCEL:
					EndDialog(hDlg, IDOK);
					break;
			 }
			return true;
	} 

	return DefWindowProc( hDlg, uMessage, wParam, lParam);
}

void InputConfig(HWND hwnd)
{
	inputCfg = new INPUTCLASS();
	if (inputCfg !=NULL)
	{
		pressed = 0;
		tab = 0;
		for (int t=0; t<MAXKEYPAD; t++)
			tempKeyPad[t] = keyPad[t];
		DialogBox(hAppInst,MAKEINTRESOURCE(IDD_INPUT), hwnd, (DLGPROC) InputConfigDlgProc);
		delete inputCfg;
	}
	else
		printlog("Input config: Error create DI class\n");
	inputCfg = NULL;
}
// =============================================== end Config input

void NDS_inputInit()
{
	int i;
	memset(keyPad, 0, sizeof(keyPad));
	
	for (i=0; i < MAXKEYPAD; i++)
	{
		char buf[64];
		memset(buf, 0, sizeof(buf));
		wsprintf(buf,"Key_%s", keyPadNames[i]);
		keyPad[i] = GetPrivateProfileInt("NDS_Input",buf,keyPadDefs[i], IniName);
		if (keyPad[i]>255)
		{
			if (!input->JoystickEnabled())
			{
				keyPad[i] = keyPadDefs[i];
			}
		}
	}
}

void NDS_inputPost(BOOL paused, LPSTR buf)
{
	if (paused) return;

	bool R = (buf[keyPad[KEY_RIGHT]] & 0x80)!=0;
	bool L = (buf[keyPad[KEY_LEFT]] & 0x80)!=0;
	bool D = (buf[keyPad[KEY_DOWN]] & 0x80)!=0;
	bool U = (buf[keyPad[KEY_UP]] & 0x80)!=0;
	bool T = (buf[keyPad[KEY_START]] & 0x80)!=0;
	bool S = (buf[keyPad[KEY_SELECT]] & 0x80)!=0;
	bool B = (buf[keyPad[KEY_B]] & 0x80)!=0;
	bool A = (buf[keyPad[KEY_A]] & 0x80)!=0;
	bool Y = (buf[keyPad[KEY_Y]] & 0x80)!=0;
	bool X = (buf[keyPad[KEY_X]] & 0x80)!=0;
	bool W = (buf[keyPad[KEY_L]] & 0x80)!=0;
	bool E = (buf[keyPad[KEY_R]] & 0x80)!=0;
	bool G = (buf[keyPad[KEY_DEBUG]] & 0x80)!=0;

	NDS_setPad( R, L, D, U, T, S, B, A, Y, X, W, E, G);
}

// TODO
// ==================================================== GUI input
// =======================================================================


// ==================================================== INPUTCLASS
// ===============================================================
// ===============================================================
// ===============================================================
INPUTCLASS::INPUTCLASS()
{
	hParentWnd = NULL;
	inputProc = NULL;
}

INPUTCLASS::~INPUTCLASS()
{
	if (pDI != NULL)
	{
		if (pKeyboard != NULL)
		{
			IDirectInputDevice8_Unacquire(pKeyboard);
			IDirectInputDevice8_Release(pKeyboard);
			pKeyboard = NULL;
		}

		if (pJoystick != NULL)
		{
			IDirectInputDevice8_Unacquire(pJoystick);
			IDirectInputDevice8_Release(pJoystick);
			pJoystick = NULL;
		}
	}
}

BOOL INPUTCLASS::Init(HWND hParentWnd, INPUTPROC inputProc)
{
	if (hParentWnd == NULL) return FALSE;
	if (inputProc == NULL) return FALSE;

	this->hParentWnd = hParentWnd;

	pDI = NULL;
	pKeyboard = NULL;
	pJoystick = NULL;
	memset(cDIBuf, 0, sizeof(cDIBuf));

	if(FAILED(DirectInput8Create(GetModuleHandle(NULL),DIRECTINPUT_VERSION,IID_IDirectInput8,(void**)&pDI,NULL)))
		return FALSE;

	if (!FAILED(IDirectInput8_CreateDevice(pDI,GUID_SysKeyboard,&pKeyboard,NULL)))
	{
		if (!FAILED(IDirectInputDevice8_SetDataFormat(pKeyboard,&c_dfDIKeyboard)))
		{
			if (FAILED(IDirectInputDevice8_SetCooperativeLevel(pKeyboard,hParentWnd,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE)))
			{
				IDirectInputDevice8_Release(pKeyboard);
				pKeyboard = NULL;
			}
		}
		else
		{
			IDirectInputDevice8_Release(pKeyboard);
			pKeyboard = NULL;
		}
	}

	if (!FAILED(IDirectInput8_CreateDevice(pDI,GUID_Joystick,&pJoystick,NULL)))
	{
		if(!FAILED(IDirectInputDevice8_SetDataFormat(pJoystick,&c_dfDIJoystick2)))
		{
			if(FAILED(IDirectInputDevice8_SetCooperativeLevel(pJoystick,hParentWnd,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE)))
			{
				IDirectInputDevice8_Release(pJoystick);
				pJoystick = NULL;
			}
			else
			{
				memset(&DIJoycap,0,sizeof(DIDEVCAPS));
				DIJoycap.dwSize=sizeof(DIDEVCAPS);
				IDirectInputDevice8_GetCapabilities(pJoystick,&DIJoycap);
			}
		}
		else
		{
			IDirectInputDevice8_Release(pJoystick);
			pJoystick = NULL;
		}
	}

	if (pKeyboard == NULL && pJoystick == NULL) return FALSE;

	this->inputProc = inputProc;

#if 1
	if (pKeyboard != NULL) printlog("DirectX Input: keyboard is initialised\n");
	if (pJoystick != NULL) printlog("DirectX Input: joystick is initialised\n");
#endif
	paused = FALSE;

	return TRUE;
}

BOOL INPUTCLASS::JoystickEnabled()
{
	return (pJoystick==NULL?FALSE:TRUE);
}

void INPUTCLASS::process()
{
	HRESULT hr;

	if (paused) return;
	if (inputProc == NULL) return;

	if (pKeyboard)
	{
		hr=IDirectInputDevice8_GetDeviceState(pKeyboard,256,cDIBuf);
		if (FAILED(hr)) 
		{
			//printlog("DInput: keyboard acquire\n");
			IDirectInputDevice8_Acquire(pKeyboard);
		}
	}

	if (pJoystick)
	{
		DIJOYSTATE2 JoyStatus;

		hr=IDirectInputDevice8_Poll(pJoystick);
		if (FAILED(hr))	IDirectInputDevice8_Acquire(pJoystick);
		else
		{
			hr=IDirectInputDevice8_GetDeviceState(pJoystick,sizeof(JoyStatus),&JoyStatus);
			if (FAILED(hr)) hr=IDirectInputDevice8_Acquire(pJoystick);
			else
			{
				memset(cDIBuf+255,0,sizeof(cDIBuf)-255);
				//TODO: analog
				//if (JoyStatus.lX<-1) cDIBuf[258]=-128;
				//if (JoyStatus.lX>1) cDIBuf[259]=-128;
				//if (JoyStatus.lY<-1) cDIBuf[256]=-128;
				//if (JoyStatus.lY>1) cDIBuf[257]=-128;
				
				if (JoyStatus.rgdwPOV[0]==0) cDIBuf[256]=-128;
				if (JoyStatus.rgdwPOV[0]==4500) { cDIBuf[256]=-128; cDIBuf[259]=-128;}
				if (JoyStatus.rgdwPOV[0]==9000) cDIBuf[259]=-128;
				if (JoyStatus.rgdwPOV[0]==13500) { cDIBuf[259]=-128; cDIBuf[257]=-128;}
				if (JoyStatus.rgdwPOV[0]==18000) cDIBuf[257]=-128;
				if (JoyStatus.rgdwPOV[0]==22500) { cDIBuf[257]=-128; cDIBuf[258]=-128;}
				if (JoyStatus.rgdwPOV[0]==27000) cDIBuf[258]=-128;
				if (JoyStatus.rgdwPOV[0]==31500) { cDIBuf[258]=-128; cDIBuf[256]=-128;}
				memcpy(cDIBuf+260,JoyStatus.rgbButtons,sizeof(JoyStatus.rgbButtons));
			}
		}
	}

	this->inputProc(paused, (LPSTR)&cDIBuf);
}
// ==================================================== END INPUTCLASS