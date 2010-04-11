/* 	NDS Firmware tools

	Copyright 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef __TEMPLATES_H_
#include "common.h"
#include "resource_ids.h"
typedef struct
{
	char	wclass[20];
	char	wtitle[50];
	u32		style;
	u32		x;
	u32		y;
	u32		w;
	u32		h;
	u32		id;
} TEMPLATE;

#define Y_HEADER	65
#define Y_ARM9		250
#define Y_ARM7		375
#define Y_OTHERS	520
#define X_SECOND_FW	350
#define Y_BUTTONS	500

const TEMPLATE TmainWnd[] = {
	{"STATIC",	"Firmware path:",				0,					5, 5, 100, 20,								NULL},
	{"EDIT",	"",								ES_READONLY,		5, 25, 595, 20,								IDC_BPATH},
	{"BUTTON",	"Browse...",					WS_TABSTOP | BS_DEFPUSHBUTTON, 610, 25, 80, 20,					IDC_BBROWSE},

	{"BUTTON",	"Header data",					BS_GROUPBOX, 4, Y_HEADER, 340, 180,								NULL},
	{"STATIC",	"Console type",					0,					15,  Y_HEADER + 30, 300, 20,				NULL},
	{"STATIC",	"Version",						0,					15,  Y_HEADER + 60, 300, 20,				NULL},
	{"STATIC",	"Built timestamp (d/m/y h:m)",	0,					15,  Y_HEADER + 90, 300, 20,				NULL},
	{"STATIC",	"Size of firmware",				0,					15,  Y_HEADER + 120, 300, 20,				NULL},
	{"STATIC",	"CRC",							0,					15,  Y_HEADER + 150, 300, 20,				NULL},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_HEADER + 30, 100, 20,			IDC_H1_TYPE},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_HEADER + 60, 100, 20,			IDC_H1_VERSION},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_HEADER + 90, 100, 20,			IDC_H1_TIMESTAMP},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_HEADER + 120, 100, 20,			IDC_H1_SIZE},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_HEADER + 150, 100, 20,			IDC_H1_CRC},

	{"BUTTON",	"ARM9",							BS_GROUPBOX, 4, Y_ARM9, 340, 120,								NULL},
	{"STATIC",	"ARM9 boot code address",		0,					15, Y_ARM9+30, 300, 20,						NULL},
	{"STATIC",	"ARM9 boot code RAM address",	0,					15, Y_ARM9+60, 300, 20,						NULL},
	{"STATIC",	"ARM9 GUI code address",		0,					15, Y_ARM9+90, 300, 20,						NULL},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_ARM9 + 30, 100, 20,				IDC_H1_ARM9_ROM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_ARM9 + 60, 100, 20,				IDC_H1_ARM9_RAM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_ARM9 + 90, 100, 20,				IDC_H1_ARM9_GUI},
	
	{"BUTTON",	"ARM7",							BS_GROUPBOX, 4, Y_ARM7, 340, 120,								NULL},
	{"STATIC",	"ARM7 boot code address",		0,					15, Y_ARM7+30, 300, 20,						NULL},
	{"STATIC",	"ARM7 boot code RAM address",	0,					15, Y_ARM7+60, 300, 20,						NULL},
	{"STATIC",	"ARM7 WiFi code address",		0,					15, Y_ARM7+90, 300, 20,						NULL},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_ARM7 + 30, 100, 20,				IDC_H1_ARM7_ROM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_ARM7 + 60, 100, 20,				IDC_H1_ARM7_RAM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, 230,  Y_ARM7 + 90, 100, 20,				IDC_H1_ARM7_WIFI},

	{"BUTTON",	"FlashME: Header data",			BS_GROUPBOX, X_SECOND_FW, Y_HEADER, 340, 120,					NULL},
	{"STATIC",	"FlashME version",				0,					X_SECOND_FW+11,  Y_HEADER + 30, 300, 20,	NULL},
	{"STATIC",	"Size of firmware",				0,					X_SECOND_FW+11,  Y_HEADER + 60, 300, 20,	NULL},
	{"STATIC",	"CRC",							0,					X_SECOND_FW+11,  Y_HEADER + 90, 300, 20,	NULL},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_HEADER + 30, 100, 20, IDC_H2_VERSION},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_HEADER + 60, 100, 20, IDC_H2_SIZE},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_HEADER + 90, 100, 20, IDC_H2_CRC},

	{"BUTTON",	"FlashME: ARM9",				BS_GROUPBOX, X_SECOND_FW, Y_ARM9, 340, 120,						NULL},
	{"STATIC",	"ARM9 boot code address",		0,					X_SECOND_FW+11, Y_ARM9+30, 300, 20,			NULL},
	{"STATIC",	"ARM9 boot code RAM address",	0,					X_SECOND_FW+11, Y_ARM9+60, 300, 20,			NULL},
	{"STATIC",	"ARM9 GUI code address",		0,					X_SECOND_FW+11, Y_ARM9+90, 300, 20,			NULL},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_ARM9 + 30, 100, 20,	IDC_H2_ARM9_ROM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_ARM9 + 60, 100, 20,	IDC_H2_ARM9_RAM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_ARM9 + 90, 100, 20,	IDC_H2_ARM9_GUI},
	
	{"BUTTON",	"FlashME: ARM7",				BS_GROUPBOX, X_SECOND_FW, Y_ARM7, 340, 120,						NULL},
	{"STATIC",	"ARM7 boot code address",		0,					X_SECOND_FW+11, Y_ARM7+30, 300, 20,			NULL},
	{"STATIC",	"ARM7 boot code RAM address",	0,					X_SECOND_FW+11, Y_ARM7+60, 300, 20,			NULL},
	{"STATIC",	"ARM7 WiFi code address",		0,					X_SECOND_FW+11, Y_ARM7+90, 300, 20,			NULL},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_ARM7 + 30, 100, 20,	IDC_H2_ARM7_ROM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_ARM7 + 60, 100, 20,	IDC_H2_ARM7_RAM},
	{"EDIT",	"",								ES_READONLY | ES_RIGHT, X_SECOND_FW+230,  Y_ARM7 + 90, 100, 20,	IDC_H2_ARM7_WIFI},

	{"BUTTON",	"User",							WS_TABSTOP | WS_DISABLED, 5, Y_BUTTONS, 100, 20,				IDC_BUSER},
	{"BUTTON",	"WiFi",							WS_TABSTOP | WS_DISABLED, 120, Y_BUTTONS, 100, 20,				IDC_BWIFI},
	{"BUTTON",	"WiFi AP",						WS_TABSTOP | WS_DISABLED, 235, Y_BUTTONS, 100, 20,				IDC_BWIFIAP},
	{"BUTTON",	"Dump",							WS_TABSTOP | WS_DISABLED, 350, Y_BUTTONS, 100, 20,				NULL},
	{"BUTTON",	"Extract...",					WS_TABSTOP | WS_DISABLED, 465, Y_BUTTONS, 100, 20,				NULL},
	{"BUTTON",	"Exit",							WS_TABSTOP,			620, Y_BUTTONS, 70, 20,						IDC_BEXIT}
};

#define Y_BIRTH_LANG 105
#define Y_FAV_COLOR	165
#define Y_TCALIB 290
#define Y_USR_MISC 385
#define Y_USR_BUTTONS 570

const TEMPLATE TuserDlg1[] = {
	{"STATIC",		"Nickname:",	0, 5, 5, 100, 17,															NULL},
	{"EDIT",		"",				WS_TABSTOP, 5, 25, 160, 17,													IDC_NICKNAME},
	{"STATIC",		"Message:",		0, 5, 55, 100, 17,															NULL},
	{"EDIT",		"",				WS_TABSTOP, 5, 75, 400, 17,													IDC_MESSAGE},

	{"BUTTON",		"Birthday",		BS_GROUPBOX, 4, Y_BIRTH_LANG, 160, 55,										NULL},
	{"COMBOBOX",	"Day",			WS_TABSTOP | WS_VSCROLL | BS_GROUPBOX, 15, Y_BIRTH_LANG+20, 40, 120,		IDC_BIRTH_DAY},
	{"COMBOBOX",	"Month",		WS_TABSTOP | BS_GROUPBOX, 60, Y_BIRTH_LANG+20, 95, 120,						IDC_BIRTH_MONTH},

	{"BUTTON",		"Language",		BS_GROUPBOX, 170, Y_BIRTH_LANG, 110, 55,									NULL},
	{"COMBOBOX",	"Language",		WS_TABSTOP | BS_GROUPBOX, 180, Y_BIRTH_LANG+20, 90, 120,					IDC_LANGUAGE},

	{"BUTTON",		"Alarm",		BS_GROUPBOX, 285, Y_BIRTH_LANG, 120, 55,									NULL},
	{"BUTTON",		"",				WS_TABSTOP | BS_AUTOCHECKBOX, 295, Y_BIRTH_LANG+25, 13, 13,					IDC_ALARM_ON},
	{"COMBOBOX",	"hour",			WS_TABSTOP | WS_VSCROLL | BS_GROUPBOX, 315, Y_BIRTH_LANG+20, 40, 120,		IDC_ALARM_HOUR},
	{"COMBOBOX",	"min",			WS_TABSTOP | WS_VSCROLL | BS_GROUPBOX, 355, Y_BIRTH_LANG+20, 40, 120,		IDC_ALARM_MIN},

	{"BUTTON",		"Favorite color", BS_GROUPBOX, 4, Y_FAV_COLOR, 400, 115,									NULL}
};


const TEMPLATE TuserDlg2[] = {
	{"BUTTON",	"Touch-screen calibraion (HEX)",BS_GROUPBOX, 4, Y_TCALIB, 400, 95,								NULL},
	
	{"BUTTON",	"ADC",						BS_GROUPBOX, 14, Y_TCALIB+15, 190, 75,								NULL},
	{"STATIC",	"X1",						0, 25, Y_TCALIB+35, 100, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT , 45, Y_TCALIB+35, 50, 17,					IDC_TADC_X1},
	{"STATIC",	"Y1",						0, 25, Y_TCALIB+60, 40, 17,											NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 45, Y_TCALIB+60, 50, 17,						IDC_TADC_Y1},
	{"STATIC",	"X2",						0, 120, Y_TCALIB+35, 40, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 140, Y_TCALIB+35, 50, 17,					IDC_TADC_X2},
	{"STATIC",	"Y2",						0, 120, Y_TCALIB+60, 40, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 140, Y_TCALIB+60, 50, 17,					IDC_TADC_Y2},

	{"BUTTON",	"Screen",					BS_GROUPBOX, 205, Y_TCALIB+15, 190, 75,								NULL},
	{"STATIC",	"X1",						0, 215, Y_TCALIB+35, 40, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 235, Y_TCALIB+35, 50, 17,					IDC_TSCR_X1},
	{"STATIC",	"Y1",						0, 215, Y_TCALIB+60, 40, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 235, Y_TCALIB+60, 50, 17,					IDC_TSCR_Y1},
	{"STATIC",	"X2",						0, 310, Y_TCALIB+35, 40, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 330, Y_TCALIB+35, 50, 17,					IDC_TSCR_X2},
	{"STATIC",	"Y2",						0, 310, Y_TCALIB+60, 40, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 330, Y_TCALIB+60, 50, 17,					IDC_TSCR_Y2},

	{"BUTTON",	"Misc",						BS_GROUPBOX, 4, Y_USR_MISC, 400, 180,								NULL},
	{"STATIC",	"GBA mode screen selection",0, 15, Y_USR_MISC+20, 180, 17,										NULL},
	{"COMBOBOX","GBA",						WS_TABSTOP | BS_GROUPBOX, 215, Y_USR_MISC+20, 180, 120,				IDC_GBA_MODE_SCREENS},
	{"STATIC",	"Backlight level",			0, 15, Y_USR_MISC+50, 130, 17,										NULL},
	{"COMBOBOX","Backlight",				WS_TABSTOP | BS_GROUPBOX, 215, Y_USR_MISC+50, 180, 120,				IDC_BRIGHTLIGHT_LEVEL},
	{"STATIC",	"Boot menu",				0, 15, Y_USR_MISC+80, 100, 17,										NULL},
	{"COMBOBOX","boot",						WS_TABSTOP | BS_GROUPBOX, 215, Y_USR_MISC+80, 180, 120,				IDC_BOOTMENU},
	{"STATIC",	"RTC offset",				0, 15, Y_USR_MISC+120, 100, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 100, Y_USR_MISC+120, 90, 17,					IDC_RTC_OFFSET},
	{"STATIC",	"First boot (year)",		0, 220, Y_USR_MISC+120, 120, 17,									NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 345, Y_USR_MISC+120, 50, 17,					IDC_YEAR_BOOT},
	{"STATIC",	"Update counter",			ES_READONLY, 15, Y_USR_MISC+150, 120, 17,							NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 140, Y_USR_MISC+150, 50, 17,					IDC_UPDATE_COUNTER},
	{"STATIC",	"CRC16 (HEX)",				ES_READONLY, 220, Y_USR_MISC+150, 90, 17,							NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 345, Y_USR_MISC+150, 50, 17,					IDC_USER_CRC16},
	{"BUTTON",	"Backup",					WS_TABSTOP | WS_DISABLED, 15, Y_USR_BUTTONS, 70, 17,				NULL},
	{"BUTTON",	"Restore",					WS_TABSTOP | WS_DISABLED, 90, Y_USR_BUTTONS, 70, 17,				NULL},
	{"BUTTON",	"Update",					WS_TABSTOP | WS_DISABLED, 245, Y_USR_BUTTONS, 70, 17,				NULL},
	{"BUTTON",	"Cancel",					WS_TABSTOP | BS_DEFPUSHBUTTON, 320, Y_USR_BUTTONS, 70, 17,			IDCANCEL}
};

#define Y_WIFI_GENERAL	5
#define X_WIFI_CHANNELS	490
#define Y_WIFI_CHANNELS	5
#define Y_WIFI_REGS		55
#define Y_WIFI_BB		155
#define Y_WIFI_RF		275
#define Y_WIFI_BUTTONS	575
const TEMPLATE TwifiDlg1[] = {

	{"BUTTON",	"General",					BS_GROUPBOX, 4, Y_WIFI_GENERAL, 485, 50,							NULL},

	{"STATIC",	"CRC16",					0, 15, Y_WIFI_GENERAL+20, 45, 17,									NULL},
	{"EDIT",	"",							ES_RIGHT | ES_READONLY, 65, Y_WIFI_GENERAL+20, 55, 17,				IDC_WIFI_CRC16},
	{"STATIC",	"Version",					0, 125, Y_WIFI_GENERAL+20, 55, 17,									NULL},
	{"EDIT",	"",							ES_RIGHT | ES_READONLY, 180, Y_WIFI_GENERAL+20, 25, 17,				IDC_WIFI_VERSION},
	{"STATIC",	"MAC ID",					0, 215, Y_WIFI_GENERAL+20, 55, 17,									NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 265, Y_WIFI_GENERAL+20, 110, 17,				IDC_WIFI_MACID},
	{"STATIC",	"RF type",					0, 385, Y_WIFI_GENERAL+20, 55, 17,									NULL},
	{"EDIT",	"",							ES_READONLY| ES_RIGHT, 440, Y_WIFI_GENERAL+20, 35, 17,				IDC_WIFI_RFTYPE},

	{"BUTTON",	"Enabled channels",			BS_GROUPBOX, X_WIFI_CHANNELS + 4, Y_WIFI_CHANNELS, 230, 50,			NULL},

	{"BUTTON",	"ch01",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 15, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_01},
	{"BUTTON",	"ch02",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 30, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_02},
	{"BUTTON",	"ch03",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 45, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_03},
	{"BUTTON",	"ch04",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 60, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_04},
	{"BUTTON",	"ch05",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 75, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_05},
	{"BUTTON",	"ch06",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 90, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_06},
	{"BUTTON",	"ch07",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 105, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_07},
	{"BUTTON",	"ch08",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 120, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_08},
	{"BUTTON",	"ch09",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 135, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_09},
	{"BUTTON",	"ch10",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 150, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_10},
	{"BUTTON",	"ch11",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 165, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_11},
	{"BUTTON",	"ch12",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 180, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_12},
	{"BUTTON",	"ch13",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 195, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_13},
	{"BUTTON",	"ch14",						WS_TABSTOP | BS_AUTOCHECKBOX, X_WIFI_CHANNELS + 210, Y_WIFI_CHANNELS+25, 13, 13,		IDC_WIFI_ECHANNEL_14},


	{"BUTTON",	"Initial values for... (HEX)",	BS_GROUPBOX, 4, Y_WIFI_REGS, 720, 100,							NULL},

	{"STATIC",	"0x4808146",				0, 15, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 15, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_044},
	{"STATIC",	"0x4808148",				0, 105, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 105, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_046},
	{"STATIC",	"0x480814A",				0, 195, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 195, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_048},
	{"STATIC",	"0x480814C",				0, 285, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 285, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_04A},
	{"STATIC",	"0x4808120",				0, 375, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 375, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_04C},
	{"STATIC",	"0x4808122",				0, 465, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 465, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_04E},
	{"STATIC",	"0x4808154",				0, 555, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 555, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_050},
	{"STATIC",	"0x4808144",				0, 645, Y_WIFI_REGS+15, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 645, Y_WIFI_REGS+30, 70, 17,					IDC_WIFI_052},
	{"STATIC",	"0x4808130",				0,  15, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 15, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_054},
	{"STATIC",	"0x4808132",				0, 105, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 105, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_056},
	{"STATIC",	"0x4808140",				0, 195, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 195, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_058},
	{"STATIC",	"0x4808142",				0, 285, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 285, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_05A},
	{"STATIC",	"0x4808038",				0, 375, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 375, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_05C},
	{"STATIC",	"0x4808124",				0, 465, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 465, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_05E},
	{"STATIC",	"0x4808128",				0, 555, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 555, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_060},
	{"STATIC",	"0x4808150",				0, 645, Y_WIFI_REGS+55, 75, 17,										NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_RIGHT, 645, Y_WIFI_REGS+70, 70, 17,					IDC_WIFI_062},

	{"BUTTON",	"Initial 8bit values for BB (HEX)",	BS_GROUPBOX, 4, Y_WIFI_BB, 720, 120,						NULL},
	{"STATIC",	" 0:",						0, 15, Y_WIFI_BB+15, 75, 17,										NULL},
	{"STATIC",	"27:",						0, 15, Y_WIFI_BB+40, 75, 17,										NULL},
	{"STATIC",	"53:",						0, 15, Y_WIFI_BB+65, 75, 17,										NULL},
	{"STATIC",	"79:",						0, 15, Y_WIFI_BB+90, 75, 17,										NULL}
};

const TEMPLATE TwifiDlg2[] = {
	{"BUTTON",	"Backup",					WS_TABSTOP | WS_DISABLED, 15, Y_WIFI_BUTTONS, 70, 17,				NULL},
	{"BUTTON",	"Restore",					WS_TABSTOP | WS_DISABLED, 90, Y_WIFI_BUTTONS, 70, 17,				NULL},
	{"BUTTON",	"Update",					WS_TABSTOP | WS_DISABLED, 570, Y_WIFI_BUTTONS, 70, 17,				NULL},
	{"BUTTON",	"Cancel",					WS_TABSTOP | BS_DEFPUSHBUTTON, 645, Y_WIFI_BUTTONS, 70, 17,			IDCANCEL}
};

#define Y_WIFI_AP_BUTTONS	670
const TEMPLATE TwifiAPDlg1[] = {
	{"BUTTON",	"Connection Data",			BS_GROUPBOX, 4, 15, 720, 215,										NULL},
	{"STATIC",	"SSID",						0, 15, 35, 75, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_LEFT, 50, 35, 150, 17,								IDC_WIFI_AP_SSID},
	{"STATIC",	"SSID (WEP)",				0, 220, 35, 125, 17,												NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_LEFT, 300, 35, 150, 17,								IDC_WIFI_AP_SSID2},

	{"STATIC",	"User ID",					0, 470, 35, 95, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_CENTER, 525, 35, 185, 17,							IDC_WIFI_AP_USER_ID},

	{"BUTTON",	"WEP",						BS_GROUPBOX, 14, 55, 700, 80,										NULL},

	{"STATIC",	"Key1",						0, 25, 75, 75, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_LEFT, 65, 75, 150, 17,								IDC_WIFI_AP_KEY1},
	{"STATIC",	"Key2",						0, 225, 75, 75, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_LEFT, 265, 75, 150, 17,								IDC_WIFI_AP_KEY2},
	{"STATIC",	"Key3",						0, 25, 105, 75, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_LEFT, 65, 100, 150, 17,								IDC_WIFI_AP_KEY3},
	{"STATIC",	"Key4",						0, 225, 105, 75, 17,												NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_LEFT, 265, 100, 150, 17,							IDC_WIFI_AP_KEY4},

	{"STATIC",	"WEP mode",					0, 445, 75, 75, 17,													NULL},
	{"COMBOBOX","WEPsize",					WS_TABSTOP | WS_VSCROLL | BS_GROUPBOX, 535, 75, 80, 120,			IDC_WIFI_AP_WEP_SIZE},
	{"COMBOBOX","WEPmethod",				WS_TABSTOP | WS_VSCROLL | BS_GROUPBOX, 620, 75, 80, 120,			IDC_WIFI_AP_WEP_EMETHOD},

	{"BUTTON",	"IP",						BS_GROUPBOX, 14, 140, 345, 80,										NULL},

	{"STATIC",	"IP",						0, 25, 155, 95, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_CENTER, 165, 155, 150, 17,							IDC_WIFI_AP_IP},
	{"STATIC",	"Subnet mask",				0, 25, 175, 95, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_CENTER, 165, 175, 150, 17,							IDC_WIFI_AP_IPMASK},
	{"STATIC",	"Gateway",					0, 25, 195, 95, 17,													NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_CENTER, 165, 195, 150, 17,							IDC_WIFI_AP_GATEWAY},

	{"BUTTON",	"DNS",						BS_GROUPBOX, 370, 140, 345, 60,										NULL},
	
	{"STATIC",	"Primary",					0, 385, 155, 95, 17,												NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_CENTER, 525, 155, 150, 17,							IDC_WIFI_AP_DNS1},
	{"STATIC",	"Secondary",				0, 385, 175, 95, 17,												NULL},
	{"EDIT",	"",							WS_TABSTOP | ES_CENTER, 525, 175, 150, 17,							IDC_WIFI_AP_DNS2},

	{"STATIC",	"Status",					0, 375, 205, 95, 17,												NULL},
	{"EDIT",	"",							ES_CENTER | ES_READONLY, 425, 205, 150, 17,							IDC_WIFI_AP_STATUS},

	{"STATIC",	"CRC16",					0, 595, 205, 95, 17,												NULL},
	{"EDIT",	"",							ES_CENTER | ES_READONLY, 645, 205, 70, 17,							IDC_WIFI_AP_CRC16},
};

const TEMPLATE TwifiAPDlg2[] = {

	{"BUTTON",	"Backup",					WS_TABSTOP | WS_DISABLED, 15, Y_WIFI_AP_BUTTONS, 70, 20,				NULL},
	{"BUTTON",	"Restore",					WS_TABSTOP | WS_DISABLED, 90, Y_WIFI_AP_BUTTONS, 70, 20,				NULL},
	{"BUTTON",	"Update",					WS_TABSTOP | WS_DISABLED, 570, Y_WIFI_AP_BUTTONS, 70, 20,				NULL},
	{"BUTTON",	"Cancel",					WS_TABSTOP | BS_DEFPUSHBUTTON, 645, Y_WIFI_AP_BUTTONS, 70, 20,			IDCANCEL}
};
#endif