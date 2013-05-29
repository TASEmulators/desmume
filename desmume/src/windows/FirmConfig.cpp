/*
	Copyright (C) 2007 Normmatt
	Copyright (C) 2007-2009 DeSmuME team

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

#include <stdio.h>
#include <stdlib.h>
#include "../common.h"
#include <mmsystem.h>
#include <COMMDLG.H>
#include <string.h>

#include "CWindow.h"

#include "resource.h"
#include "FirmConfig.h"

#include "../debug.h"
#include "../NDSSystem.h"
#include "../firmware.h"

static char nickname_buffer[11];
static char message_buffer[27];

const char firmLang[6][16]   = {"Japanese","English","French","German","Italian","Spanish"};
const char firmColor[16][16] = {"Gray","Brown","Red","Pink","Orange","Yellow","Lime Green",
                                "Green","Dark Green","Sea Green","Turquoise","Blue",
                                "Dark Blue","Dark Purple","Violet","Magenta"};
const char firmDay[31][16]   = {"1","2","3","4","5","6","7","8","9","10","11","12","13","14",
                                "15","16","17","18","19","20","21","22","23","24","25","26",
                                "27","28","29","30","31"};
const char firmMonth[12][16] = {"January","Feburary","March","April","May","June","July",
                                "August","September","October","November","December"};

static void WriteFirmConfig( struct NDS_fw_config_data *fw_config)
{
	char temp_str[27];
	int i;

    WritePrivateProfileInt("Firmware","favColor", fw_config->fav_colour,IniName);
    WritePrivateProfileInt("Firmware","bMonth", fw_config->birth_month,IniName);
    WritePrivateProfileInt("Firmware","bDay",fw_config->birth_day,IniName);
    WritePrivateProfileInt("Firmware","Language",fw_config->language,IniName);

	/* FIXME: harshly only use the lower byte of the UTF-16 character.
	 * This would cause strange behaviour if the user could set UTF-16 but
	 * they cannot yet.
	 */
	for ( i = 0; i < fw_config->nickname_len; i++) {
		temp_str[i] = fw_config->nickname[i];
	}
	temp_str[i] = '\0';
    WritePrivateProfileString("Firmware", "nickName", temp_str, IniName);

	for ( i = 0; i < fw_config->message_len; i++) {
		temp_str[i] = fw_config->message[i];
	}
	temp_str[i] = '\0';
	WritePrivateProfileString("Firmware","Message", temp_str, IniName);
}

BOOL CALLBACK FirmConfig_Proc(HWND dialog,UINT komunikat,WPARAM wparam,LPARAM lparam)
{
	struct NDS_fw_config_data *fw_config = &CommonSettings.fw_config;
	int i;
	char temp_str[27];

	switch(komunikat)
	{
		case WM_INITDIALOG:
				for(i=0;i<6;i++) SendDlgItemMessage(dialog,IDC_COMBO4,CB_ADDSTRING,0,(LPARAM)&firmLang[i]);
				for(i=0;i<12;i++) SendDlgItemMessage(dialog,IDC_COMBO2,CB_ADDSTRING,0,(LPARAM)&firmMonth[i]);
				for(i=0;i<16;i++) SendDlgItemMessage(dialog,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)&firmColor[i]);
				for(i=0;i<31;i++) SendDlgItemMessage(dialog,IDC_COMBO3,CB_ADDSTRING,0,(LPARAM)&firmDay[i]);
				SendDlgItemMessage(dialog,IDC_COMBO1,CB_SETCURSEL,fw_config->fav_colour,0);
				SendDlgItemMessage(dialog,IDC_COMBO2,CB_SETCURSEL,fw_config->birth_month-1,0);
				SendDlgItemMessage(dialog,IDC_COMBO3,CB_SETCURSEL,fw_config->birth_day-1,0);
				SendDlgItemMessage(dialog,IDC_COMBO4,CB_SETCURSEL,fw_config->language,0);
				SendDlgItemMessage(dialog,IDC_EDIT1,EM_SETLIMITTEXT,10,0);
				SendDlgItemMessage(dialog,IDC_EDIT2,EM_SETLIMITTEXT,26,0);
				SendDlgItemMessage(dialog,IDC_EDIT1,EM_SETSEL,0,10);
				SendDlgItemMessage(dialog,IDC_EDIT2,EM_SETSEL,0,26);

				for ( i = 0; i < fw_config->nickname_len; i++) {
					nickname_buffer[i] = fw_config->nickname[i];
				}
				nickname_buffer[i] = '\0';
				SendDlgItemMessage(dialog,IDC_EDIT1,WM_SETTEXT,0,(LPARAM)nickname_buffer);

				for ( i = 0; i < fw_config->message_len; i++) {
					message_buffer[i] = fw_config->message[i];
				}
				message_buffer[i] = '\0';
				SendDlgItemMessage(dialog,IDC_EDIT2,WM_SETTEXT,0,(LPARAM)message_buffer);
				break;
	
		case WM_COMMAND:
				if((HIWORD(wparam)==BN_CLICKED)&&(((int)LOWORD(wparam))==IDOK))
				{
					int char_index;
					LRESULT res;
				fw_config->fav_colour = SendDlgItemMessage(dialog,IDC_COMBO1,CB_GETCURSEL,0,0);
				fw_config->birth_month = 1 + SendDlgItemMessage(dialog,IDC_COMBO2,CB_GETCURSEL,0,0);
				fw_config->birth_day = 1 + SendDlgItemMessage(dialog,IDC_COMBO3,CB_GETCURSEL,0,0);
				fw_config->language = SendDlgItemMessage(dialog,IDC_COMBO4,CB_GETCURSEL,0,0);

				*(WORD *)temp_str = 10;
				res = SendDlgItemMessage(dialog,IDC_EDIT1,EM_GETLINE,0,(LPARAM)temp_str);

				if ( res > 0) {
					temp_str[res] = '\0';
					fw_config->nickname_len = strlen( temp_str);
				}
				else {
					strcpy( temp_str, "yopyop");
					fw_config->nickname_len = strlen( temp_str);
				}
				for ( char_index = 0; char_index < fw_config->nickname_len; char_index++) {
					fw_config->nickname[char_index] = temp_str[char_index];
				}

				*(WORD *)temp_str = 26;
				res = SendDlgItemMessage(dialog,IDC_EDIT2,EM_GETLINE,0,(LPARAM)temp_str);
				if ( res > 0) {
					temp_str[res] = '\0';
					fw_config->message_len = strlen( temp_str);
				}
				else {
					fw_config->message_len = 0;
				}
				for ( char_index = 0; char_index < fw_config->message_len; char_index++) {
					fw_config->message[char_index] = temp_str[char_index];
				}

				WriteFirmConfig( fw_config);
				EndDialog(dialog,0);
				if (CommonSettings.UseExtFirmware == 0)
					NDS_CreateDummyFirmware( fw_config);
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


