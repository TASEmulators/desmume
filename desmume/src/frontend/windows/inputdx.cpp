/*
	This file is part of DeSmuME, derived from several files in Snes9x 1.51 which are 
	licensed under the terms supplied at the end of this file (for the terms are very long!)
	Differences from that baseline version are:

	Copyright (C) 2009-2016 DeSmuME team

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

//TODO - rumble is broken. hopefully nobody will notice

#include "inputdx.h"

#ifdef __MINGW32__
#define _WIN32_IE 0x0501
#define _WIN32_WINNT 0x0501
#endif

#include <tchar.h>
#include <io.h>
#include <string>

#if (((defined(_MSC_VER) && _MSC_VER >= 1300)) || defined(__MINGW32__))
	// both MINGW and VS.NET use fstream instead of fstream.h which is deprecated
	#include <fstream>
	using namespace std;
#else
	// for VC++ 6
	#include <fstream.h>
#endif

#include "types.h"
#include "NDSSystem.h"
#include "slot2.h"
#include "debug.h"

#include "resource.h"
#include "hotkey.h"
#include "main.h"
#include "winutil.h"

// Gamepad Dialog Strings
// Support Unicode display
//#define INPUTCONFIG_TITLE "Input Configuration"
#define INPUTCONFIG_JPTOGGLE "Enabled"
//#define INPUTCONFIG_DIAGTOGGLE "Toggle Diagonals"
//#define INPUTCONFIG_OK "&OK"
//#define INPUTCONFIG_CANCEL "&Cancel"
#define INPUTCONFIG_JPCOMBO "Joypad #%d"
// Support Unicode display
#define INPUTCONFIG_LABEL_UP IDC_LABEL_UP
#define INPUTCONFIG_LABEL_DOWN IDC_LABEL_DOWN
#define INPUTCONFIG_LABEL_LEFT IDC_LABEL_LEFT
#define INPUTCONFIG_LABEL_RIGHT IDC_LABEL_RIGHT
#define INPUTCONFIG_LABEL_A IDC_LABEL_A
#define INPUTCONFIG_LABEL_B IDC_LABEL_B
#define INPUTCONFIG_LABEL_X IDC_LABEL_X
#define INPUTCONFIG_LABEL_Y IDC_LABEL_Y
#define INPUTCONFIG_LABEL_L IDC_LABEL_L
#define INPUTCONFIG_LABEL_R IDC_LABEL_R
#define INPUTCONFIG_LABEL_START IDC_LABEL_START
#define INPUTCONFIG_LABEL_SELECT IDC_LABEL_SELECT
#define INPUTCONFIG_LABEL_UPLEFT IDC_LABEL_UPLEFT
#define INPUTCONFIG_LABEL_UPRIGHT IDC_LABEL_UPRIGHT
#define INPUTCONFIG_LABEL_DOWNRIGHT IDC_LABEL_DOWNRIGHT
#define INPUTCONFIG_LABEL_DOWNLEFT IDC_LABEL_DOWNLEFT
#define INPUTCONFIG_LABEL_BLUE IDC_LABEL_BLUE //"Blue means the button is already mapped.\nPink means it conflicts with a custom hotkey.\nRed means it's reserved by Windows.\nButtons can be disabled using Escape.\nGrayed buttons arent supported yet (sorry!)"

#define INPUTCONFIG_LABEL_UNUSED ""
#define INPUTCONFIG_LABEL_CLEAR_TOGGLES_AND_TURBO "Clear All"
#define INPUTCONFIG_LABEL_MAKE_TURBO "TempTurbo"
#define INPUTCONFIG_LABEL_MAKE_HELD "Autohold"
#define INPUTCONFIG_LABEL_MAKE_TURBO_HELD "Autofire"
#define INPUTCONFIG_LABEL_CONTROLLER_TURBO_PANEL_MOD " Turbo"

// Hotkeys Dialog Strings
#define HOTKEYS_TITLE "Hotkey Configuration"
#define HOTKEYS_CONTROL_MOD "Ctrl + "
#define HOTKEYS_SHIFT_MOD "Shift + "
#define HOTKEYS_ALT_MOD "Alt + "
// Support Unicode display
#define HOTKEYS_LABEL_BLUE IDC_LABEL_BLUE1 //"Blue means the hotkey is already mapped.\nPink means it conflicts with a game button.\nRed means it's reserved by Windows.\nA hotkey can be disabled using Escape."
#define HOTKEYS_HKCOMBO "Page %d"

// gaming buttons and axes
#define GAMEDEVICE_JOYNUMPREFIX "(J%x)" // don't change this
#define GAMEDEVICE_JOYBUTPREFIX "#[%d]" // don't change this
#define GAMEDEVICE_XNEG "Left"
#define GAMEDEVICE_XPOS "Right"
#define GAMEDEVICE_YPOS "Up"
#define GAMEDEVICE_YNEG "Down"
#define GAMEDEVICE_POVLEFT "POV Left"
#define GAMEDEVICE_POVRIGHT "POV Right"
#define GAMEDEVICE_POVUP "POV Up"
#define GAMEDEVICE_POVDOWN "POV Down"
#define GAMEDEVICE_POVDNLEFT "POV Dn Left"
#define GAMEDEVICE_POVDNRIGHT "POV Dn Right"
#define GAMEDEVICE_POVUPLEFT  "POV Up Left"
#define GAMEDEVICE_POVUPRIGHT "POV Up Right"
#define GAMEDEVICE_ZPOS "Z +"
#define GAMEDEVICE_ZNEG "Z -"
#define GAMEDEVICE_RPOS "R Up"
#define GAMEDEVICE_RNEG "R Down"
#define GAMEDEVICE_UPOS "U Up"
#define GAMEDEVICE_UNEG "U Down"
#define GAMEDEVICE_VPOS "V Up"
#define GAMEDEVICE_VNEG "V Down"
#define GAMEDEVICE_BUTTON "Button %d"

#define GAMEDEVICE_XROTPOS "X Rot Up"
#define GAMEDEVICE_XROTNEG "X Rot Down"
#define GAMEDEVICE_YROTPOS "Y Rot Up"
#define GAMEDEVICE_YROTNEG "Y Rot Down"
#define GAMEDEVICE_ZROTPOS "Z Rot Up"
#define GAMEDEVICE_ZROTNEG "Z Rot Down"

// gaming general
#define GAMEDEVICE_DISABLED "Disabled"

// gaming keys
#define GAMEDEVICE_KEY "#%d"
#define GAMEDEVICE_NUMPADPREFIX "Numpad-%c"
#define GAMEDEVICE_VK_TAB "Tab"
#define GAMEDEVICE_VK_BACK "Backspace"
#define GAMEDEVICE_VK_CLEAR "Delete"
#define GAMEDEVICE_VK_RETURN "Enter"
#define GAMEDEVICE_VK_LSHIFT "LShift"
#define GAMEDEVICE_VK_RSHIFT "RShift"
#define GAMEDEVICE_VK_LCONTROL "LCtrl"
#define GAMEDEVICE_VK_RCONTROL "RCtrl"
#define GAMEDEVICE_VK_LMENU "LAlt"
#define GAMEDEVICE_VK_RMENU "RAlt"
#define GAMEDEVICE_VK_PAUSE "Pause"
#define GAMEDEVICE_VK_CAPITAL "Capslock"
#define GAMEDEVICE_VK_ESCAPE "Disabled"
#define GAMEDEVICE_VK_SPACE "Space"
#define GAMEDEVICE_VK_PRIOR "PgUp"
#define GAMEDEVICE_VK_NEXT "PgDn"
#define GAMEDEVICE_VK_HOME "Home"
#define GAMEDEVICE_VK_END "End"
#define GAMEDEVICE_VK_LEFT "Left"
#define GAMEDEVICE_VK_RIGHT "Right"
#define GAMEDEVICE_VK_UP "Up"
#define GAMEDEVICE_VK_DOWN "Down"
#define GAMEDEVICE_VK_SELECT "Select"
#define GAMEDEVICE_VK_PRINT "Print"
#define GAMEDEVICE_VK_EXECUTE "Execute"
#define GAMEDEVICE_VK_SNAPSHOT "SnapShot"
#define GAMEDEVICE_VK_INSERT "Insert"
#define GAMEDEVICE_VK_DELETE "Delete"
#define GAMEDEVICE_VK_HELP "Help"
#define GAMEDEVICE_VK_LWIN "LWinKey"
#define GAMEDEVICE_VK_RWIN "RWinKey"
#define GAMEDEVICE_VK_APPS "AppKey"
#define GAMEDEVICE_VK_MULTIPLY "Numpad *"
#define GAMEDEVICE_VK_ADD "Numpad +"
#define GAMEDEVICE_VK_SEPARATOR "Separator"
#define GAMEDEVICE_VK_OEM_1 "Semi-Colon"
#define GAMEDEVICE_VK_OEM_7 "Apostrophe"
#define GAMEDEVICE_VK_OEM_COMMA "Comma"
#define GAMEDEVICE_VK_OEM_PERIOD "Period"
#define GAMEDEVICE_VK_SUBTRACT "Numpad -"
#define GAMEDEVICE_VK_DECIMAL "Numpad ."
#define GAMEDEVICE_VK_DIVIDE "Numpad /"
#define GAMEDEVICE_VK_NUMLOCK "Num-lock"
#define GAMEDEVICE_VK_SCROLL "Scroll-lock"
#define GAMEDEVICE_VK_OEM_MINUS "-"
#define GAMEDEVICE_VK_OEM_PLUS "="
#define GAMEDEVICE_VK_SHIFT "Shift"
#define GAMEDEVICE_VK_CONTROL "Control"
#define GAMEDEVICE_VK_MENU "Alt"
#define GAMEDEVICE_VK_OEM_4 "["
#define GAMEDEVICE_VK_OEM_6 "]"
#define GAMEDEVICE_VK_OEM_5 "\\"
#define GAMEDEVICE_VK_OEM_2 "/"
#define GAMEDEVICE_VK_OEM_3 "`"
#define GAMEDEVICE_VK_F1 "F1"
#define GAMEDEVICE_VK_F2 "F2"
#define GAMEDEVICE_VK_F3 "F3"
#define GAMEDEVICE_VK_F4 "F4"
#define GAMEDEVICE_VK_F5 "F5"
#define GAMEDEVICE_VK_F6 "F6"
#define GAMEDEVICE_VK_F7 "F7"
#define GAMEDEVICE_VK_F8 "F8"
#define GAMEDEVICE_VK_F9 "F9"
#define GAMEDEVICE_VK_F10 "F10"
#define GAMEDEVICE_VK_F11 "F11"
#define GAMEDEVICE_VK_F12 "F12"
// Support Unicode display
#define BUTTON_OK L"&OK"
#define BUTTON_CANCEL L"&Cancel"

static TCHAR szClassName[] = _T("InputCustom");
static TCHAR szHotkeysClassName[] = _T("InputCustomHot");
static TCHAR szGuitarClassName[] = _T("InputCustomGuitar");
static TCHAR szPaddleClassName[] = _T("InputCustomPaddle");

static LRESULT CALLBACK InputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK HotInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK GuitarInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK PaddleInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

SJoyState Joystick [16];
SJoyState JoystickF [16];
SJoypad ToggleJoypadStorage[8];
SJoypad TurboToggleJoypadStorage[8];
u32 joypads [8];

//the main input configuration:
SJoypad DefaultJoypad[16] = {
    {
        true,					/* Joypad 1 enabled */
			VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,	/* Left, Right, Up, Down */
			0, 0, 0, 0,             /* Left_Up, Left_Down, Right_Up, Right_Down */
			VK_RETURN, VK_RSHIFT,    /* Start, Select */
			0, 0,					/* Lid, Debug */
			'X', 'Z',				/* A B */
			'S', 'A',				/* X Y */
			'Q', 'W'				/* L R */
    },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 },
	{ false, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0, 0, 0, 0, 0, 0 }
};

SJoypad Joypad[16];

SGuitar Guitar;
SGuitar DefaultGuitar = { false, 'E', 'R', 'T', 'Y' };

SPiano Piano;
SPiano DefaultPiano = { false, 'Z', 'S', 'X', 'D', 'C', 'V', 'G', 'B', 'H', 'N', 'J', 'M', VK_OEM_COMMA };

SPaddle Paddle;
SPaddle DefaultPaddle = { false, 'K', 'L' };

bool killStylusTopScreen = false;
bool killStylusOffScreen = false;
bool allowUpAndDown = false;
bool allowBackgroundInput = false;

extern volatile bool paused;

#define MAXKEYPAD 15

#define WM_CUSTKEYDOWN	(WM_USER+50)
#define WM_CUSTKEYUP	(WM_USER+51)

#define NUM_HOTKEY_CONTROLS 20

#define COUNT(a) (sizeof (a) / sizeof (a[0]))

const int IDC_LABEL_HK_Table[NUM_HOTKEY_CONTROLS] = {
	IDC_LABEL_HK1 , IDC_LABEL_HK2 , IDC_LABEL_HK3 , IDC_LABEL_HK4 , IDC_LABEL_HK5 ,
	IDC_LABEL_HK6 , IDC_LABEL_HK7 , IDC_LABEL_HK8 , IDC_LABEL_HK9 , IDC_LABEL_HK10,
	IDC_LABEL_HK11, IDC_LABEL_HK12, IDC_LABEL_HK13, IDC_LABEL_HK14, IDC_LABEL_HK15,
	IDC_LABEL_HK16, IDC_LABEL_HK17, IDC_LABEL_HK18, IDC_LABEL_HK19, IDC_LABEL_HK20,
};
const int IDC_HOTKEY_Table[NUM_HOTKEY_CONTROLS] = {
	IDC_HOTKEY1 , IDC_HOTKEY2 , IDC_HOTKEY3 , IDC_HOTKEY4 , IDC_HOTKEY5 ,
	IDC_HOTKEY6 , IDC_HOTKEY7 , IDC_HOTKEY8 , IDC_HOTKEY9 , IDC_HOTKEY10,
	IDC_HOTKEY11, IDC_HOTKEY12, IDC_HOTKEY13, IDC_HOTKEY14, IDC_HOTKEY15,
	IDC_HOTKEY16, IDC_HOTKEY17, IDC_HOTKEY18, IDC_HOTKEY19, IDC_HOTKEY20,
};

typedef char TcDIBuf[512];

TcDIBuf					cDIBuf;
LPDIRECTINPUT8			pDI;
DIDEVCAPS				DIJoycap;

static LPDIRECTINPUT8		tmp_pDI = NULL;
static char					tmp_device_name[255] = { 0 };
static LPDIRECTINPUTDEVICE8 tmp_Device = NULL;
static LPDIRECTINPUTDEVICE8 tmp_Joystick = NULL;

std::vector<LPDIRECTINPUTDEVICE8> joyDevices;
std::vector<std::string> joyDeviceNames;
std::vector<bool> joyDeviceFeedback;

BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	if ( FAILED( tmp_pDI->CreateDevice(lpddi->guidInstance, &tmp_Device, NULL) ) )
	{
		tmp_Device = NULL;
		return DIENUM_CONTINUE;
	}

	joyDevices.push_back(tmp_Device);
	joyDeviceNames.push_back(lpddi->tszProductName);
	if (lpddi->guidFFDriver.Data1) joyDeviceFeedback.push_back(true);
	else joyDeviceFeedback.push_back(false);
	return DIENUM_CONTINUE;
}


static void EnumDevices(LPDIRECTINPUT8 pDI)
{
	tmp_pDI = pDI;
	pDI->EnumDevices(DI8DEVCLASS_GAMECTRL,
									EnumCallback,
									NULL,
									DIEDFL_ATTACHEDONLY);
}

BOOL CALLBACK EnumObjects(const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext)
{
	if( pdidoi->dwType & DIDFT_AXIS )
	{
		DIPROPRANGE diprg; 
        diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        diprg.diph.dwHow        = DIPH_BYID; 
        diprg.diph.dwObj        = pdidoi->dwType;
        diprg.lMin              = -10000; 
        diprg.lMax              = 10000; 
   
        if( FAILED(tmp_Joystick->SetProperty(DIPROP_RANGE, &diprg.diph)) ) 
			return DIENUM_STOP;
	}
	return DIENUM_CONTINUE;
}

static void ReadControl(const char* name, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileInt("Controls",name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

static void ReadHotkey(const char* name, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileInt("Hotkeys",name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

static void ReadGuitarControl(const char* name, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileInt("Slot2.GuitarGrip",name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

static void ReadPianoControl(const char* name, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileInt("Slot2.Piano",name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

static void ReadPaddleControl(const char* name, WORD& output)
{
	UINT temp;
	temp = GetPrivateProfileInt("Slot2.Paddle",name,-1,IniName);
	if(temp != -1) {
		output = temp;
	}
}

void LoadHotkeyConfig()
{
	SCustomKey *key = &CustomKeys.key(0);

	while (!IsLastCustomKey(key)) {
		ReadHotkey(key->code,key->key); 
		std::string modname = (std::string)key->code + (std::string)" MOD";
		ReadHotkey(modname.c_str(),key->modifiers);
		key++;
	}
}

static void SaveHotkeyConfig()
{
	SCustomKey *key = &CustomKeys.key(0);

	while (!IsLastCustomKey(key)) {
		WritePrivateProfileInt("Hotkeys",(char*)key->code,key->key,IniName);
		std::string modname = (std::string)key->code + (std::string)" MOD";
		WritePrivateProfileInt("Hotkeys",(char*)modname.c_str(),key->modifiers,IniName);
		key++;
	}
}

static void LoadGuitarConfig()
{
	memcpy(&Guitar,&DefaultGuitar,sizeof(Guitar));

	//Guitar.Enabled = true;
#define DO(X) ReadGuitarControl(#X,Guitar.X);
	DO(GREEN);
	DO(RED);
	DO(YELLOW);
	DO(BLUE);
#undef DO
}

static void LoadPianoConfig()
{
	memcpy(&Piano,&DefaultPiano,sizeof(Piano));

	//Piano.Enabled = true;
#define DO(X) ReadPianoControl(#X,Piano.X);
	DO(C); DO(CS);
	DO(D); DO(DS);
	DO(E);
	DO(F); DO(FS);
	DO(G); DO(GS);
	DO(A); DO(AS);
	DO(B);
	DO(HIC);
#undef DO
}

static void LoadPaddleConfig()
{
	memcpy(&Paddle, &DefaultPaddle, sizeof(Paddle));

	ReadPaddleControl("DEC", Paddle.DEC);
	ReadPaddleControl("INC", Paddle.INC);
}


static void LoadInputConfig()
{
	memcpy(&Joypad,&DefaultJoypad,sizeof(Joypad));
	
	//read from configuration file
	Joypad[0].Enabled = true;
#define DO(X) ReadControl(#X,Joypad[0] . X);
	DO(Left); DO(Right); DO(Up); DO(Down);
	DO(Left_Up); DO(Left_Down); DO(Right_Up); DO(Right_Down);
	DO(Start); DO(Select); 
	DO(Lid); DO(Debug);
	DO(A); DO(B); DO(X); DO(Y);
	DO(L); DO(R);
#undef DO

	allowUpAndDown = GetPrivateProfileInt("Controls","AllowUpAndDown",0,IniName) != 0;
	allowBackgroundInput = GetPrivateProfileInt("Controls","AllowBackgroundInput",0,IniName) != 0;
	killStylusTopScreen = GetPrivateProfileInt("Controls","KillStylusTopScreen",0,IniName) != 0;
	killStylusOffScreen = GetPrivateProfileInt("Controls","KillStylusOffScreen",0,IniName) != 0;
}

static void WriteControl(char* name, WORD val)
{
	WritePrivateProfileInt("Controls",name,val,IniName);
}

static void SaveInputConfig()
{
#define DO(X) WriteControl(#X,Joypad[0] . X);
	DO(Left); DO(Right); DO(Up); DO(Down);
	DO(Left_Up); DO(Left_Down); DO(Right_Up); DO(Right_Down);
	DO(Start); DO(Select); 
	DO(Lid); DO(Debug);
	DO(A); DO(B); DO(X); DO(Y);
	DO(L); DO(R);
#undef DO

	WritePrivateProfileInt("Controls","AllowUpAndDown",allowUpAndDown?1:0,IniName);
	WritePrivateProfileInt("Controls","KillStylusTopScreen",killStylusTopScreen?1:0,IniName);
	WritePrivateProfileInt("Controls","KillStylusOffScreen",killStylusOffScreen?1:0,IniName);
}

BOOL di_init()
{
	HWND hParentWnd = MainWindow->getHWnd();

	pDI = NULL;
	memset(cDIBuf, 0, sizeof(cDIBuf));

	if(FAILED(DirectInput8Create(GetModuleHandle(NULL),DIRECTINPUT_VERSION,IID_IDirectInput8,(void**)&pDI,NULL)))
		return FALSE;

	memset(&JoystickF,0,sizeof(JoystickF));

	EnumDevices(pDI);

	for(int i=0;i<(int)joyDevices.size();i++) {
		JoystickF[i].Attached = true;
		JoystickF[i].Device = joyDevices[i];
		JoystickF[i].FeedBack = true;

		LPDIRECTINPUTDEVICE8 pJoystick = joyDevices[i];

		if (pJoystick)
		{
			if(!FAILED(pJoystick->SetDataFormat(&c_dfDIJoystick2)))
			{
				if(FAILED(pJoystick->SetCooperativeLevel(hParentWnd,DISCL_BACKGROUND|DISCL_EXCLUSIVE)))
				{
					pJoystick->Release();
					pJoystick = NULL;
				}
				else
				{
					tmp_Joystick = pJoystick;
					pJoystick->EnumObjects(::EnumObjects, (VOID*)hParentWnd, DIDFT_ALL);
					memset(&DIJoycap,0,sizeof(DIDEVCAPS));
					DIJoycap.dwSize=sizeof(DIDEVCAPS);
					pJoystick->GetCapabilities(&DIJoycap);
				}
			}
			else
			{
				JoystickF[i].Attached = false;
				JoystickF[i].Device = NULL;
				pJoystick->Release();
				pJoystick = NULL;
			}
		}

		if (pJoystick)
		{
			DIPROPDWORD dipdw;
			dipdw.diph.dwSize = sizeof(DIPROPDWORD);
			dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			dipdw.diph.dwObj = 0;
			dipdw.diph.dwHow = DIPH_DEVICE;
			dipdw.dwData = 0;
			if ( !FAILED( pJoystick->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph) ) )
			{
				DWORD		rgdwAxes[1] = { DIJOFS_Y };
				LONG		rglDirection[2] = { 0 };
				DICONSTANTFORCE		cf = { 0 };
				DIEFFECT	eff;

				cf.lMagnitude = (DI_FFNOMINALMAX * 100);
				
				memset(&eff, 0, sizeof(eff));
				eff.dwSize = sizeof(DIEFFECT);
				eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
				eff.dwDuration = INFINITE;
				eff.dwSamplePeriod = 0;
				eff.dwGain = DI_FFNOMINALMAX;
				eff.dwTriggerButton = DIEB_NOTRIGGER;
				eff.dwTriggerRepeatInterval = 0;
				eff.cAxes = 1;
				eff.rgdwAxes = rgdwAxes;
				eff.rglDirection = rglDirection;
				eff.lpEnvelope = 0;
				eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
				eff.lpvTypeSpecificParams = &cf;
				eff.dwStartDelay = 0;

				if( FAILED( pJoystick->CreateEffect(GUID_ConstantForce, &eff, &JoystickF[i].pEffect, NULL) ) )
					JoystickF[i].FeedBack = FALSE;
			}
			else
				JoystickF[i].FeedBack = FALSE;
			{}
		}

		INFO("DirectX Input: \n");
		if (pJoystick != NULL)
		{
			INFO("   - gamecontrol successfully inited: %s\n", joyDeviceNames[i].c_str());
			if (joyDeviceFeedback[i]) INFO("\t\t\t\t      (with FeedBack support)\n");
		}
	}

	paused = FALSE;

	return TRUE;
}

//BOOL JoystickEnabled()
//{
//	return (pJoystick==NULL?FALSE:TRUE);
//}


HWND funky;
//WPARAM tid;

//
void JoystickChanged( short ID, short Movement)
{
	// don't allow two changes to happen too close together in time
	{
		static bool first = true;
		static DWORD lastTime = 0;
		if(first || timeGetTime() - lastTime > 300) // 0.3 seconds
		{
			first = false;
			lastTime = timeGetTime();
		}
		else
		{
			return; // too soon after last change
		}
	}

	WORD JoyKey;

    JoyKey = 0x8000;
	JoyKey |= (WORD)(ID << 8);
	JoyKey |= Movement;
	SendMessage(funky,WM_USER+45,JoyKey,0);
//	KillTimer(funky,tid);
}

int FunkyNormalize(int cur, int min, int max)
{
	int Result = 0;

    if ((max - min) == 0)

        return (Result);

    Result = cur - min;
    Result = (Result * 200) / (max - min);
    Result -= 100;

    return Result;
}



#define S9X_JOY_NEUTRAL 60

void CheckAxis (short joy, short control, int val,
                                       int min, int max,
                                       bool &first, bool &second)
{



    if (FunkyNormalize (val, min, max) < -S9X_JOY_NEUTRAL)

    {
        second = false;
        if (!first)
        {
            JoystickChanged (joy, control);
            first = true;

        }
    }
    else
        first = false;

    if (FunkyNormalize (val, min, max) > S9X_JOY_NEUTRAL)
    {
        first = false;
        if (!second)
        {
            JoystickChanged (joy, (short) (control + 1));
            second = true;
        }
    }
    else
        second = false;
}


void CheckAxis_game (int val, int min, int max, bool &first, bool &second)
{
    if (FunkyNormalize (val, min, max) < -S9X_JOY_NEUTRAL)
    {
        second = false;
        first = true;
    }
    else
        first = false;

    if (FunkyNormalize (val, min, max) > S9X_JOY_NEUTRAL)
    {
        first = false;
        second = true;
    }
    else
        second = false;
}

void S9xUpdateJoyState()
{
	for(int C=0;C<16;C++)
	{
		memset(&Joystick[C],0,sizeof(Joystick[C]));
		if(!JoystickF[C].Attached) continue;
		LPDIRECTINPUTDEVICE8 pJoystick = JoystickF[C].Device;
		if (pJoystick)
		{
			DIJOYSTATE2 JoyStatus;

			HRESULT hr=pJoystick->Poll();
			if (FAILED(hr))	
				pJoystick->Acquire();
			else
			{
				hr=pJoystick->GetDeviceState(sizeof(JoyStatus),&JoyStatus);
				if (FAILED(hr)) hr=pJoystick->Acquire();
				else
				{
					CheckAxis_game(JoyStatus.lX,-10000,10000,Joystick[C].Left,Joystick[C].Right);
					CheckAxis_game(JoyStatus.lY,-10000,10000,Joystick[C].Up,Joystick[C].Down);
					CheckAxis_game(JoyStatus.lZ,-10000,10000,Joystick[C].ZNeg,Joystick[C].ZPos);
					CheckAxis_game(JoyStatus.lRx,-10000,10000,Joystick[C].XRotMin,Joystick[C].XRotMax);
					CheckAxis_game(JoyStatus.lRy,-10000,10000,Joystick[C].YRotMin,Joystick[C].YRotMax);
					CheckAxis_game(JoyStatus.lRz,-10000,10000,Joystick[C].ZRotMin,Joystick[C].ZRotMax);

					 switch (JoyStatus.rgdwPOV[0])
					{
				 case JOY_POVBACKWARD:
					Joystick[C].PovDown = true;
					break;
				case 4500:
					//Joystick[C].PovUpRight = true;
					Joystick[C].PovUp = true;
					Joystick[C].PovRight = true;
					break;
				case 13500:
					//Joystick[C].PovDnRight = true;
					Joystick[C].PovDown = true;
					Joystick[C].PovRight = true;
					break;
				case 22500:
					//Joystick[C].PovDnLeft = true;
					Joystick[C].PovDown = true;
					Joystick[C].PovLeft = true;
					break;
				case 31500:
					//Joystick[C].PovUpLeft = true;
					Joystick[C].PovUp = true;
					Joystick[C].PovLeft = true;
					break;

				case JOY_POVFORWARD:
					Joystick[C].PovUp = true;
					break;

				case JOY_POVLEFT:
					Joystick[C].PovLeft = true;
					break;

				case JOY_POVRIGHT:
					Joystick[C].PovRight = true;
					break;

				default:
					break;
					}

	   for(int B=0;B<128;B++)
			if( JoyStatus.rgbButtons[B] )
				Joystick[C].Button[B] = true;

				}
			}
		}
	}
}

void di_poll_scan()
{
	for(int C=0;C<16;C++)
	{
		//if(!JoystickF[C].Attached) continue;
		LPDIRECTINPUTDEVICE8 pJoystick = JoystickF[C].Device;
		if(!pJoystick) continue;
		if (pJoystick)
		{
			DIJOYSTATE2 JoyStatus;

			HRESULT hr=pJoystick->Poll();
			if (FAILED(hr))	
				pJoystick->Acquire();
			else
			{
				hr=pJoystick->GetDeviceState(sizeof(JoyStatus),&JoyStatus);
				if (FAILED(hr)) hr=pJoystick->Acquire();
				else
				{
					CheckAxis(C,0,JoyStatus.lX,-10000,10000,Joystick[C].Left,Joystick[C].Right);
					CheckAxis(C,2,JoyStatus.lY,-10000,10000,Joystick[C].Down,Joystick[C].Up);
					CheckAxis(C,41,JoyStatus.lZ,-10000,10000,Joystick[C].ZNeg,Joystick[C].ZPos);
					CheckAxis(C,53,JoyStatus.lRx,-10000,10000,Joystick[C].XRotMin,Joystick[C].XRotMax);
					CheckAxis(C,55,JoyStatus.lRy,-10000,10000,Joystick[C].YRotMin,Joystick[C].YRotMax);
					CheckAxis(C,57,JoyStatus.lRz,-10000,10000,Joystick[C].ZRotMin,Joystick[C].ZRotMax);
			
					 switch (JoyStatus.rgdwPOV[0])
					{
				 case JOY_POVBACKWARD:
					if( !JoystickF[C].PovDown)
					{   JoystickChanged( C, 7); }

					JoystickF[C].PovDown = true;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;
				case 4500:
					if( !JoystickF[C].PovUpRight)
					{   JoystickChanged( C, 52); }
					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = true;
					break;
				case 13500:
					if( !JoystickF[C].PovDnRight)
					{   JoystickChanged( C, 50); }
					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = true;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;
				case 22500:
					if( !JoystickF[C].PovDnLeft)
					{   JoystickChanged( C, 49); }
					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = true;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;
				case 31500:
					if( !JoystickF[C].PovUpLeft)
					{   JoystickChanged( C, 51); }
					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = true;
					JoystickF[C].PovUpRight = false;
					break;

				case JOY_POVFORWARD:
					if( !JoystickF[C].PovUp)
					{   JoystickChanged( C, 6); }

					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = true;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;

				case JOY_POVLEFT:
					if( !JoystickF[C].PovLeft)
					{   JoystickChanged( C, 4); }

					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = true;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;

				case JOY_POVRIGHT:
					if( !JoystickF[C].PovRight)
					{   JoystickChanged( C, 5); }

					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = true;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;

				default:
					JoystickF[C].PovDown = false;
					JoystickF[C].PovUp = false;
					JoystickF[C].PovLeft = false;
					JoystickF[C].PovRight = false;
					JoystickF[C].PovDnLeft = false;
					JoystickF[C].PovDnRight = false;
					JoystickF[C].PovUpLeft = false;
					JoystickF[C].PovUpRight = false;
					break;
					}

	   for(int B=0;B<128;B++)
			if( JoyStatus.rgbButtons[B] )
			{
				if( !JoystickF[C].Button[B])
				{
					JoystickChanged( C, (short)(8+B));
					JoystickF[C].Button[B] = true;
				}
			}
			else
			{   JoystickF[C].Button[B] = false; }


				}
			}
		}
	} // C loop

}


void FunkyJoyStickTimer()
{
	di_poll_scan();
}

void TranslateKey(WORD keyz,char *out)
{
//	sprintf(out,"%d",keyz);
//	return;

	char temp[128];
	if(keyz&0x8000)
	{
		sprintf(out,GAMEDEVICE_JOYNUMPREFIX,((keyz>>8)&0xF));
		switch(keyz&0xFF)
		{
		case 0:  strcat(out,GAMEDEVICE_XNEG); break;
		case 1:  strcat(out,GAMEDEVICE_XPOS); break;
        case 2:  strcat(out,GAMEDEVICE_YPOS); break;
		case 3:  strcat(out,GAMEDEVICE_YNEG); break;
		case 4:  strcat(out,GAMEDEVICE_POVLEFT); break;
		case 5:  strcat(out,GAMEDEVICE_POVRIGHT); break;
		case 6:  strcat(out,GAMEDEVICE_POVUP); break;
		case 7:  strcat(out,GAMEDEVICE_POVDOWN); break;
		case 49: strcat(out,GAMEDEVICE_POVDNLEFT); break;
		case 50: strcat(out,GAMEDEVICE_POVDNRIGHT); break;
		case 51: strcat(out,GAMEDEVICE_POVUPLEFT); break;
		case 52: strcat(out,GAMEDEVICE_POVUPRIGHT); break;
		case 41: strcat(out,GAMEDEVICE_ZNEG); break;
		case 42: strcat(out,GAMEDEVICE_ZPOS); break;
		case 43: strcat(out,GAMEDEVICE_RPOS); break;
		case 44: strcat(out,GAMEDEVICE_RNEG); break;
		case 45: strcat(out,GAMEDEVICE_UPOS); break;
		case 46: strcat(out,GAMEDEVICE_UNEG); break;
		case 47: strcat(out,GAMEDEVICE_VPOS); break;
		case 48: strcat(out,GAMEDEVICE_VNEG); break;
		
		case 53: strcat(out,GAMEDEVICE_XROTPOS); break;
		case 54: strcat(out,GAMEDEVICE_XROTNEG); break;
		case 55: strcat(out,GAMEDEVICE_YROTPOS); break;
		case 56: strcat(out,GAMEDEVICE_YROTNEG); break;
		case 57: strcat(out,GAMEDEVICE_ZROTPOS); break;
		case 58: strcat(out,GAMEDEVICE_ZROTNEG); break;

		default:
			if ((keyz & 0xff) > 40)
            {
				sprintf(temp,GAMEDEVICE_JOYBUTPREFIX,keyz&0xFF);
				strcat(out,temp);
				break;
            }

			sprintf(temp,GAMEDEVICE_BUTTON,(keyz&0xFF)-8);
			strcat(out,temp);
			break;

		}
		return;
	}
	sprintf(out,GAMEDEVICE_KEY,keyz);
	if((keyz>='0' && keyz<='9')||(keyz>='A' &&keyz<='Z'))
	{
		sprintf(out,"%c",keyz);
		return;
	}

	if( keyz >= VK_NUMPAD0 && keyz <= VK_NUMPAD9)
    {

		sprintf(out,GAMEDEVICE_NUMPADPREFIX,'0'+(keyz-VK_NUMPAD0));

        return ;
    }
	switch(keyz)
    {
        case 0:				sprintf(out,GAMEDEVICE_DISABLED); break;
        case VK_TAB:		sprintf(out,GAMEDEVICE_VK_TAB); break;
        case VK_BACK:		sprintf(out,GAMEDEVICE_VK_BACK); break;
        case VK_CLEAR:		sprintf(out,GAMEDEVICE_VK_CLEAR); break;
        case VK_RETURN:		sprintf(out,GAMEDEVICE_VK_RETURN); break;
        case VK_LSHIFT:		sprintf(out,GAMEDEVICE_VK_LSHIFT); break;
		case VK_RSHIFT:		sprintf(out,GAMEDEVICE_VK_RSHIFT); break;
        case VK_LCONTROL:	sprintf(out,GAMEDEVICE_VK_LCONTROL); break;
		case VK_RCONTROL:	sprintf(out,GAMEDEVICE_VK_RCONTROL); break;
        case VK_LMENU:		sprintf(out,GAMEDEVICE_VK_LMENU); break;
		case VK_RMENU:		sprintf(out,GAMEDEVICE_VK_RMENU); break;
        case VK_PAUSE:		sprintf(out,GAMEDEVICE_VK_PAUSE); break;
        case VK_CANCEL:		sprintf(out,GAMEDEVICE_VK_PAUSE); break; // the Pause key can resolve to either "Pause" or "Cancel" depending on when it's pressed
        case VK_CAPITAL:	sprintf(out,GAMEDEVICE_VK_CAPITAL); break;
        case VK_ESCAPE:		sprintf(out,GAMEDEVICE_VK_ESCAPE); break;
        case VK_SPACE:		sprintf(out,GAMEDEVICE_VK_SPACE); break;
        case VK_PRIOR:		sprintf(out,GAMEDEVICE_VK_PRIOR); break;
        case VK_NEXT:		sprintf(out,GAMEDEVICE_VK_NEXT); break;
        case VK_HOME:		sprintf(out,GAMEDEVICE_VK_HOME); break;
        case VK_END:		sprintf(out,GAMEDEVICE_VK_END); break;
        case VK_LEFT:		sprintf(out,GAMEDEVICE_VK_LEFT ); break;
        case VK_RIGHT:		sprintf(out,GAMEDEVICE_VK_RIGHT); break;
        case VK_UP:			sprintf(out,GAMEDEVICE_VK_UP); break;
        case VK_DOWN:		sprintf(out,GAMEDEVICE_VK_DOWN); break;
        case VK_SELECT:		sprintf(out,GAMEDEVICE_VK_SELECT); break;
        case VK_PRINT:		sprintf(out,GAMEDEVICE_VK_PRINT); break;
        case VK_EXECUTE:	sprintf(out,GAMEDEVICE_VK_EXECUTE); break;
        case VK_SNAPSHOT:	sprintf(out,GAMEDEVICE_VK_SNAPSHOT); break;
        case VK_INSERT:		sprintf(out,GAMEDEVICE_VK_INSERT); break;
        case VK_DELETE:		sprintf(out,GAMEDEVICE_VK_DELETE); break;
        case VK_HELP:		sprintf(out,GAMEDEVICE_VK_HELP); break;
        case VK_LWIN:		sprintf(out,GAMEDEVICE_VK_LWIN); break;
        case VK_RWIN:		sprintf(out,GAMEDEVICE_VK_RWIN); break;
        case VK_APPS:		sprintf(out,GAMEDEVICE_VK_APPS); break;
        case VK_MULTIPLY:	sprintf(out,GAMEDEVICE_VK_MULTIPLY); break;
        case VK_ADD:		sprintf(out,GAMEDEVICE_VK_ADD); break;
        case VK_SEPARATOR:	sprintf(out,GAMEDEVICE_VK_SEPARATOR); break;
		case /*VK_OEM_1*/0xBA:		sprintf(out,GAMEDEVICE_VK_OEM_1); break;
        case /*VK_OEM_2*/0xBF:		sprintf(out,GAMEDEVICE_VK_OEM_2); break;
        case /*VK_OEM_3*/0xC0:		sprintf(out,GAMEDEVICE_VK_OEM_3); break;
        case /*VK_OEM_4*/0xDB:		sprintf(out,GAMEDEVICE_VK_OEM_4); break;
        case /*VK_OEM_5*/0xDC:		sprintf(out,GAMEDEVICE_VK_OEM_5); break;
        case /*VK_OEM_6*/0xDD:		sprintf(out,GAMEDEVICE_VK_OEM_6); break;
		case /*VK_OEM_7*/0xDE:		sprintf(out,GAMEDEVICE_VK_OEM_7); break;
		case /*VK_OEM_COMMA*/0xBC:	sprintf(out,GAMEDEVICE_VK_OEM_COMMA );break;
		case /*VK_OEM_PERIOD*/0xBE:	sprintf(out,GAMEDEVICE_VK_OEM_PERIOD);break;
        case VK_SUBTRACT:	sprintf(out,GAMEDEVICE_VK_SUBTRACT); break;
        case VK_DECIMAL:	sprintf(out,GAMEDEVICE_VK_DECIMAL); break;
        case VK_DIVIDE:		sprintf(out,GAMEDEVICE_VK_DIVIDE); break;
        case VK_NUMLOCK:	sprintf(out,GAMEDEVICE_VK_NUMLOCK); break;
        case VK_SCROLL:		sprintf(out,GAMEDEVICE_VK_SCROLL); break;
        case /*VK_OEM_MINUS*/0xBD:	sprintf(out,GAMEDEVICE_VK_OEM_MINUS); break;
        case /*VK_OEM_PLUS*/0xBB:	sprintf(out,GAMEDEVICE_VK_OEM_PLUS); break;
        case VK_SHIFT:		sprintf(out,GAMEDEVICE_VK_SHIFT); break;
        case VK_CONTROL:	sprintf(out,GAMEDEVICE_VK_CONTROL); break;
        case VK_MENU:		sprintf(out,GAMEDEVICE_VK_MENU); break;
        case VK_F1:			sprintf(out,GAMEDEVICE_VK_F1); break;
        case VK_F2:			sprintf(out,GAMEDEVICE_VK_F2); break;
        case VK_F3:			sprintf(out,GAMEDEVICE_VK_F3); break;
        case VK_F4:			sprintf(out,GAMEDEVICE_VK_F4); break;
        case VK_F5:			sprintf(out,GAMEDEVICE_VK_F5); break;
        case VK_F6:			sprintf(out,GAMEDEVICE_VK_F6); break;
        case VK_F7:			sprintf(out,GAMEDEVICE_VK_F7); break;
        case VK_F8:			sprintf(out,GAMEDEVICE_VK_F8); break;
        case VK_F9:			sprintf(out,GAMEDEVICE_VK_F9); break;
        case VK_F10:		sprintf(out,GAMEDEVICE_VK_F10); break;
        case VK_F11:		sprintf(out,GAMEDEVICE_VK_F11); break;
        case VK_F12:		sprintf(out,GAMEDEVICE_VK_F12); break;
    }

    return ;



}

bool IsReserved (WORD Key, int modifiers)
{
	// keys that do other stuff in Windows
	if(Key == VK_CAPITAL || Key == VK_NUMLOCK || Key == VK_SCROLL || Key == VK_SNAPSHOT
	|| Key == VK_LWIN    || Key == VK_RWIN    || Key == VK_APPS || Key == /*VK_SLEEP*/0x5F
	|| (Key == VK_F4 && (modifiers & CUSTKEY_ALT_MASK) != 0)) // alt-f4 (behaves unlike accelerators)
		return true;

	// menu shortcuts (accelerators) -- TODO: should somehow parse GUI.Accelerators for this information
	if(modifiers == CUSTKEY_CTRL_MASK
	 && (Key == 'O')
	|| modifiers == CUSTKEY_ALT_MASK
	 && (Key == VK_F5 || Key == VK_F7 || Key == VK_F8 || Key == VK_F9
	  || Key == 'R' || Key == 'T' || Key == /*VK_OEM_4*/0xDB || Key == /*VK_OEM_6*/0xDD
	  || Key == 'E' || Key == 'A' || Key == VK_RETURN || Key == VK_DELETE)
	  || Key == VK_MENU || Key == VK_CONTROL)
		return true;

	return false;
}


int GetNumHotKeysAssignedTo (WORD Key, int modifiers)
{
	int count = 0;
	{
		#define MATCHES_KEY(k) \
			(Key != 0 && Key != VK_ESCAPE \
		   && ((Key == k->key && modifiers == k->modifiers) \
		   || (Key == VK_SHIFT   && k->modifiers & CUSTKEY_SHIFT_MASK) \
		   || (Key == VK_MENU    && k->modifiers & CUSTKEY_ALT_MASK) \
		   || (Key == VK_CONTROL && k->modifiers & CUSTKEY_CTRL_MASK) \
		   || (k->key == VK_SHIFT   && modifiers & CUSTKEY_SHIFT_MASK) \
		   || (k->key == VK_MENU    && modifiers & CUSTKEY_ALT_MASK) \
		   || (k->key == VK_CONTROL && modifiers & CUSTKEY_CTRL_MASK)))

		SCustomKey *key = &CustomKeys.key(0);
		while (!IsLastCustomKey(key)) {
			if (MATCHES_KEY(key)) {
				count++;
			}
			key++;
		}


		#undef MATCHES_KEY
	}
	return count;
}

int GetNumButtonsAssignedTo (WORD Key)
{
	int count = 0;
	for(int J = 0; J < 5*2; J++)
	{
		// don't want to report conflicts with disabled keys
		if(!Joypad[J%5].Enabled || Key == 0 || Key == VK_ESCAPE)
			continue;

		if(Key == Joypad[J].Left)       count++;
		if(Key == Joypad[J].Right)      count++;
		if(Key == Joypad[J].Left_Up)    count++;
		if(Key == Joypad[J].Left_Down)  count++;
		if(Key == Joypad[J].Right_Up)   count++;
		if(Key == Joypad[J].Right_Down) count++;
		if(Key == Joypad[J].Up)         count++;
		if(Key == Joypad[J].Down)       count++;
		if(Key == Joypad[J].Start)      count++;
		if(Key == Joypad[J].Select)     count++;
		if(Key == Joypad[J].A)          count++;
		if(Key == Joypad[J].B)          count++;
		if(Key == Joypad[J].X)          count++;
		if(Key == Joypad[J].Y)          count++;
		if(Key == Joypad[J].L)          count++;
		if(Key == Joypad[J].R)          count++;
		if(Key == Joypad[J].Lid)          count++;
		if(Key == Joypad[J].Debug)          count++;
    }
	return count;
}

COLORREF CheckButtonKey( WORD Key)
{
	COLORREF red,magenta,blue,white;
	red =RGB(255,0,0);
	magenta =RGB(255,0,255);
	blue = RGB(0,0,255);
	white = RGB(255,255,255);

	// Check for conflict with reserved windows keys
    if(IsReserved(Key,0))
		return red;

    // Check for conflict with Snes9X hotkeys
	if(GetNumHotKeysAssignedTo(Key,0) > 0)
        return magenta;

    // Check for duplicate button keys
    if(GetNumButtonsAssignedTo(Key) > 1)
        return blue;

    return white;
}

COLORREF CheckHotKey( WORD Key, int modifiers)
{
	COLORREF red,magenta,blue,white;
	red =RGB(255,0,0);
	magenta =RGB(255,0,255);
	blue = RGB(0,0,255);
	white = RGB(255,255,255);

	// Check for conflict with reserved windows keys
    if(IsReserved(Key,modifiers))
		return red;

    // Check for conflict with button keys
    if(modifiers == 0 && GetNumButtonsAssignedTo(Key) > 0)
        return magenta;

	// Check for duplicate Snes9X hotkeys
	if(GetNumHotKeysAssignedTo(Key,modifiers) > 1)
        return blue;

    return white;
}

static void InitCustomControls()
{

    WNDCLASSEX wc;

    wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = InputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);

    wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szHotkeysClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = HotInputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);

	wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szGuitarClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = GuitarInputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);

	wc.cbSize         = sizeof(wc);
    wc.lpszClassName  = szPaddleClassName;
    wc.hInstance      = GetModuleHandle(0);
    wc.lpfnWndProc    = PaddleInputCustomWndProc;
    wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
    wc.hIcon          = 0;
    wc.lpszMenuName   = 0;
    wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
    wc.style          = 0;
    wc.cbClsExtra     = 0;
    wc.cbWndExtra     = sizeof(InputCust *);
    wc.hIconSm        = 0;


    RegisterClassEx(&wc);
}

InputCust * GetInputCustom(HWND hwnd)
{
	return (InputCust *)GetWindowLong(hwnd, 0);
}

void SetInputCustom(HWND hwnd, InputCust *icp)
{
    SetWindowLong(hwnd, 0, (LONG)icp);
}

LRESULT InputCustom_OnPaint(InputCust *ccp, WPARAM wParam, LPARAM lParam)
{
    HDC				hdc;
    PAINTSTRUCT		ps;
    HANDLE			hOldFont;
    TCHAR			szText[200];
    RECT			rect;
	SIZE			sz;
	int				x,y;

    // Get a device context for this window
    hdc = BeginPaint(ccp->hwnd, &ps);

    // Set the font we are going to use
    hOldFont = SelectObject(hdc, ccp->hFont);

    // Set the text colours
    SetTextColor(hdc, ccp->crForeGnd);
    SetBkColor  (hdc, ccp->crBackGnd);

    // Find the text to draw
    GetWindowText(ccp->hwnd, szText, sizeof(szText));

    // Work out where to draw
    GetClientRect(ccp->hwnd, &rect);


    // Find out how big the text will be
    GetTextExtentPoint32(hdc, szText, lstrlen(szText), &sz);

    // Center the text
    x = (rect.right  - sz.cx) / 2;
    y = (rect.bottom - sz.cy) / 2;

    // Draw the text
    ExtTextOut(hdc, x, y, ETO_OPAQUE, &rect, szText, lstrlen(szText), 0);

    // Restore the old font when we have finished
    SelectObject(hdc, hOldFont);

    // Release the device context
    EndPaint(ccp->hwnd, &ps);

    return 0;
}

static LRESULT CALLBACK InputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWLP_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		selectedItem = NULL;

		SetTimer(hwnd,777,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_USER+45:
	case WM_KEYDOWN:
		TranslateKey(wParam,temp);
		col = CheckButtonKey(wParam);

		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

		break;
	case WM_USER+44:

		TranslateKey(wParam,temp);
		if(IsWindowEnabled(hwnd))
		{
			col = CheckButtonKey(wParam);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			FunkyJoyStickTimer();
		}
		SetTimer(hwnd,777,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK GuitarInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
		// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWLP_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		selectedItem = NULL;

		SetTimer(hwnd,777,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_USER+45:
	case WM_KEYDOWN:
		TranslateKey(wParam,temp);
		col = CheckButtonKey(wParam);

		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

		break;
	case WM_USER+44:

		TranslateKey(wParam,temp);
		if(IsWindowEnabled(hwnd))
		{
			col = CheckButtonKey(wParam);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			FunkyJoyStickTimer();
		}
		SetTimer(hwnd,777,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK PaddleInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
		// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWLP_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		selectedItem = NULL;

		SetTimer(hwnd,777,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_USER+45:
	case WM_KEYDOWN:
		TranslateKey(wParam,temp);
		col = CheckButtonKey(wParam);

		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

		break;
	case WM_USER+44:

		TranslateKey(wParam,temp);
		if(IsWindowEnabled(hwnd))
		{
			col = CheckButtonKey(wParam);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			FunkyJoyStickTimer();
		}
		SetTimer(hwnd,777,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void TranslateKeyWithModifiers(int wParam, int modifiers, char * outStr)
{

	// if the key itself is a modifier, special case output:
	if(wParam == VK_SHIFT)
		strcpy(outStr, "Shift");
	else if(wParam == VK_MENU)
		strcpy(outStr, "Alt");
	else if(wParam == VK_CONTROL)
		strcpy(outStr, "Control");
	else
	{
		// otherwise, prepend the modifier(s)
		if(wParam != VK_ESCAPE && wParam != 0)
		{
			if((modifiers & CUSTKEY_CTRL_MASK) != 0)
			{
				sprintf(outStr,HOTKEYS_CONTROL_MOD);
				outStr += strlen(HOTKEYS_CONTROL_MOD);
			}
			if((modifiers & CUSTKEY_ALT_MASK) != 0)
			{
				sprintf(outStr,HOTKEYS_ALT_MOD);
				outStr += strlen(HOTKEYS_ALT_MOD);
			}
			if((modifiers & CUSTKEY_SHIFT_MASK) != 0)
			{
				sprintf(outStr,HOTKEYS_SHIFT_MOD);
				outStr += strlen(HOTKEYS_SHIFT_MOD);
			}
		}

		// and append the translated non-modifier key
		TranslateKey(wParam,outStr);
	}
}

static bool keyPressLock = false;

static LRESULT CALLBACK HotInputCustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// retrieve the custom structure POINTER for THIS window
    InputCust *icp = GetInputCustom(hwnd);
	HWND pappy = (HWND__ *)GetWindowLongPtr(hwnd,GWLP_HWNDPARENT);
	funky= hwnd;

	static HWND selectedItem = NULL;

	char temp[100];
	COLORREF col;
    switch(msg)
    {

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS|DLGC_WANTALLKEYS|DLGC_WANTCHARS;
		break;


    case WM_NCCREATE:

        // Allocate a new CustCtrl structure for this window.
        icp = (InputCust *) malloc( sizeof(InputCust) );

        // Failed to allocate, stop window creation.
        if(icp == NULL) return FALSE;

        // Initialize the CustCtrl structure.
        icp->hwnd      = hwnd;
        icp->crForeGnd = GetSysColor(COLOR_WINDOWTEXT);
        icp->crBackGnd = GetSysColor(COLOR_WINDOW);
        icp->hFont     = (HFONT__ *) GetStockObject(DEFAULT_GUI_FONT);

        // Assign the window text specified in the call to CreateWindow.
        SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

        // Attach custom structure to this window.
        SetInputCustom(hwnd, icp);

		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);

		keyPressLock = false;

		selectedItem = NULL;

		SetTimer(hwnd,747,125,NULL);

        // Continue with window creation.
        return TRUE;

    // Clean up when the window is destroyed.
    case WM_NCDESTROY:
        free(icp);
        break;
	case WM_PAINT:
		return InputCustom_OnPaint(icp,wParam,lParam);
		break;
	case WM_ERASEBKGND:
		return 1;
/*
	case WM_KEYUP:
		{
			int count = 0;
			for(int i=0;i<256;i++)
				if(GetAsyncKeyState(i) & 1)
					count++;

			if(count < 2)
			{
				int p = count;
			}
			if(count < 1)
			{
				int p = count;
			}

			TranslateKey(wParam,temp);
			col = CheckButtonKey(wParam);

			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
			SetWindowText(hwnd,temp);
			InvalidateRect(icp->hwnd, NULL, FALSE);
			UpdateWindow(icp->hwnd);
			SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);
		}
		break;
*/
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:

		{
			int count = 0;
			for(int i=2;i<256;i++)
			{
				if(i >= VK_LSHIFT && i <= VK_RMENU)
					continue;
				if(GetAsyncKeyState(i) & 1)
					count++;
			}

			if(count <= 1)
			{
				keyPressLock = false;
			}
		}

		// no break

	case WM_USER+45:
		// assign a hotkey:
		{
			// don't assign pure modifiers on key-down (they're assigned on key-up)
			if(wParam == VK_SHIFT || wParam == VK_MENU || wParam == VK_CONTROL)
				break;

			int modifiers = 0;
			if(GetAsyncKeyState(VK_MENU))
				modifiers |= CUSTKEY_ALT_MASK;
			if(GetAsyncKeyState(VK_CONTROL))
				modifiers |= CUSTKEY_CTRL_MASK;
			if(GetAsyncKeyState(VK_SHIFT))
				modifiers |= CUSTKEY_SHIFT_MASK;

			TranslateKeyWithModifiers(wParam, modifiers, temp);

			col = CheckHotKey(wParam,modifiers);
///			if(col == RGB(255,0,0)) // un-redify
///				col = RGB(255,255,255);

			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
			SetWindowText(hwnd,temp);
			InvalidateRect(icp->hwnd, NULL, FALSE);
			UpdateWindow(icp->hwnd);
			SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);

			keyPressLock = true;

		}
		break;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if(!keyPressLock)
		{
			int count = 0;
			for(int i=2;i<256;i++)
			{
				if(i >= VK_LSHIFT && i <= VK_RMENU)
					continue;
				if(GetAsyncKeyState(i) & 1) // &1 seems to solve an weird non-zero return problem, don't know why
					count++;
			}
			if(count <= 1)
			{
				if(wParam == VK_SHIFT || wParam == VK_MENU || wParam == VK_CONTROL)
				{
					if(wParam == VK_SHIFT)
						sprintf(temp, "Shift");
					if(wParam == VK_MENU)
						sprintf(temp, "Alt");
					if(wParam == VK_CONTROL)
						sprintf(temp, "Control");

					col = CheckHotKey(wParam,0);

					icp->crForeGnd = ((~col) & 0x00ffffff);
					icp->crBackGnd = col;
					SetWindowText(hwnd,temp);
					InvalidateRect(icp->hwnd, NULL, FALSE);
					UpdateWindow(icp->hwnd);
					SendMessage(pappy,WM_USER+43,wParam,(LPARAM)hwnd);
				}
			}
		}
		break;
	case WM_USER+44:

		// set a hotkey field:
		{
		int modifiers = lParam;

		TranslateKeyWithModifiers(wParam, modifiers, temp);

		if(IsWindowEnabled(hwnd))
		{
			col = CheckHotKey(wParam,modifiers);
///			if(col == RGB(255,0,0)) // un-redify
///				col = RGB(255,255,255);
		}
		else
		{
			col = RGB( 192,192,192);
		}
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		SetWindowText(hwnd,temp);
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		}
		break;

	case WM_SETFOCUS:
	{
		selectedItem = hwnd;
		col = RGB( 0,255,0);
		icp->crForeGnd = ((~col) & 0x00ffffff);
		icp->crBackGnd = col;
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
//		tid = wParam;

		break;
	}
	case WM_KILLFOCUS:
	{
		selectedItem = NULL;
		SendMessage(pappy,WM_USER+46,wParam,(LPARAM)hwnd); // refresh fields on deselect
		break;
	}

	case WM_TIMER:
		if(hwnd == selectedItem)
		{
			FunkyJoyStickTimer();
		}
		SetTimer(hwnd,747,125,NULL);
		break;
	case WM_LBUTTONDOWN:
		SetFocus(hwnd);
		break;
	case WM_ENABLE:
		COLORREF col;
		if(wParam)
		{
			col = RGB( 255,255,255);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		else
		{
			col = RGB( 192,192,192);
			icp->crForeGnd = ((~col) & 0x00ffffff);
			icp->crBackGnd = col;
		}
		InvalidateRect(icp->hwnd, NULL, FALSE);
		UpdateWindow(icp->hwnd);
		return true;
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void set_buttoninfo(int index, HWND hDlg)
{
	SendDlgItemMessage(hDlg,IDC_UP,WM_USER+44,Joypad[index].Up,0);
	SendDlgItemMessage(hDlg,IDC_LEFT,WM_USER+44,Joypad[index].Left,0);
	SendDlgItemMessage(hDlg,IDC_DOWN,WM_USER+44,Joypad[index].Down,0);
	SendDlgItemMessage(hDlg,IDC_RIGHT,WM_USER+44,Joypad[index].Right,0);
	SendDlgItemMessage(hDlg,IDC_A,WM_USER+44,Joypad[index].A,0);
	SendDlgItemMessage(hDlg,IDC_B,WM_USER+44,Joypad[index].B,0);
	SendDlgItemMessage(hDlg,IDC_X,WM_USER+44,Joypad[index].X,0);
	SendDlgItemMessage(hDlg,IDC_Y,WM_USER+44,Joypad[index].Y,0);
	SendDlgItemMessage(hDlg,IDC_L,WM_USER+44,Joypad[index].L,0);
	SendDlgItemMessage(hDlg,IDC_R,WM_USER+44,Joypad[index].R,0);
	SendDlgItemMessage(hDlg,IDC_START,WM_USER+44,Joypad[index].Start,0);
	SendDlgItemMessage(hDlg,IDC_SELECT,WM_USER+44,Joypad[index].Select,0);
	SendDlgItemMessage(hDlg,IDC_LID,WM_USER+44,Joypad[index].Lid,0);
	SendDlgItemMessage(hDlg,IDC_DEBUG,WM_USER+44,Joypad[index].Debug,0);
	if(index < 5)
	{
		SendDlgItemMessage(hDlg,IDC_UPLEFT,WM_USER+44,Joypad[index].Left_Up,0);
		SendDlgItemMessage(hDlg,IDC_UPRIGHT,WM_USER+44,Joypad[index].Right_Up,0);
		SendDlgItemMessage(hDlg,IDC_DWNLEFT,WM_USER+44,Joypad[index].Left_Down,0);
		SendDlgItemMessage(hDlg,IDC_DWNRIGHT,WM_USER+44,Joypad[index].Right_Down,0);
	}
}

void EnableDisableKeyFields (int index, HWND hDlg)
{
	bool enableUnTurboable;
	if(index < 5)
	{
		// Support Unicode display
		SetDlgItemText(hDlg,IDC_LABEL_RIGHT, (LPCSTR)INPUTCONFIG_LABEL_RIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_UPLEFT, (LPCSTR)INPUTCONFIG_LABEL_UPLEFT);
		SetDlgItemText(hDlg,IDC_LABEL_UPRIGHT, (LPCSTR)INPUTCONFIG_LABEL_UPRIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNRIGHT, (LPCSTR)INPUTCONFIG_LABEL_DOWNRIGHT);
		SetDlgItemText(hDlg,IDC_LABEL_UP, (LPCSTR)INPUTCONFIG_LABEL_UP);
		SetDlgItemText(hDlg,IDC_LABEL_LEFT, (LPCSTR)INPUTCONFIG_LABEL_LEFT);
		SetDlgItemText(hDlg,IDC_LABEL_DOWN, (LPCSTR)INPUTCONFIG_LABEL_DOWN);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNLEFT, (LPCSTR)INPUTCONFIG_LABEL_DOWNLEFT);
		enableUnTurboable = true;
	}
	else
	{		
		SetDlgItemText(hDlg,IDC_LABEL_UP, (LPCSTR)INPUTCONFIG_LABEL_MAKE_TURBO);
		SetDlgItemText(hDlg,IDC_LABEL_LEFT, (LPCSTR)INPUTCONFIG_LABEL_MAKE_HELD);
		SetDlgItemText(hDlg,IDC_LABEL_DOWN, (LPCSTR)INPUTCONFIG_LABEL_MAKE_TURBO_HELD);
		SetDlgItemText(hDlg,IDC_LABEL_RIGHT, (LPCSTR)INPUTCONFIG_LABEL_CLEAR_TOGGLES_AND_TURBO);
		SetDlgItemText(hDlg,IDC_LABEL_UPLEFT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_LABEL_UPRIGHT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNLEFT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_LABEL_DOWNRIGHT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_UPLEFT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_UPRIGHT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_DWNLEFT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		SetDlgItemText(hDlg,IDC_DWNRIGHT, (LPCSTR)INPUTCONFIG_LABEL_UNUSED);
		enableUnTurboable = false;
	}

	//EnableWindow(GetDlgItem(hDlg,IDC_UPLEFT), false);
	//EnableWindow(GetDlgItem(hDlg,IDC_UPRIGHT), false);
	//EnableWindow(GetDlgItem(hDlg,IDC_DWNRIGHT), false);
	//EnableWindow(GetDlgItem(hDlg,IDC_DWNLEFT), false);
}

INT_PTR CALLBACK DlgInputConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int i, which;
	static int index=0;


	static SJoypad savepad[10];


	//HBRUSH g_hbrBackground;

switch(msg)
	{
	case WM_INITDIALOG:
		// Support Unicode display
		SetDlgItemTextW(hDlg,IDOK,BUTTON_OK);
		SetDlgItemTextW(hDlg,IDCANCEL,BUTTON_CANCEL);
		// Support Unicode display
		//SetWindowText(hDlg,INPUTCONFIG_TITLE);
		//if(DirectX.Clipped) S9xReRefresh();
		//SetDlgItemText(hDlg,IDC_JPTOGGLE,INPUTCONFIG_JPTOGGLE);
///		SetDlgItemText(hDlg,IDC_DIAGTOGGLE,INPUTCONFIG_DIAGTOGGLE);
		// Support Unicode display
		SetDlgItemTextW(hDlg,IDC_LABEL_UP,(LPWSTR)INPUTCONFIG_LABEL_UP);
		SetDlgItemTextW(hDlg,IDC_LABEL_DOWN,(LPWSTR)INPUTCONFIG_LABEL_DOWN);
		SetDlgItemTextW(hDlg,IDC_LABEL_LEFT,(LPWSTR)INPUTCONFIG_LABEL_LEFT);
		SetDlgItemTextW(hDlg,IDC_LABEL_A,(LPWSTR)INPUTCONFIG_LABEL_A);
		SetDlgItemTextW(hDlg,IDC_LABEL_B,(LPWSTR)INPUTCONFIG_LABEL_B);
		SetDlgItemTextW(hDlg,IDC_LABEL_X,(LPWSTR)INPUTCONFIG_LABEL_X);
		SetDlgItemTextW(hDlg,IDC_LABEL_Y,(LPWSTR)INPUTCONFIG_LABEL_Y);
		SetDlgItemTextW(hDlg,IDC_LABEL_L,(LPWSTR)INPUTCONFIG_LABEL_L);
		SetDlgItemTextW(hDlg,IDC_LABEL_R,(LPWSTR)INPUTCONFIG_LABEL_R);
		SetDlgItemTextW(hDlg,IDC_LABEL_START,(LPWSTR)INPUTCONFIG_LABEL_START);
		SetDlgItemTextW(hDlg,IDC_LABEL_SELECT,(LPWSTR)INPUTCONFIG_LABEL_SELECT);
		SetDlgItemTextW(hDlg,IDC_LABEL_UPRIGHT,(LPWSTR)INPUTCONFIG_LABEL_UPRIGHT);
		SetDlgItemTextW(hDlg,IDC_LABEL_UPLEFT,(LPWSTR)INPUTCONFIG_LABEL_UPLEFT);
		SetDlgItemTextW(hDlg,IDC_LABEL_DOWNRIGHT,(LPWSTR)INPUTCONFIG_LABEL_DOWNRIGHT);
		SetDlgItemTextW(hDlg,IDC_LABEL_DOWNLEFT,(LPWSTR)INPUTCONFIG_LABEL_DOWNLEFT);
		SetDlgItemTextW(hDlg,IDC_LABEL_BLUE,(LPWSTR)INPUTCONFIG_LABEL_BLUE);

		for(i=5;i<10;i++)
			Joypad[i].Left_Up = Joypad[i].Right_Up = Joypad[i].Left_Down = Joypad[i].Right_Down = 0;

		memcpy(savepad, Joypad, 10*sizeof(SJoypad));

		for( i=0;i<256;i++)
			GetAsyncKeyState(i);

		//for( C = 0; C != 16; C ++)
	 //       JoystickF[C].Attached = joyGetDevCaps( JOYSTICKID1+C, &JoystickF[C].Caps, sizeof( JOYCAPS)) == JOYERR_NOERROR;

		//memset(&JoystickF[0],0,sizeof(JoystickF[0]));
		//JoystickF[0].Attached = pJoystick != NULL;


		//for(i=1;i<6;i++)
		//{
		//	sprintf(temp,INPUTCONFIG_JPCOMBO,i);
		//	SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)temp);
		//}

		//for(i=6;i<11;i++)
		//{
		//	sprintf(temp,INPUTCONFIG_JPCOMBO INPUTCONFIG_LABEL_CONTROLLER_TURBO_PANEL_MOD,i-5);
		//	SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)temp);
		//}

		//SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_SETCURSEL,(WPARAM)0,0);

		//SendDlgItemMessage(hDlg,IDC_JPTOGGLE,BM_SETCHECK, Joypad[index].Enabled ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
		
		SendDlgItemMessage(hDlg,IDC_ALLOWLEFTRIGHT,BM_SETCHECK, allowUpAndDown ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
		SendDlgItemMessage(hDlg,IDC_KILLSTYLUSTOP,BM_SETCHECK, killStylusTopScreen ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
		SendDlgItemMessage(hDlg,IDC_KILLSTYLUSOFF,BM_SETCHECK, killStylusOffScreen ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);

		set_buttoninfo(index,hDlg);

		EnableDisableKeyFields(index,hDlg);

		//PostMessage(hDlg,WM_COMMAND, CBN_SELCHANGE<<16, 0);

		//SetFocus(GetDlgItem(hDlg,IDC_JPCOMBO));

		return TRUE;
		break;
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;
	case WM_USER+46:
		// refresh command, for clicking away from a selected field
		//index = SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_GETCURSEL,0,0);
		//if(index > 4) index += 3; // skip controllers 6, 7, and 8 in the input dialog
		set_buttoninfo(index,hDlg);
		return TRUE;
	case WM_USER+43:
		//MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
		which = GetDlgCtrlID((HWND)lParam);
		switch(which)
		{
		case IDC_UP:
			Joypad[index].Up = wParam;

			break;
		case IDC_DOWN:
			Joypad[index].Down = wParam;

			break;
		case IDC_LEFT:
			Joypad[index].Left = wParam;

			break;
		case IDC_RIGHT:
			Joypad[index].Right = wParam;

			break;
		case IDC_A:
			Joypad[index].A = wParam;

			break;
		case IDC_B:
			Joypad[index].B = wParam;

			break;
		case IDC_X:
			Joypad[index].X = wParam;

			break;
		case IDC_Y:
			Joypad[index].Y = wParam;

			break;
		case IDC_L:
			Joypad[index].L = wParam;
			break;

		case IDC_R:
			Joypad[index].R = wParam;

			break;
		case IDC_SELECT:
			Joypad[index].Select = wParam;

			break;
		case IDC_START:
			Joypad[index].Start = wParam;

			break;
		case IDC_UPLEFT:
			Joypad[index].Left_Up = wParam;

			break;
		case IDC_UPRIGHT:
			Joypad[index].Right_Up = wParam;

			break;
		case IDC_DWNLEFT:
			Joypad[index].Left_Down = wParam;

			break;
		case IDC_DWNRIGHT:
			Joypad[index].Right_Down = wParam;

			break;
		case IDC_LID:
			Joypad[index].Lid = wParam;

			break;
		case IDC_DEBUG:
			Joypad[index].Debug = wParam;

			break;

		}

		set_buttoninfo(index,hDlg);

		PostMessage(hDlg,WM_NEXTDLGCTL,0,0);
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			memcpy(Joypad, savepad, 10*sizeof(SJoypad));
			EndDialog(hDlg,0);
			break;

		case IDOK:
			allowUpAndDown = IsDlgButtonChecked(hDlg, IDC_ALLOWLEFTRIGHT) != 0;
			killStylusTopScreen = IsDlgButtonChecked(hDlg, IDC_KILLSTYLUSTOP) != 0;
			killStylusOffScreen = IsDlgButtonChecked(hDlg, IDC_KILLSTYLUSOFF) != 0;
			SaveInputConfig();
			EndDialog(hDlg,0);
			break;

		//case IDC_JPTOGGLE: // joypad Enable toggle
		//	index = SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_GETCURSEL,0,0);
		//	if(index > 4) index += 3; // skip controllers 6, 7, and 8 in the input dialog
		//	Joypad[index].Enabled=IsDlgButtonChecked(hDlg,IDC_JPTOGGLE);
		//	set_buttoninfo(index, hDlg); // update display of conflicts
		//	break;

		}
		//switch(HIWORD(wParam))
		//{
		//	case CBN_SELCHANGE:
		//		index = SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_GETCURSEL,0,0);
		//		SendDlgItemMessage(hDlg,IDC_JPCOMBO,CB_SETCURSEL,(WPARAM)index,0);
		//		if(index > 4) index += 3; // skip controllers 6, 7, and 8 in the input dialog
		//		if(index < 8)
		//		{
		//			SendDlgItemMessage(hDlg,IDC_JPTOGGLE,BM_SETCHECK, Joypad[index].Enabled ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
		//			EnableWindow(GetDlgItem(hDlg,IDC_JPTOGGLE),TRUE);
		//		}
		//		else
		//		{
		//			SendDlgItemMessage(hDlg,IDC_JPTOGGLE,BM_SETCHECK, Joypad[index-8].Enabled ? (WPARAM)BST_CHECKED : (WPARAM)BST_UNCHECKED, 0);
		//			EnableWindow(GetDlgItem(hDlg,IDC_JPTOGGLE),FALSE);
		//		}

		//		set_buttoninfo(index,hDlg);

		//		EnableDisableKeyFields(index,hDlg);

		//		break;
		//}
		//return FALSE;

	}

	return FALSE;
}


bool S9xGetState (WORD KeyIdent)
{
	if(KeyIdent == 0 || KeyIdent == 0xFF || KeyIdent == VK_ESCAPE) // if it's the 'disabled' key, it's never pressed
		return true;

    if (KeyIdent & 0x8000) // if it's a joystick 'key':
    {
        int j = (KeyIdent >> 8) & 15;

		//S9xUpdateJoyState();

        switch (KeyIdent & 0xff)
        {
            case 0: return !Joystick [j].Left;
            case 1: return !Joystick [j].Right;
            case 2: return !Joystick [j].Up;
            case 3: return !Joystick [j].Down;
            case 4: return !Joystick [j].PovLeft;
            case 5: return !Joystick [j].PovRight;
            case 6: return !Joystick [j].PovUp;
            case 7: return !Joystick [j].PovDown;
			case 49:return !Joystick [j].PovDnLeft;
			case 50:return !Joystick [j].PovDnRight;
			case 51:return !Joystick [j].PovUpLeft;
			case 52:return !Joystick [j].PovUpRight;
            case 41:return !Joystick [j].ZNeg;
            case 42:return !Joystick [j].ZPos;
            case 43:return !Joystick [j].RUp;
            case 44:return !Joystick [j].RDown;
            case 45:return !Joystick [j].UUp;
            case 46:return !Joystick [j].UDown;
            case 47:return !Joystick [j].VUp;
            case 48:return !Joystick [j].VDown;
			case 53: return !Joystick[j].XRotMin;
			case 54: return !Joystick[j].XRotMax;
			case 55: return !Joystick[j].YRotMin;
			case 56: return !Joystick[j].YRotMax;
			case 57: return !Joystick[j].ZRotMin;
			case 58: return !Joystick[j].ZRotMax;

            default:
                if ((KeyIdent & 0xff) > 40)
                    return true; // not pressed

                return !Joystick [j].Button [(KeyIdent & 0xff) - 8];
        }
    }

	// the pause key is special, need this to catch all presses of it
	// Both GetKeyState and GetAsyncKeyState cannot catch it anyway,
	// so this should be handled in WM_KEYDOWN message.
	if(KeyIdent == VK_PAUSE)
	{
		return true; // not pressed
//		if(GetAsyncKeyState(VK_PAUSE)) // not &'ing this with 0x8000 is intentional and necessary
//			return false;
	}

	SHORT gks = GetKeyState (KeyIdent);
    return ((gks & 0x80) == 0);
}

void S9xWinScanJoypads(const bool willAcceptInput)
{
	S9xUpdateJoyState();

	if (willAcceptInput)
	{
		for (int J = 0; J < 8; J++)
		{
			if (Joypad[J].Enabled)
			{
				int PadState = 0;
				PadState |= (!S9xGetState(Joypad[J].R))          ? R_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].L))          ? L_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].X))          ? X_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].A))          ? A_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Right))      ? RIGHT_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Right_Up))   ? RIGHT_MASK | UP_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Right_Down)) ? RIGHT_MASK | DOWN_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Left))       ? LEFT_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Left_Up))    ? LEFT_MASK | UP_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Left_Down))  ? LEFT_MASK | DOWN_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Down))       ? DOWN_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Up))         ? UP_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Start))      ? START_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Select))     ? SELECT_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Y))          ? Y_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].B))          ? B_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Lid))        ? LID_MASK : 0;
				PadState |= (!S9xGetState(Joypad[J].Debug))      ? DEBUG_MASK : 0;
				joypads[J] = PadState | 0x80000000;
			}
		}
	}
	else
	{
		for (int J = 0; J < 8; J++)
		{
			if (Joypad[J].Enabled)
			{
				joypads[J] = 0x80000000;
			}
		}
	}
}

//void S9xOldAutofireAndStuff ()
//{
//	// stuff ripped out of Snes9x that's no longer functional, at least for now
//    for (int J = 0; J < 8; J++)
//    {
//        if (Joypad [J].Enabled)
//        {
//			// toggle checks
//			{
//       	     	PadState  = 0;
//				PadState |= ToggleJoypadStorage[J].Left||TurboToggleJoypadStorage[J].Left			? LEFT_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Right||TurboToggleJoypadStorage[J].Right			? RIGHT_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Up||TurboToggleJoypadStorage[J].Up				? UP_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Down||TurboToggleJoypadStorage[J].Down			? DOWN_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Start||TurboToggleJoypadStorage[J].Start			? START_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Select||TurboToggleJoypadStorage[J].Select		? SELECT_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Lid||TurboToggleJoypadStorage[J].Lid				? LID_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Debug||TurboToggleJoypadStorage[J].Debug			? DEBUG_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].A||TurboToggleJoypadStorage[J].A					? A_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].B||TurboToggleJoypadStorage[J].B					? B_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].X||TurboToggleJoypadStorage[J].X					? X_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].Y||TurboToggleJoypadStorage[J].Y					? Y_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].L||TurboToggleJoypadStorage[J].L					? L_MASK : 0;
//				PadState |= ToggleJoypadStorage[J].R||TurboToggleJoypadStorage[J].R				    ? R_MASK : 0;
//			}
//			// auto-hold AND regular key/joystick presses
//			if(S9xGetState(Joypad[J+8].Left))
//			{
//				PadState ^= (!S9xGetState(Joypad[J].R)||!S9xGetState(Joypad[J+8].R))      ?  R_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].L)||!S9xGetState(Joypad[J+8].L))      ?  L_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].X)||!S9xGetState(Joypad[J+8].X))      ?  X_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].A)||!S9xGetState(Joypad[J+8].A))      ? A_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Right))  ?   RIGHT_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Right_Up))  ? RIGHT_MASK + UP_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Right_Down)) ? RIGHT_MASK + DOWN_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Left))   ?   LEFT_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Left_Up)) ?   LEFT_MASK + UP_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Left_Down)) ?  LEFT_MASK + DOWN_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Down))   ?   DOWN_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Up))     ?   UP_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Start)||!S9xGetState(Joypad[J+8].Start))  ?  START_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Select)||!S9xGetState(Joypad[J+8].Select)) ?  SELECT_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Y)||!S9xGetState(Joypad[J+8].Y))      ?  Y_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].B)||!S9xGetState(Joypad[J+8].B))      ? B_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Lid)||!S9xGetState(Joypad[J+8].Lid))      ?  LID_MASK : 0;
//				PadState ^= (!S9xGetState(Joypad[J].Debug)||!S9xGetState(Joypad[J+8].Debug))      ? DEBUG_MASK : 0;
//			}
//
//			bool turbofy = !S9xGetState(Joypad[J+8].Up); // All Mod for turbo
//
//			u32 TurboMask = 0;
//
//			//handle turbo case! (autofire / auto-fire)
//			if(turbofy || ((TurboMask&A_MASK))&&(PadState&A_MASK) || !S9xGetState(Joypad[J+8].A      )) PadState^=(joypads[J]&A_MASK);
//			if(turbofy || ((TurboMask&B_MASK))&&(PadState&B_MASK) || !S9xGetState(Joypad[J+8].B      )) PadState^=(joypads[J]&B_MASK);
//			if(turbofy || ((TurboMask&Y_MASK))&&(PadState&Y_MASK) || !S9xGetState(Joypad[J+8].Y       )) PadState^=(joypads[J]&Y_MASK);
//			if(turbofy || ((TurboMask&X_MASK))&&(PadState&X_MASK) || !S9xGetState(Joypad[J+8].X       )) PadState^=(joypads[J]&X_MASK);
//			if(turbofy || ((TurboMask&L_MASK))&&(PadState&L_MASK) || !S9xGetState(Joypad[J+8].L       )) PadState^=(joypads[J]&L_MASK);
//			if(turbofy || ((TurboMask&R_MASK))&&(PadState&R_MASK) || !S9xGetState(Joypad[J+8].R       )) PadState^=(joypads[J]&R_MASK);
//			if(turbofy || ((TurboMask&START_MASK))&&(PadState&START_MASK) || !S9xGetState(Joypad[J+8].Start )) PadState^=(joypads[J]&START_MASK);
//			if(turbofy || ((TurboMask&SELECT_MASK))&&(PadState&SELECT_MASK) || !S9xGetState(Joypad[J+8].Select)) PadState^=(joypads[J]&SELECT_MASK);
//			if(turbofy || ((TurboMask&DEBUG_MASK))&&(PadState&DEBUG_MASK) || !S9xGetState(Joypad[J+8].Debug)) PadState^=(joypads[J]&DEBUG_MASK);
//			if(           ((TurboMask&LEFT_MASK))&&(PadState&LEFT_MASK)                                    ) PadState^=(joypads[J]&LEFT_MASK);
//			if(           ((TurboMask&UP_MASK))&&(PadState&UP_MASK)                                      ) PadState^=(joypads[J]&UP_MASK);
//			if(           ((TurboMask&RIGHT_MASK))&&(PadState&RIGHT_MASK)                                   ) PadState^=(joypads[J]&RIGHT_MASK);
//			if(           ((TurboMask&DOWN_MASK))&&(PadState&DOWN_MASK)                                    ) PadState^=(joypads[J]&DOWN_MASK);
//			if(           ((TurboMask&LID_MASK))&&(PadState&LID_MASK)                                    ) PadState^=(joypads[J]&LID_MASK);
//
//			if(TurboToggleJoypadStorage[J].A     ) PadState^=(joypads[J]&A_MASK);
//			if(TurboToggleJoypadStorage[J].B     ) PadState^=(joypads[J]&B_MASK);
//			if(TurboToggleJoypadStorage[J].Y     ) PadState^=(joypads[J]&Y_MASK);
//			if(TurboToggleJoypadStorage[J].X     ) PadState^=(joypads[J]&X_MASK);
//			if(TurboToggleJoypadStorage[J].L     ) PadState^=(joypads[J]&L_MASK);
//			if(TurboToggleJoypadStorage[J].R     ) PadState^=(joypads[J]&R_MASK);
//			if(TurboToggleJoypadStorage[J].Start ) PadState^=(joypads[J]&START_MASK);
//			if(TurboToggleJoypadStorage[J].Select) PadState^=(joypads[J]&SELECT_MASK);
//			if(TurboToggleJoypadStorage[J].Left  ) PadState^=(joypads[J]&LEFT_MASK);
//			if(TurboToggleJoypadStorage[J].Up    ) PadState^=(joypads[J]&UP_MASK);
//			if(TurboToggleJoypadStorage[J].Right ) PadState^=(joypads[J]&RIGHT_MASK);
//			if(TurboToggleJoypadStorage[J].Down  ) PadState^=(joypads[J]&DOWN_MASK);
//			if(TurboToggleJoypadStorage[J].Lid  ) PadState^=(joypads[J]&LID_MASK);
//			if(TurboToggleJoypadStorage[J].Debug ) PadState^=(joypads[J]&DEBUG_MASK);
//			//end turbo case...
//
//
//			// enforce left+right/up+down disallowance here to
//			// avoid recording unused l+r/u+d that will cause desyncs
//			// when played back with l+r/u+d is allowed
//			//if(!allowUpAndDown)
//			//{
//			//	if((PadState[1] & 2) != 0)
//			//		PadState[1] &= ~(1);
//			//	if((PadState[1] & 8) != 0)
//			//		PadState[1] &= ~(4);
//			//}
//
//            joypads [J] = PadState | 0x80000000;
//        }
//        else
//            joypads [J] = 0;
//    }
//
//	// input from macro
//	//for (int J = 0; J < 8; J++)
//	//{
//	//	if(MacroIsEnabled(J))
//	//	{
//	//		uint16 userPadState = joypads[J] & 0xFFFF;
//	//		uint16 macroPadState = MacroInput(J);
//	//		uint16 newPadState;
//
//	//		switch(GUI.MacroInputMode)
//	//		{
//	//		case MACRO_INPUT_MOV:
//	//			newPadState = macroPadState;
//	//			break;
//	//		case MACRO_INPUT_OR:
//	//			newPadState = macroPadState | userPadState;
//	//			break;
//	//		case MACRO_INPUT_XOR:
//	//			newPadState = macroPadState ^ userPadState;
//	//			break;
//	//		default:
//	//			newPadState = userPadState;
//	//			break;
//	//		}
//
//	//		PadState[0] = (uint8) ( newPadState       & 0xFF);
//	//		PadState[1] = (uint8) ((newPadState >> 8) & 0xFF);
//
//	//		// enforce left+right/up+down disallowance here to
//	//		// avoid recording unused l+r/u+d that will cause desyncs
//	//		// when played back with l+r/u+d is allowed
//	//		if(!allowUpAndDown)
//	//		{
//	//			if((PadState[1] & 2) != 0)
//	//				PadState[1] &= ~(1);
//	//			if((PadState[1] & 8) != 0)
//	//				PadState[1] &= ~(4);
//	//		}
//
//	//		joypads [J] = PadState [0] | (PadState [1] << 8) | 0x80000000;
//	//	}
//	//}
//
////#ifdef NETPLAY_SUPPORT
////    if (Settings.NetPlay)
////	{
////		// Send joypad position update to server
////		S9xNPSendJoypadUpdate (joypads [GUI.NetplayUseJoypad1 ? 0 : NetPlay.Player-1]);
////
////		// set input from network
////		for (int J = 0; J < NP_MAX_CLIENTS; J++)
////			joypads[J] = S9xNPGetJoypad (J);
////	}
////#endif
//}

void input_feedback(bool enable)
{
	
	for(int C=0;C<16;C++)
	{
		if(!JoystickF[C].Attached) continue;
		if(!JoystickF[C].FeedBack) continue;
		if(!JoystickF[C].pEffect) continue;
		
		//printf("Joy%i Feedback %s\n", C, enable?"ON":"OFF");
		if (enable)
			JoystickF[C].pEffect->Start(2, 0);
		else
			JoystickF[C].pEffect->Stop();
	}
	
	//use xinput if it is available!!
	//but try lazy initializing xinput so that the dll is not required
	{
		static DWORD ( WINAPI *_XInputSetState)(DWORD,XINPUT_VIBRATION*) = NULL;
		static bool xinput_tried = false;
		if(!xinput_tried)
		{
			xinput_tried = true;
			HMODULE lib = LoadLibrary("xinput1_3.dll");
			if(lib)
			{
				_XInputSetState = (DWORD (WINAPI *)(DWORD,XINPUT_VIBRATION*))GetProcAddress(lib,"XInputSetState");
			}
		}
		if(_XInputSetState)
		{
			XINPUT_VIBRATION vib;
			vib.wLeftMotorSpeed = enable?65535:0;
			vib.wRightMotorSpeed = enable?65535:0;
			for(int i=0;i<4;i++)
				_XInputSetState(0,&vib);
		}
	}
}


void input_init()
{
	InitCustomControls();
	
	LoadInputConfig();
	LoadHotkeyConfig();
	LoadGuitarConfig();
	LoadPianoConfig();
	LoadPaddleConfig();

	di_init();
	FeedbackON = input_feedback;
}

void input_deinit()
{
	input_feedback(false);

	//todo
	// release all dx input devices
}

// TODO: maybe some of this stuff should move back to NDSSystem.cpp?
// I don't know which is the better place for it.

static void StepManualTurbo();
static void ApplyAntipodalRestriction(buttonstruct<bool>& pad);
static void SetManualTurbo(buttonstruct<bool>& pad);
static void RunAntipodalRestriction(const buttonstruct<bool>& pad);

// may run multiple times per frame.
// gets the user input and puts in a request with NDSSystem about it,
// and updates input-related state that needs to update even while paused.
void input_acquire()
{
	const bool willAcceptInput = ( allowBackgroundInput || (MainWindow->getHWnd() == GetForegroundWindow()) );
	u32 oldInput = joypads[0];

	S9xWinScanJoypads(willAcceptInput);

	buttonstruct<bool> buttons = {};
	buttons.R = (joypads[0] & RIGHT_MASK)!=0;
	buttons.L = (joypads[0] & LEFT_MASK)!=0;
	buttons.D = (joypads[0] & DOWN_MASK)!=0;
	buttons.U = (joypads[0] & UP_MASK)!=0;
	buttons.S = (joypads[0] & START_MASK)!=0;
	buttons.T = (joypads[0] & SELECT_MASK)!=0;
	buttons.B = (joypads[0] & B_MASK)!=0;
	buttons.A = (joypads[0] & A_MASK)!=0;
	buttons.Y = (joypads[0] & Y_MASK)!=0;
	buttons.X = (joypads[0] & X_MASK)!=0;
	buttons.W = (joypads[0] & L_MASK)!=0;
	buttons.E = (joypads[0] & R_MASK)!=0;
	buttons.G = (joypads[0] & DEBUG_MASK)!=0;
	buttons.F = (joypads[0] & LID_MASK)!=0;

	// take care of toggling the auto-hold flags.
	if(AutoHoldPressed)
	{
		if(buttons.R && !(oldInput & RIGHT_MASK))  AutoHold.R ^= true;
		if(buttons.L && !(oldInput & LEFT_MASK))   AutoHold.L ^= true;
		if(buttons.D && !(oldInput & DOWN_MASK))   AutoHold.D ^= true;
		if(buttons.U && !(oldInput & UP_MASK))     AutoHold.U ^= true;
		if(buttons.S && !(oldInput & START_MASK))  AutoHold.S ^= true;
		if(buttons.T && !(oldInput & SELECT_MASK)) AutoHold.T ^= true;
		if(buttons.B && !(oldInput & B_MASK))      AutoHold.B ^= true;
		if(buttons.A && !(oldInput & A_MASK))      AutoHold.A ^= true;
		if(buttons.Y && !(oldInput & Y_MASK))      AutoHold.Y ^= true;
		if(buttons.X && !(oldInput & X_MASK))      AutoHold.X ^= true;
		if(buttons.W && !(oldInput & L_MASK))      AutoHold.W ^= true;
		if(buttons.E && !(oldInput & R_MASK))      AutoHold.E ^= true;
	}

	// update upAndDown timers
	RunAntipodalRestriction(buttons);

	// apply any autofire that requires the user to be
	// actively holding a button in order to trigger it
	SetManualTurbo(buttons);

	// let's actually apply auto-hold here too,
	// even though this is supposed to be the "raw" user input,
	// since things seem to work out better this way (more useful input display, for one thing),
	// and it kind of makes sense to think of auto-held keys as
	// a direct extension of what the user is physically trying to press.
	for(int i = 0; i < ARRAY_SIZE(buttons.array); i++)
		buttons.array[i] ^= AutoHold.array[i];


	// set initial input request
	NDS_setPad(
		buttons.R, buttons.L, buttons.D, buttons.U,
		buttons.T, buttons.S, buttons.B, buttons.A,
		buttons.Y, buttons.X, buttons.W, buttons.E,
		buttons.G, buttons.F);

	if (willAcceptInput)
	{
		// TODO: this part hasn't been revised yet,
		// but guitarGrip_setKey should only request a change (like NDS_setPad does)
		if (Guitar.Enabled)
		{
			bool gG = !S9xGetState(Guitar.GREEN);
			bool gR = !S9xGetState(Guitar.RED);
			bool gY = !S9xGetState(Guitar.YELLOW);
			bool gB = !S9xGetState(Guitar.BLUE);
			guitarGrip_setKey(gG, gR, gY, gB);
		}

		//etc. same as above
		if (Piano.Enabled)
		{
			bool c   = !S9xGetState(Piano.C);
			bool cs  = !S9xGetState(Piano.CS);
			bool d   = !S9xGetState(Piano.D);
			bool ds  = !S9xGetState(Piano.DS);
			bool e   = !S9xGetState(Piano.E);
			bool f   = !S9xGetState(Piano.F);
			bool fs  = !S9xGetState(Piano.FS);
			bool g   = !S9xGetState(Piano.G);
			bool gs  = !S9xGetState(Piano.GS);
			bool a   = !S9xGetState(Piano.A);
			bool as  = !S9xGetState(Piano.AS);
			bool b   = !S9xGetState(Piano.B);
			bool hic = !S9xGetState(Piano.HIC);
			piano_setKey(c, cs, d, ds, e, f, fs, g, gs, a, as, b, hic);
		}

		if (Paddle.Enabled)
		{
			bool dec = !S9xGetState(Paddle.DEC);
			bool inc = !S9xGetState(Paddle.INC);
			if (inc) nds.paddle += 5;
			if (dec) nds.paddle -= 5;
		}
	}
	else
	{
		if (Guitar.Enabled)
		{
			guitarGrip_setKey(false, false, false, false);
		}

		if (Piano.Enabled)
		{
			piano_setKey(false, false, false, false, false, false, false, false, false, false, false, false, false);
		}
	}
}

// only runs once per frame (always after input_acquire has been called at least once).
// applies transformations to the user's input,
// and updates input-related state that needs to update once per frame.
void input_process()
{
	UserButtons& input = NDS_getProcessingUserInput().buttons;

	// prevent left+right/up+down if that option is set to not allow it
	ApplyAntipodalRestriction(input);

	// step turbo frame timers
	StepManualTurbo();

	// TODO: things like macros or "hands free" turbo/autofire
	// should probably be applied here.
}

static bool turbo[4] = {true, false, true, false};

static void StepManualTurbo()
{
	for (int i = 0; i < ARRAY_SIZE(TurboTime.array); i++)
	{
		TurboTime.array[i]++;
		if(!Turbo.array[i] || TurboTime.array[i] >= (int)ARRAY_SIZE(turbo))
			TurboTime.array[i] = 0; // reset timer if the button isn't pressed or we overran
	}
}

static void SetManualTurbo(buttonstruct<bool>& pad)
{
	for (int i = 0; i < ARRAY_SIZE(pad.array); i++)
		if(Turbo.array[i])
			pad.array[i] = turbo[TurboTime.array[i]];
}

static buttonstruct<int> cardinalHeldTime = {0};

static void RunAntipodalRestriction(const buttonstruct<bool>& pad)
{
	if(allowUpAndDown)
		return;

	pad.U ? (cardinalHeldTime.U++) : (cardinalHeldTime.U=0);
	pad.D ? (cardinalHeldTime.D++) : (cardinalHeldTime.D=0);
	pad.L ? (cardinalHeldTime.L++) : (cardinalHeldTime.L=0);
	pad.R ? (cardinalHeldTime.R++) : (cardinalHeldTime.R=0);
}
static void ApplyAntipodalRestriction(buttonstruct<bool>& pad)
{
	if(allowUpAndDown)
		return;

	// give preference to whichever direction was most recently pressed
	if(pad.U && pad.D)
		if(cardinalHeldTime.U < cardinalHeldTime.D)
			pad.D = false;
		else
			pad.U = false;
	if(pad.L && pad.R)
		if(cardinalHeldTime.L < cardinalHeldTime.R)
			pad.R = false;
		else
			pad.L = false;
}



static void set_hotkeyinfo(HWND hDlg)
{
	HotkeyPage page = (HotkeyPage) SendDlgItemMessage(hDlg,IDC_HKCOMBO,CB_GETCURSEL,0,0);
	SCustomKey *key = &CustomKeys.key(0);
	int i = 0;

	while (!IsLastCustomKey(key) && i < NUM_HOTKEY_CONTROLS) {
		if (page == key->page) {
			SendDlgItemMessage(hDlg, IDC_HOTKEY_Table[i], WM_USER+44, key->key, key->modifiers);
			SetDlgItemTextW(hDlg, IDC_LABEL_HK_Table[i], key->name.c_str());
			ShowWindow(GetDlgItem(hDlg, IDC_HOTKEY_Table[i]), SW_SHOW);
			i++;
		}
		key++;
	}
	// disable unused controls
	for (; i < NUM_HOTKEY_CONTROLS; i++) {
		SendDlgItemMessage(hDlg, IDC_HOTKEY_Table[i], WM_USER+44, 0, 0);
		SetDlgItemText(hDlg, IDC_LABEL_HK_Table[i], INPUTCONFIG_LABEL_UNUSED);
		ShowWindow(GetDlgItem(hDlg, IDC_HOTKEY_Table[i]), SW_HIDE);
	}
}


// DlgHotkeyConfig
INT_PTR CALLBACK DlgHotkeyConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int i, which;
	static HotkeyPage page = (HotkeyPage) 0;


	static SCustomKeys keys;

	//HBRUSH g_hbrBackground;
switch(msg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint (hDlg, &ps);

			EndPaint (hDlg, &ps);
		}
		return TRUE;
	case WM_INITDIALOG:
		//if(DirectX.Clipped) S9xReRefresh();
		// Support Unicode display
		wchar_t menuItemString[256];
		LoadStringW(hAppInst, ID_HOTKEYS_TITLE, menuItemString, 256);
		SetWindowTextW(hDlg, menuItemString);

		// insert hotkey page list items
		for(i = 0 ; i < NUM_HOTKEY_PAGE ; i++)
		{
			SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)hotkeyPageTitle[i]);
		}

		SendDlgItemMessage(hDlg,IDC_HKCOMBO,CB_SETCURSEL,(WPARAM)0,0);

		InitCustomKeys(&keys);
		CopyCustomKeys(&keys, &CustomKeys);
		for( i=0;i<256;i++)
		{
			GetAsyncKeyState(i);
		}

		// Support Unicode display
		SetDlgItemTextW(hDlg,IDC_LABEL_BLUE,(LPWSTR)HOTKEYS_LABEL_BLUE);

		set_hotkeyinfo(hDlg);

		PostMessage(hDlg,WM_COMMAND, CBN_SELCHANGE<<16, 0);

		SetFocus(GetDlgItem(hDlg,IDC_HKCOMBO));


		return TRUE;

	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return TRUE;
	case WM_USER+46:
		// refresh command, for clicking away from a selected field
		page = (HotkeyPage) SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_GETCURSEL, 0, 0);
		set_hotkeyinfo(hDlg);
		return TRUE;
	case WM_USER+43:
	{
		//MessageBox(hDlg,"USER+43 CAUGHT","moo",MB_OK);
		int modifiers = GetModifiers(wParam);

		page = (HotkeyPage) SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_GETCURSEL, 0, 0);
		wchar_t text[256];

		which = GetDlgCtrlID((HWND)lParam);
		for (i = 0; i < NUM_HOTKEY_CONTROLS; i++) {
			if (which == IDC_HOTKEY_Table[i])
				break;
		}
		GetDlgItemTextW(hDlg, IDC_LABEL_HK_Table[i], text, COUNT(text));

		SCustomKey *key = &CustomKeys.key(0);
		while (!IsLastCustomKey(key)) {
			if (page == key->page) {
				if(text == key->name) {
					key->key = wParam;
					key->modifiers = modifiers;
					break;
				}
			}
			key++;
		}

		set_hotkeyinfo(hDlg);
		PostMessage(hDlg,WM_NEXTDLGCTL,0,0);
//		PostMessage(hDlg,WM_KILLFOCUS,0,0);
	}
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			CopyCustomKeys(&CustomKeys, &keys);
			EndDialog(hDlg,0);
			break;
		case IDOK:
			SaveHotkeyConfig();
			EndDialog(hDlg,0);
			break;
		}
		switch(HIWORD(wParam))
		{
			case CBN_SELCHANGE:
				page = (HotkeyPage) SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_GETCURSEL, 0, 0);
				SendDlgItemMessage(hDlg, IDC_HKCOMBO, CB_SETCURSEL, (WPARAM)page, 0);

				set_hotkeyinfo(hDlg);

				SetFocus(GetDlgItem(hDlg, IDC_HKCOMBO));

				break;
		}
		return FALSE;

	}

	return FALSE;
}


void RunInputConfig()
{
	DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_INPUTCONFIG), MainWindow->getHWnd(), DlgInputConfig);
}

void RunHotkeyConfig()
{
	DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_KEYCUSTOM), MainWindow->getHWnd(), DlgHotkeyConfig);
}


/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/
