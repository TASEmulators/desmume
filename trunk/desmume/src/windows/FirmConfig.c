/*  
	Copyright (C) 2007 Normmatt

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

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <COMMDLG.H>
#include <string.h>

#include "CWindow.h"

#include "resource.h"
#include "FirmConfig.h"
#include "ConfigKeys.h"

#include "../debug.h"
#include "../NDSSystem.h"

char IniName[MAX_PATH];
u32 keytab[12];

const char firmLang[6][16]   = {"Japanese","English","French","German","Italian","Spanish"};
const char firmColor[16][16] = {"Gray","Brown","Red","Pink","Orange","Yellow","Lime Green",
                                "Green","Dark Green","Sea Green","Turquoise","Blue",
                                "Dark Blue","Dark Purple","Violet","Magenta"};
const char firmDay[31][16]   = {"1","2","3","4","5","6","7","8","9","10","11","12","13","14",
                                "15","16","17","18","19","20","21","22","23","24","25","26",
                                "27","28","29","30","31"};
const char firmMonth[12][16] = {"January","Feburary","March","April","May","June","July",
                                "August","September","October","November","December"};

void  ReadFirmConfig(void)
{
    GetINIPath(IniName,MAX_PATH);

    memset(&firmware,0,sizeof(firmware));
    firmware.favColor = GetPrivateProfileInt("Firmware","favColor", 10, IniName);
    firmware.bMonth = GetPrivateProfileInt("Firmware","bMonth", 7, IniName);
    firmware.bDay = GetPrivateProfileInt("Firmware","bDay", 15, IniName);
    GetPrivateProfileString("Firmware","nickName", "yopyop", firmware.nickName, 10, IniName);
    firmware.nickLen = strlen(firmware.nickName);
    GetPrivateProfileString("Firmware","Message", "Hi,it/s me!", firmware.message, 26, IniName);
    firmware.msgLen = strlen(firmware.message);
    firmware.language = GetPrivateProfileInt("Firmware","Language", 2, IniName);
}

void  WriteFirmConfig(void)
{   
    GetINIPath(IniName,MAX_PATH);

    WritePrivateProfileInt("Firmware","favColor",firmware.favColor,IniName);
    WritePrivateProfileInt("Firmware","bMonth",firmware.bMonth,IniName);
    WritePrivateProfileInt("Firmware","bDay",firmware.bDay,IniName);
    WritePrivateProfileInt("Firmware","Language",firmware.language,IniName);
    WritePrivateProfileString("Firmware","nickName", firmware.nickName, IniName);
    WritePrivateProfileString("Firmware","Message", firmware.message, IniName);
}

BOOL CALLBACK FirmConfig_Proc(HWND dialog,UINT komunikat,WPARAM wparam,LPARAM lparam)
{
	int i,j;
	char tempstring[256];
	switch(komunikat)
	{
		case WM_INITDIALOG:
                ReadConfig();
				for(i=0;i<6;i++) SendDlgItemMessage(dialog,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM)&firmLang[i]);
				for(i=0;i<12;i++) SendDlgItemMessage(dialog,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)&firmMonth[i]);
				for(i=0;i<16;i++) SendDlgItemMessage(dialog,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)&firmColor[i]);
				for(i=0;i<31;i++) SendDlgItemMessage(dialog,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)&firmDay[i]);
				SendDlgItemMessage(dialog,IDC_COMBO1,CB_SETCURSEL,firmware.favColor,0);
				SendDlgItemMessage(dialog,IDC_COMBO2,CB_SETCURSEL,firmware.bMonth-1,0);
				SendDlgItemMessage(dialog,IDC_COMBO3,CB_SETCURSEL,firmware.bDay-1,0);
				SendDlgItemMessage(dialog,IDC_COMBO4,CB_SETCURSEL,firmware.language,0);
				SendDlgItemMessage(dialog,IDC_EDIT1,EM_SETLIMITTEXT,10,0);
				SendDlgItemMessage(dialog,IDC_EDIT2,EM_SETLIMITTEXT,26,0);
				SendDlgItemMessage(dialog,IDC_EDIT1,EM_SETSEL,0,10);
				SendDlgItemMessage(dialog,IDC_EDIT2,EM_SETSEL,0,26);
				SendDlgItemMessage(dialog,IDC_EDIT1,EM_REPLACESEL,0,(LPARAM)&firmware.nickName);
				SendDlgItemMessage(dialog,IDC_EDIT2,EM_REPLACESEL,0,(LPARAM)&firmware.message);
				break;
	
		case WM_COMMAND:
				if((HIWORD(wparam)==BN_CLICKED)&&(((int)LOWORD(wparam))==IDOK))
				{
				firmware.favColor=SendDlgItemMessage(dialog,IDC_COMBO1,CB_GETCURSEL,0,0);
				firmware.bMonth=1+SendDlgItemMessage(dialog,IDC_COMBO2,CB_GETCURSEL,0,0);
				firmware.bDay=1+SendDlgItemMessage(dialog,IDC_COMBO3,CB_GETCURSEL,0,0);
				firmware.language=SendDlgItemMessage(dialog,IDC_COMBO4,CB_GETCURSEL,0,0);
				SendDlgItemMessage(dialog,IDC_EDIT1,EM_GETLINE,0,(LPARAM)&firmware.nickName);
				SendDlgItemMessage(dialog,IDC_EDIT2,EM_GETLINE,0,(LPARAM)&firmware.message);
				WriteFirmConfig();
				EndDialog(dialog,0);
				return 1;
				}
				else
				if((HIWORD(wparam)==BN_CLICKED)&&(((int)LOWORD(wparam))==IDCANCEL))
				{
                EndDialog(dialog, 0);
                return 0;
                }
		        break;
	}
	return 0;
}


