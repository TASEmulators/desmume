/*  
	Copyright (C) 2006-2007 Normmatt

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

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <COMMDLG.H>
#include <string.h>
#include <dinput.h>

#include "CWindow.h"
#include "ConfigKeys.h"

#include "../debug.h"
#include "resource.h"

static char IniName[MAX_PATH];
char                    vPath[MAX_PATH],*szPath,currDir[MAX_PATH];

const char *tabkeytext[52] = {"0","1","2","3","4","5","6","7","8","9","A","B","C",
"D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X",
"Y","Z","SPACE","UP","DOWN","LEFT","RIGHT","TAB","SHIFT","DEL","INSERT","HOME","END","ENTER",
"Joystick UP","Joystick DOWN","Joystick LEFT","Joystick RIGHT"};

DWORD dds_up=37;
DWORD dds_down=38;
DWORD dds_left=39;
DWORD dds_right=40;

DWORD dds_a=31;
DWORD dds_b=11;
DWORD dds_x=16;
DWORD dds_y=17;

DWORD dds_l=12;
DWORD dds_r=23;

DWORD dds_select=36;
DWORD dds_start=47;

DWORD dds_debug=13;

extern DWORD ds_up;
extern DWORD ds_down;
extern DWORD ds_left;
extern DWORD ds_right;
extern DWORD ds_a;
extern DWORD ds_b;
extern DWORD ds_x;
extern DWORD ds_y;
extern DWORD ds_l;
extern DWORD ds_r;
extern DWORD ds_select;
extern DWORD ds_start;

#define KEY_UP           ds_up
#define KEY_DOWN         ds_down
#define KEY_LEFT         ds_left
#define KEY_RIGHT        ds_right
#define KEY_X            ds_x
#define KEY_Y            ds_y
#define KEY_A            ds_a
#define KEY_B            ds_b
#define KEY_L            ds_l
#define KEY_R            ds_r
#define KEY_START        ds_start
#define KEY_SELECT       ds_select
#define KEY_DEBUG        ds_debug

/* DirectInput stuff */
char					g_cDIBuf[512];
LPDIRECTINPUT8			g_pDI;
LPDIRECTINPUTDEVICE8	g_pKeyboard;
LPDIRECTINPUTDEVICE8	g_pJoystick;
DIDEVCAPS				g_DIJoycap;

void GetINIPath(char *inipath,u16 bufferSize)
{   
    if (*vPath)
       szPath = vPath;
    else
    {
       char *p;
       ZeroMemory(vPath, sizeof(vPath));
       GetModuleFileName(NULL, vPath, sizeof(vPath));
       p = vPath + lstrlen(vPath);
       while (p >= vPath && *p != '\\') p--;
       if (++p >= vPath) *p = 0;
       szPath = vPath;
    }
	if (strlen(szPath) + strlen("\\desmume.ini") < bufferSize)
	{
		sprintf(inipath, "%s\\desmume.ini",szPath);
	} else if (bufferSize> strlen(".\\desmume.ini")) {
		sprintf(inipath, ".\\desmume.ini",szPath);
	} else
	{
		memset(inipath,0,bufferSize) ;
	}
}

void  ReadConfig(void)
{
	FILE *fp;
    int i;

    GetINIPath(IniName,MAX_PATH);

    i=GetPrivateProfileInt("Keys","Key_A",31, IniName);
    KEY_A = i;
    
    i=GetPrivateProfileInt("Keys","Key_B",11, IniName);
    KEY_B = i;
    
    i=GetPrivateProfileInt("Keys","Key_SELECT",36, IniName);
    KEY_SELECT = i;
    
    i=GetPrivateProfileInt("Keys","Key_START",13, IniName);
    if(i==13)
    KEY_START = 47;
    else
    KEY_START = i;
        
    i=GetPrivateProfileInt("Keys","Key_RIGHT",40, IniName);
    KEY_RIGHT = i;
    
    i=GetPrivateProfileInt("Keys","Key_LEFT",39, IniName);
    KEY_LEFT = i;
    
    i=GetPrivateProfileInt("Keys","Key_UP",37, IniName);
    KEY_UP = i;
    
    i=GetPrivateProfileInt("Keys","Key_DOWN",38, IniName);
    KEY_DOWN = i;  
    
    i=GetPrivateProfileInt("Keys","Key_R",23, IniName);
    KEY_R = i;
    
    i=GetPrivateProfileInt("Keys","Key_L",12, IniName);
    KEY_L = i;
    
    i=GetPrivateProfileInt("Keys","Key_X",16, IniName);
    KEY_X = i;
    
    i=GetPrivateProfileInt("Keys","Key_Y",17, IniName);
    KEY_Y = i;

    /*i=GetPrivateProfileInt("Keys","Key_DEBUG",13, IniName);
    KEY_DEBUG = i;*/
}

void WritePrivateProfileInt(char* appname, char* keyname, int val, char* file)
{
	char temp[256] = "";
	sprintf(temp, "%d", val);
	WritePrivateProfileString(appname, keyname, temp, file);
}

void  WriteConfig(void)
{
	FILE *fp;
    int i;
    
    GetINIPath(IniName,MAX_PATH);

    WritePrivateProfileInt("Keys","Key_A",KEY_A,IniName);
    WritePrivateProfileInt("Keys","Key_B",KEY_B,IniName);
    WritePrivateProfileInt("Keys","Key_SELECT",KEY_SELECT,IniName);
    
    if(KEY_START==47)
    WritePrivateProfileInt("Keys","Key_START",13,IniName);
    else
    WritePrivateProfileInt("Keys","Key_START",KEY_START,IniName);
    
    WritePrivateProfileInt("Keys","Key_RIGHT",KEY_RIGHT,IniName);
    WritePrivateProfileInt("Keys","Key_LEFT",KEY_LEFT,IniName);
    WritePrivateProfileInt("Keys","Key_UP",KEY_UP,IniName);
    WritePrivateProfileInt("Keys","Key_DOWN",KEY_DOWN,IniName);
    WritePrivateProfileInt("Keys","Key_R",KEY_R,IniName);
    WritePrivateProfileInt("Keys","Key_L",KEY_L,IniName);
    WritePrivateProfileInt("Keys","Key_X",KEY_X,IniName);
    WritePrivateProfileInt("Keys","Key_Y",KEY_Y,IniName);
    /*WritePrivateProfileInt("Keys","Key_DEBUG",KEY_DEBUG,IniName);*/
}

void dsDefaultKeys(void)
{
	KEY_A=dds_a;
	KEY_B=dds_b;
	KEY_SELECT=dds_select;
	KEY_START=dds_start;
	KEY_RIGHT=dds_right;
	KEY_LEFT=dds_left;
	KEY_UP=dds_up;
	KEY_DOWN=dds_down;
	KEY_R=dds_r;
	KEY_L=dds_l;
	KEY_X=dds_x;
	KEY_Y=dds_y;
	//KEY_DEBUG=dds_debug;
}

DWORD key_combos[12]={
IDC_COMBO1,
IDC_COMBO2,
IDC_COMBO3,
IDC_COMBO4,
IDC_COMBO5,
IDC_COMBO6,
IDC_COMBO7,
IDC_COMBO8,
IDC_COMBO9,
IDC_COMBO10,
IDC_COMBO11,
IDC_COMBO12};

BOOL CALLBACK ConfigView_Proc(HWND dialog,UINT komunikat,WPARAM wparam,LPARAM lparam)
{
	int i,j;
	char tempstring[256];
	switch(komunikat)
	{
		case WM_INITDIALOG:
                ReadConfig();
				if (g_pKeyboard)
					for(i=0;i<48;i++)
						for(j=0;j<12;j++)
							SendDlgItemMessage(dialog,key_combos[j],CB_ADDSTRING,0,(LPARAM)tabkeytext[i]);
				if (g_pJoystick)
				{
					for(i=0;i<4;i++)
						for(j=0;j<12;j++)
							SendDlgItemMessage(dialog,key_combos[j],CB_ADDSTRING,0,(LPARAM)tabkeytext[i+48]);
					for(i=0;i<g_DIJoycap.dwButtons;i++)
						for(j=0;j<12;j++)
						{
							char buf[30];
							sprintf(buf,"Joystick B%i",i+1);
							SendDlgItemMessage(dialog,key_combos[j],CB_ADDSTRING,0,(LPARAM)buf);
						}
				}
				SendDlgItemMessage(dialog,IDC_COMBO1,CB_SETCURSEL,KEY_UP,0);
				SendDlgItemMessage(dialog,IDC_COMBO2,CB_SETCURSEL,KEY_LEFT,0);
				SendDlgItemMessage(dialog,IDC_COMBO3,CB_SETCURSEL,KEY_RIGHT,0);
				SendDlgItemMessage(dialog,IDC_COMBO4,CB_SETCURSEL,KEY_DOWN,0);
				SendDlgItemMessage(dialog,IDC_COMBO5,CB_SETCURSEL,KEY_Y,0);
				SendDlgItemMessage(dialog,IDC_COMBO6,CB_SETCURSEL,KEY_X,0);
				SendDlgItemMessage(dialog,IDC_COMBO7,CB_SETCURSEL,KEY_A,0);
				SendDlgItemMessage(dialog,IDC_COMBO8,CB_SETCURSEL,KEY_B,0);
				SendDlgItemMessage(dialog,IDC_COMBO9,CB_SETCURSEL,KEY_START,0);
				SendDlgItemMessage(dialog,IDC_COMBO10,CB_SETCURSEL,KEY_L,0);
				SendDlgItemMessage(dialog,IDC_COMBO11,CB_SETCURSEL,KEY_R,0);
				SendDlgItemMessage(dialog,IDC_COMBO12,CB_SETCURSEL,KEY_SELECT,0);
				//SendDlgItemMessage(dialog,IDC_COMBO13,CB_SETCURSEL,KEY_DEBUG,0);
				break;
	
		case WM_CLOSE:
		case WM_QUIT:
			EndDialog(dialog,0);
			return 1;
			break;
		case WM_COMMAND:
				if((HIWORD(wparam)==BN_CLICKED)&&(((int)LOWORD(wparam))==IDC_BUTTON1))
				{
				dsDefaultKeys();
				SendDlgItemMessage(dialog,IDC_COMBO1,CB_SETCURSEL,KEY_UP,0);
				SendDlgItemMessage(dialog,IDC_COMBO2,CB_SETCURSEL,KEY_LEFT,0);
				SendDlgItemMessage(dialog,IDC_COMBO3,CB_SETCURSEL,KEY_RIGHT,0);
				SendDlgItemMessage(dialog,IDC_COMBO4,CB_SETCURSEL,KEY_DOWN,0);
				SendDlgItemMessage(dialog,IDC_COMBO5,CB_SETCURSEL,KEY_Y,0);
				SendDlgItemMessage(dialog,IDC_COMBO6,CB_SETCURSEL,KEY_X,0);
				SendDlgItemMessage(dialog,IDC_COMBO7,CB_SETCURSEL,KEY_A,0);
				SendDlgItemMessage(dialog,IDC_COMBO8,CB_SETCURSEL,KEY_B,0);
				SendDlgItemMessage(dialog,IDC_COMBO9,CB_SETCURSEL,KEY_START,0);
				SendDlgItemMessage(dialog,IDC_COMBO10,CB_SETCURSEL,KEY_L,0);
				SendDlgItemMessage(dialog,IDC_COMBO11,CB_SETCURSEL,KEY_R,0);
				SendDlgItemMessage(dialog,IDC_COMBO12,CB_SETCURSEL,KEY_SELECT,0);
				//SendDlgItemMessage(dialog,IDC_COMBO13,CB_SETCURSEL,KEY_DEBUG,0);
				}
				else
				if((HIWORD(wparam)==BN_CLICKED)&&(((int)LOWORD(wparam))==IDC_FERMER))
				{
				KEY_UP=SendDlgItemMessage(dialog,IDC_COMBO1,CB_GETCURSEL,0,0);
				KEY_LEFT=SendDlgItemMessage(dialog,IDC_COMBO2,CB_GETCURSEL,0,0);
				KEY_RIGHT=SendDlgItemMessage(dialog,IDC_COMBO3,CB_GETCURSEL,0,0);
				KEY_DOWN=SendDlgItemMessage(dialog,IDC_COMBO4,CB_GETCURSEL,0,0);
				KEY_Y=SendDlgItemMessage(dialog,IDC_COMBO5,CB_GETCURSEL,0,0);
				KEY_X=SendDlgItemMessage(dialog,IDC_COMBO6,CB_GETCURSEL,0,0);
				KEY_A=SendDlgItemMessage(dialog,IDC_COMBO7,CB_GETCURSEL,0,0);
				KEY_B=SendDlgItemMessage(dialog,IDC_COMBO8,CB_GETCURSEL,0,0);
				KEY_START=SendDlgItemMessage(dialog,IDC_COMBO9,CB_GETCURSEL,0,0);
				KEY_L=SendDlgItemMessage(dialog,IDC_COMBO10,CB_GETCURSEL,0,0);
				KEY_R=SendDlgItemMessage(dialog,IDC_COMBO11,CB_GETCURSEL,0,0);
				KEY_SELECT=SendDlgItemMessage(dialog,IDC_COMBO12,CB_GETCURSEL,0,0);
				//KEY_DEBUG=SendDlgItemMessage(dialog,IDC_COMBO13,CB_GETCURSEL,0,0);
				WriteConfig();
				EndDialog(dialog,0);
				return 1;
				}
		        break;
	}
	return 0;
}


//================================================================================================
//================================================================================================
//================================================================================================
//================================================================================================
//================================================================================================
BOOL CALLBACK JoyObjects(const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext)
{
	if( pdidoi->dwType & DIDFT_AXIS )
    {
        DIPROPRANGE diprg; 
        diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        diprg.diph.dwHow        = DIPH_BYID; 
        diprg.diph.dwObj        = pdidoi->dwType;
        diprg.lMin              = -3; 
        diprg.lMax              = 3; 
   
        if( FAILED(IDirectInputDevice8_SetProperty(g_pJoystick,DIPROP_RANGE,&diprg.diph))) 
            return DIENUM_STOP;
    }
	return DIENUM_CONTINUE;
}
HRESULT Input_Init(HWND hwnd)
{
	HRESULT hr;
	int	res;

	ReadConfig();
	g_pKeyboard=NULL; g_pJoystick=NULL;

	if(!FAILED(DirectInput8Create(GetModuleHandle(NULL),DIRECTINPUT_VERSION,IID_IDirectInput8,(void**)&g_pDI,NULL)))
	{
		//******************* Keyboard
		res=0;
		if (FAILED(IDirectInput8_CreateDevice(g_pDI,GUID_SysKeyboard,&g_pKeyboard,NULL))) res=-1;
		else
			if (FAILED(IDirectInputDevice8_SetDataFormat(g_pKeyboard,&c_dfDIKeyboard))) res=-1;
			else
				if (FAILED(IDirectInputDevice8_SetCooperativeLevel(g_pKeyboard,hwnd,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE))) res=-1;
		if (res!=0)
		{
			if(g_pKeyboard) IDirectInputDevice8_Release(g_pKeyboard);
			g_pKeyboard=NULL;
		}

		//******************** Joystick
		res=0;
		if(FAILED(IDirectInput8_CreateDevice(g_pDI,GUID_Joystick,&g_pJoystick,NULL))) res=-1;
		else
			if(FAILED(IDirectInputDevice8_SetDataFormat(g_pJoystick,&c_dfDIJoystick2))) res=-1;
			else
				if(FAILED(IDirectInputDevice8_SetCooperativeLevel(g_pJoystick,hwnd,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE))) res=-1;
		if (res!=0)
		{
			if(g_pJoystick) IDirectInputDevice8_Release(g_pJoystick);
			g_pJoystick=NULL;
		}
		else
		{
			IDirectInputDevice8_EnumObjects(g_pJoystick,JoyObjects,(VOID*)hwnd,DIDFT_ALL);
			memset(&g_DIJoycap,0,sizeof(DIDEVCAPS));
			g_DIJoycap.dwSize=sizeof(DIDEVCAPS);
			IDirectInputDevice8_GetCapabilities(g_pJoystick,&g_DIJoycap);
		}
	}
	else return E_FAIL;

	if ((g_pKeyboard==NULL)&&(g_pJoystick==NULL))
	{
		Input_DeInit();
		return E_FAIL;
	}
	return S_OK;
}

HRESULT Input_DeInit()
{
	if (g_pDI)
	{
		HRESULT hr;
		if (g_pKeyboard) 
		{
			hr=IDirectInputDevice8_Unacquire(g_pKeyboard);
#ifdef DEBUG
			if (FAILED(hr)) LOG("DINPUT: error keyboard unacquire (0x%08X)\n",hr);
#endif
			hr=IDirectInputDevice8_Release(g_pKeyboard);
#ifdef DEBUG
			if (FAILED(hr)) LOG("DINPUT: error keyboard release (0x%08X)\n",hr);
#endif
			g_pKeyboard=NULL;
		}
		if (g_pJoystick) 
		{
			hr=IDirectInputDevice8_Unacquire(g_pJoystick);
#ifdef DEBUG
			if (FAILED(hr)) LOG("DINPUT: error joystick unacquire (0x%08X)\n",hr);
#endif
			hr=IDirectInputDevice8_Release(g_pJoystick);
#ifdef DEBUG
			if (FAILED(hr)) LOG("DINPUT: error joystick release (0x%08X)\n",hr);
#endif
			g_pJoystick=NULL;
		}
		IDirectInput8_Release(g_pDI);
		g_pDI=NULL;
	}
	return S_OK;
}

void Input_Process()
{
	HRESULT hr;
	DIJOYSTATE2 JoyStatus;

	long source,psource;

	memset(g_cDIBuf,0,sizeof(g_cDIBuf));
	if(g_pKeyboard) 
	{
		hr=IDirectInputDevice8_GetDeviceState(g_pKeyboard,256,g_cDIBuf);
		if (FAILED(hr)) IDirectInputDevice8_Acquire(g_pKeyboard);
	}

	if(g_pJoystick)
	{
		hr=IDirectInputDevice8_Poll(g_pJoystick);
		if (FAILED(hr))	IDirectInputDevice8_Acquire(g_pJoystick);
		else
		{
			hr=IDirectInputDevice8_GetDeviceState(g_pJoystick,sizeof(JoyStatus),&JoyStatus);
			if (FAILED(hr)) hr=IDirectInputDevice8_Acquire(g_pJoystick);
			else
			{
				if (JoyStatus.lX<-1) g_cDIBuf[258]=-128;
				if (JoyStatus.lX>1) g_cDIBuf[259]=-128;
				if (JoyStatus.lY<-1) g_cDIBuf[256]=-128;
				if (JoyStatus.lY>1) g_cDIBuf[257]=-128;

				if (JoyStatus.rgdwPOV[0]==0) g_cDIBuf[256]=-128;
				if (JoyStatus.rgdwPOV[0]==4500) { g_cDIBuf[256]=-128; g_cDIBuf[259]=-128;}
				if (JoyStatus.rgdwPOV[0]==9000) g_cDIBuf[259]=-128;
				if (JoyStatus.rgdwPOV[0]==13500) { g_cDIBuf[259]=-128; g_cDIBuf[257]=-128;}
				if (JoyStatus.rgdwPOV[0]==18000) g_cDIBuf[257]=-128;
				if (JoyStatus.rgdwPOV[0]==22500) { g_cDIBuf[257]=-128; g_cDIBuf[258]=-128;}
				if (JoyStatus.rgdwPOV[0]==27000) g_cDIBuf[258]=-128;
				if (JoyStatus.rgdwPOV[0]==31500) { g_cDIBuf[258]=-128; g_cDIBuf[256]=-128;}
				memcpy(g_cDIBuf+260,JoyStatus.rgbButtons,sizeof(JoyStatus.rgbButtons));			
			}
		}
	}
}