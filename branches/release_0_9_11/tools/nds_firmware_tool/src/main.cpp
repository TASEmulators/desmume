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
// first release: 17/12/2009

#include <windows.h>
#include <strsafe.h>
#include <commctrl.h>
#include "main.h"
#include "templates.h"

const char months[12][10] = { "January", "February", "March", "April", "May", "June", "July", 
							"August", "September", "October", "November", "December"};

const COLORREF firmColor[16] = {RGB( 96,128,152),	//  0 - Gray
								RGB(184, 72,  0),	//  1 - Brown
								RGB(248,  0, 24),	//  2 - Red
								RGB(248,136,248),	//  3 - Pink
								RGB(248,144,  0),	//  4 - Orange
								RGB(240,224,  0),	//  5 - Yellow
								RGB(168,248,  0),	//  6 - Lime Green
								RGB(  0,248,  0),	//  7 - Green
								RGB(  0,160, 56),	//  8 - Dark Green
								RGB( 72,216,136),	//  9 - Sea Green
								RGB( 48,184,240),	// 10 - Turquoise
								RGB(  0, 80,240),	// 11 - Blue
								RGB(  0,  0,144),	// 12 - Dark Blue
								RGB(136,  0,208),	// 13 - Dark Purple
								RGB(208,  0,232),	// 14 - Violet
								RGB(248,  0,144)	// 15 - Magenta
};

const char firmColorNames[16][16] = {"Gray","Brown","Red","Pink","Orange","Yellow","Lime Green",
                                "Green","Dark Green","Sea Green","Turquoise","Blue",
                                "Dark Blue","Dark Purple","Violet","Magenta"};

HEADER				header = {0};
u8					*gFirmwareData = NULL;
u32					gFFsize = 0;
u8					currColor = 0;

HINSTANCE			gInstance = NULL;
HWND				gMainWnd = NULL;
RECT				gMainWndPos = { CW_USEDEFAULT, CW_USEDEFAULT, 700, 555 };
HBRUSH				hbBackground = NULL;
HBRUSH				hbEditBackground = NULL;
HBRUSH				hbColors[16] = { NULL };

char				firmware_path[MAX_PATH] = { 0 };

//===============================================================================================
char *getType(const u8 type)
{
	char buf[30] = {0};

	switch (type)
	{
		case 0xFF:
			strcpy(buf, "NDS");
		break;

		case 0x20:
			strcpy(buf, "NDS-Lite");
		break;

		case 0x43:
			strcpy(buf, "iQueDS");
		break;

		case 0x63:
			strcpy(buf, "iQueDS-Lite");
		break;

		default:
			strcpy(buf, "Unknown");
		break;
	}
	return strdup(buf);
}

char *getTimeStamp(const u8 tm[])
{
	char buf[30] = {0};
	if ((tm[2]>31)| (tm[3]>12)| (tm[1]>24)| (tm[0]>59))
	{
		strcpy(buf, "ERROR");
		INFO("Error in timestamp: %02i/%02i/%02i %02i:%02i\n", tm[2], tm[3], tm[4], tm[1], tm[0]);
	}
	else
		sprintf(buf, "%02i/%02i/%02i %02i:%02i", tm[2], tm[3], tm[4], tm[1], tm[0]);
	return strdup(buf);
}

char *getVersion(const u8 ver[])
{
	char buf[30] = {0};
	sprintf(buf, "%c%c%c%c", ver[0], ver[1], ver[2], ver[3]);
	return strdup(buf);
}

char *getFlashMEversion(u8 vers, u16 vers_ex)
{
	char buf[10] = {0};
	if (vers == 1)
		strcpy(buf, "1..4");
	else
		sprintf(buf, "%i", vers_ex + 3);

	return strdup(buf);
}
//===============================================================================================
void clearMainDataWnd()
{
	for (int i = IDC_H_EDIT_START; i < IDC_H_EDIT_LAST; i++)
		SetDlgItemText(gMainWnd, i, "");
}

void refreshMainData()
{
	u16		shift1 = 0, shift2 = 0, shift3 = 0, shift4 = 0;
	u32		size_header = ((header.shift_amounts >> 12) & 0xF) * 128 * 1024;

	shift1 = ((header.shift_amounts >> 0) & 0x07);
	shift2 = ((header.shift_amounts >> 3) & 0x07);
	shift3 = ((header.shift_amounts >> 6) & 0x07);
	shift4 = ((header.shift_amounts >> 9) & 0x07);

	clearMainDataWnd();
	SetDlgItemText(gMainWnd, IDC_H1_TYPE, getType(header.console_type));
	SetDlgItemText(gMainWnd, IDC_H1_VERSION, getVersion(header.fw_identifier));
	SetDlgItemText(gMainWnd, IDC_H1_TIMESTAMP, getTimeStamp(header.fw_timestamp));
	if (size_header != gFFsize)
		SetDlgItemText(gMainWnd, IDC_H1_SIZE, "ERROR");
	else
		SetDlgItemInt(gMainWnd, IDC_H1_SIZE, size_header, false);
	SetDlgItemText(gMainWnd, IDC_H1_CRC, hex4(header.part12_boot_crc16));
	SetDlgItemText(gMainWnd, IDC_H1_ARM9_ROM, hex8(header.part1_rom_boot9_addr << (2 + shift1)));
	SetDlgItemText(gMainWnd, IDC_H1_ARM9_RAM, hex8((0x02800000 - (header.part1_ram_boot9_addr << (2+shift2)))));
	SetDlgItemText(gMainWnd, IDC_H1_ARM9_GUI, hex8((header.part2_rom_boot7_addr << (2+shift3))));
	SetDlgItemText(gMainWnd, IDC_H1_ARM7_ROM, hex8((0x03810000 - (header.part2_ram_boot7_addr << (2+shift4)))));
	SetDlgItemText(gMainWnd, IDC_H1_ARM7_RAM, hex8((header.part3_rom_gui9_addr << 3)));
	SetDlgItemText(gMainWnd, IDC_H1_ARM7_WIFI, hex8((header.part4_rom_wifi7_addr << 3)));
	//SetDlgItemText(gMainWnd, IDC_H1_ARM9_ROM, hex8((header.part5_gFirmwareData_gfx_addr << 3)));

	if (gFirmwareData[0x17C] != 0xFF)
	{
		INFO("FlashME founded\n");
		u32 patch_offset = 0x3FC80;
		if (gFirmwareData[0x17C] > 1)
			patch_offset = 0x3F680;

		memcpy(&header, gFirmwareData + patch_offset, sizeof(header));

		shift1 = ((header.shift_amounts >> 0) & 0x07);
		shift2 = ((header.shift_amounts >> 3) & 0x07);
		shift3 = ((header.shift_amounts >> 6) & 0x07);
		shift4 = ((header.shift_amounts >> 9) & 0x07);

		size_header = ((header.shift_amounts >> 12) & 0xF) * 128 * 1024;

		SetDlgItemText(gMainWnd, IDC_H2_VERSION, getFlashMEversion(gFirmwareData[0x17C], (u16)gFirmwareData[0x3F7FC]));
		if (size_header != gFFsize)
			SetDlgItemText(gMainWnd, IDC_H2_SIZE, "ERROR");
		else
			SetDlgItemInt(gMainWnd, IDC_H2_SIZE, size_header, false);
		SetDlgItemText(gMainWnd, IDC_H2_CRC, hex4(header.part12_boot_crc16));
		SetDlgItemText(gMainWnd, IDC_H2_ARM9_ROM, hex8(header.part1_rom_boot9_addr << (2 + shift1)));
		SetDlgItemText(gMainWnd, IDC_H2_ARM9_RAM, hex8((0x02800000 - (header.part1_ram_boot9_addr << (2+shift2)))));
		SetDlgItemText(gMainWnd, IDC_H2_ARM9_GUI, hex8((header.part2_rom_boot7_addr << (2+shift3))));
		SetDlgItemText(gMainWnd, IDC_H2_ARM7_ROM, hex8((0x03810000 - (header.part2_ram_boot7_addr << (2+shift4)))));
		SetDlgItemText(gMainWnd, IDC_H2_ARM7_RAM, hex8((header.part3_rom_gui9_addr << 3)));
		SetDlgItemText(gMainWnd, IDC_H2_ARM7_WIFI, hex8((header.part4_rom_wifi7_addr << 3)));
		//SetDlgItemText(gMainWnd, IDC_H2_ARM9_ROM, hex8((header.part5_gFirmwareData_gfx_addr << 3)));
	}
	else
		SetDlgItemText(gMainWnd, IDC_H2_VERSION, "not present  ");
}

bool loadFirmware()
{
	FILE	*fp = fopen(firmware_path, "rb");
	u8		*data = NULL;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		gFFsize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if( (gFFsize != 256*1024) && (gFFsize != 512*1024) )
		{
			INFO("Firmware: error size %i\n", gFFsize);
			fclose(fp);
			return false;
		}

		data = new u8 [gFFsize];
		if (!data)
		{
			INFO("Firmware: error memory allocate\n");
			fclose(fp);
			return false;
		}
		memset(data, 0, gFFsize);

		if (fread(data, 1, gFFsize, fp) != gFFsize)
		{
			INFO("Firmware: error reading\n");
			delete [] data;
			fclose(fp);
			return false; 
		}
		fclose(fp);

		
		if ((data[0x08] != 'M') ||
			(data[0x09] != 'A') ||
			(data[0x0A] != 'C'))
			{
				INFO("Firmware: error ID string\n");
				delete [] data;
				fclose(fp);
				return false;
			}
		
		SetWindowText(GetDlgItem(gMainWnd, IDC_BPATH), firmware_path);
		memcpy(&header, data, sizeof(header));
		memcpy(gFirmwareData, data, gFFsize);
		refreshMainData();
		delete [] data;
		return true;
	}
	INFO("Error opening %s\n", firmware_path);
	return false;
}

void refreshWiFiAPDlg(HWND hwnd)
{
	u8		*data = (u8*)(gFirmwareData + (header.user_settings_offset * 8 - 0x400));
	char	buf[256] = {0};

	for (int i =0; i < 3; i++)
	{
		u16	ofsID = (i * 30);
		u32	ofsMem = (i * 0x100);

		strncpy(buf, (char*)(data+ofsMem+0x40), 32);
		SetDlgItemText(hwnd, IDC_WIFI_AP_SSID + ofsID, buf);

		strncpy(buf, (char*)(data+ofsMem+0x60), 32);
		SetDlgItemText(hwnd, IDC_WIFI_AP_SSID2 + ofsID, buf);

		sprintf(buf, "%X", read64(data, ofsMem+0xF0));
		SetDlgItemText(hwnd, IDC_WIFI_AP_USER_ID + ofsID, buf);

		strncpy(buf, (char*)(data+ofsMem+0x80), 16);
		SetDlgItemText(hwnd, IDC_WIFI_AP_KEY1 + ofsID, buf);
		strncpy(buf, (char*)(data+ofsMem+0x90), 16);
		SetDlgItemText(hwnd, IDC_WIFI_AP_KEY2 + ofsID, buf);
		strncpy(buf, (char*)(data+ofsMem+0xA0), 16);
		SetDlgItemText(hwnd, IDC_WIFI_AP_KEY3 + ofsID, buf);
		strncpy(buf, (char*)(data+ofsMem+0xB0), 16);
		SetDlgItemText(hwnd, IDC_WIFI_AP_KEY4 + ofsID, buf);

		u8 tmp8 = read8(data, ofsMem+0xE6);
		if (tmp8 == 0)
		{
			SendMessage(GetDlgItem(hwnd, IDC_WIFI_AP_WEP_SIZE + ofsID), CB_SETCURSEL, 0, 0);
			SendMessage(GetDlgItem(hwnd, IDC_WIFI_AP_WEP_EMETHOD + ofsID), CB_SETCURSEL, 0, 0);
			EnableWindow(GetDlgItem(hwnd, IDC_WIFI_AP_WEP_EMETHOD + ofsID), false);
		}
		else
		{
			u8 tmp = 0;
			if (tmp8 > 3) 
			{
				tmp = tmp8 - 4;
				SendMessage(GetDlgItem(hwnd, IDC_WIFI_AP_WEP_EMETHOD + ofsID), CB_SETCURSEL, 0, 0);
			}
			else
			{
				tmp = tmp8;
				SendMessage(GetDlgItem(hwnd, IDC_WIFI_AP_WEP_EMETHOD + ofsID), CB_SETCURSEL, 1, 0);
			}
			SendMessage(GetDlgItem(hwnd, IDC_WIFI_AP_WEP_SIZE + ofsID), CB_SETCURSEL, tmp, 0);
		}

		if (read32(data, ofsMem+0xC0) == 0)
		{
			SetDlgItemText(hwnd, IDC_WIFI_AP_IP + ofsID, "DHCP");
			SetDlgItemText(hwnd, IDC_WIFI_AP_IPMASK + ofsID, "DHCP");
			SetDlgItemText(hwnd, IDC_WIFI_AP_GATEWAY + ofsID, "DHCP");
		}
		else
		{
			sprintf(buf, "%i.%i.%i.%i", read8(data, ofsMem+0xC0), read8(data, ofsMem+0xC1), read8(data, ofsMem+0xC2), read8(data, ofsMem+0xC3));
			SetDlgItemText(hwnd, IDC_WIFI_AP_IP + ofsID, buf);
			sprintf(buf, "%i.%i.%i.%i", read8(data, ofsMem+0xD0), read8(data, ofsMem+0xD1), read8(data, ofsMem+0xD2), read8(data, ofsMem+0xD3));
			SetDlgItemText(hwnd, IDC_WIFI_AP_IPMASK + ofsID, buf);
			sprintf(buf, "%i.%i.%i.%i", read8(data, ofsMem+0xC4), read8(data, ofsMem+0xC5), read8(data, ofsMem+0xC6), read8(data, ofsMem+0xC7));
			SetDlgItemText(hwnd, IDC_WIFI_AP_GATEWAY + ofsID, buf);
		}

		if (read32(data, ofsMem+0xC8) == 0)
		{
			SetDlgItemText(hwnd, IDC_WIFI_AP_DNS1 + ofsID, "DHCP");
		}
		else
		{
			sprintf(buf, "%i.%i.%i.%i", read8(data, ofsMem+0xC8), read8(data, ofsMem+0xC9), read8(data, ofsMem+0xCA), read8(data, ofsMem+0xCB));
			SetDlgItemText(hwnd, IDC_WIFI_AP_DNS1 + ofsID, buf);
		}

		if (read32(data, ofsMem+0xCC) == 0)
		{
			SetDlgItemText(hwnd, IDC_WIFI_AP_DNS2 + ofsID, "DHCP");
		}
		else
		{
			sprintf(buf, "%i.%i.%i.%i", read8(data, ofsMem+0xC8), read8(data, ofsMem+0xC9), read8(data, ofsMem+0xCA), read8(data, ofsMem+0xCB));
			SetDlgItemText(hwnd, IDC_WIFI_AP_DNS2 + ofsID, buf);
		}

		if (read8(data, ofsMem+0xE7) == 0)
			SetDlgItemText(hwnd, IDC_WIFI_AP_STATUS + ofsID, "Normal");
		else
			if (read8(data, ofsMem+0xE7) == 0x01)
				SetDlgItemText(hwnd, IDC_WIFI_AP_STATUS + ofsID, "AOSS");
			else
				if (read8(data, ofsMem+0xE7) == 0xFF)
					SetDlgItemText(hwnd, IDC_WIFI_AP_STATUS + ofsID, "not configured");

		SetDlgItemText(hwnd, IDC_WIFI_AP_CRC16 + ofsID, hex4(read16(data, ofsMem+0xFE)));
	}
}

BOOL CALLBACK wifiAPDlgProc(HWND hdlg,UINT msg, WPARAM wParam,LPARAM lParam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			HWND tmp_wnd = NULL;

			SetWindowText(hdlg, "WiFi Access Points Settings");
			u16 size = sizeof(TwifiAPDlg1)/sizeof(TwifiAPDlg1[0]);
			for (int t = 0; t < 3; t++)
			{
				u32 tmp_id = NULL;
				
				for (int i = 0; i < size; i++)
				{
					tmp_id = TwifiAPDlg1[i].id;

					if (tmp_id != NULL)
						 tmp_id += (t*30);

					CreateWindow(TwifiAPDlg1[i].wclass, TwifiAPDlg1[i].wtitle, WS_CHILD | WS_VISIBLE | TwifiAPDlg1[i].style, 
						TwifiAPDlg1[i].x, TwifiAPDlg1[i].y-10+t*220, TwifiAPDlg1[i].w, TwifiAPDlg1[i].h, hdlg, (HMENU)tmp_id, gInstance, NULL);
				}

				tmp_wnd = GetDlgItem(hdlg, IDC_WIFI_AP_WEP_SIZE + (t*30));
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"None");
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"5 bytes");
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"13 bytes");
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"16 bytes");
				SendMessage(tmp_wnd, CB_SETCURSEL, 0, 0);

				tmp_wnd = GetDlgItem(hdlg, IDC_WIFI_AP_WEP_EMETHOD + (t*30));
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"ASCII");
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Hex");
				SendMessage(tmp_wnd, CB_SETCURSEL, 0, 0);
			}

			size = sizeof(TwifiAPDlg2)/sizeof(TwifiAPDlg2[0]);
			for (int i = 0; i < size; i++)
			{
				CreateWindow(TwifiAPDlg2[i].wclass, TwifiAPDlg2[i].wtitle, WS_CHILD | WS_VISIBLE | TwifiAPDlg2[i].style, 
					TwifiAPDlg2[i].x, TwifiAPDlg2[i].y, TwifiAPDlg2[i].w, TwifiAPDlg2[i].h, hdlg, (HMENU)TwifiAPDlg2[i].id, gInstance, NULL);
			}

			refreshWiFiAPDlg(hdlg);
			break;
		}
		case WM_CTLCOLORDLG:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (BOOL)hbBackground;
		}

		case WM_CTLCOLORBTN:
			return (BOOL)hbBackground;

		case WM_CTLCOLOREDIT:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (BOOL)hbEditBackground;
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wParam;
			u32 id = GetWindowLong((HWND)lParam, GWL_ID);
			if (id >= IDC_WIFI_AP_STATUS)
			{
				SetTextColor(hdc,RGB(200,200,200));
				SetBkColor(hdc,RGB(60,60,60));
				return (LRESULT)hbEditBackground;
			}
			SetTextColor(hdc,RGB(0xFF,0xFF,0xFF));
			SetBkColor(hdc,RGB(0x6D,0x70, 0xA0));
			return (LRESULT)hbBackground;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					EndDialog(hdlg, 1);
				break;

				case IDCANCEL:
					EndDialog(hdlg, 0);
				break;

			}
		break;
	}

	return FALSE;
}

//====================================================================================================
bool refreshWiFiDlg(HWND hwnd)
{
	u8		*data = (u8*)gFirmwareData;
	u16		tmp16 = 0;
	char	buf[256] = {0};

	SetDlgItemText(hwnd, IDC_WIFI_CRC16, hex4(read16(data, 0x2A)));
	SetDlgItemInt(hwnd, IDC_WIFI_VERSION, read8(data, 0x2F), false);
	
	if (read8(data, 0x2F) > 5)
		strcpy(buf, "001656");
	else
		strcpy(buf, "0009BF");
	sprintf(buf, "%s%02X%02X%02X", buf, read8(data, 0x36), read8(data, 0x37), read8(data, 0x38));
	SetDlgItemText(hwnd, IDC_WIFI_MACID, buf);
	SetDlgItemInt(hwnd, IDC_WIFI_RFTYPE, read8(data, 0x40), false);


	tmp16 = read16(data, 0x3C) & 0x7FFE;
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_01), BM_SETCHECK, (tmp16 & (1<<1))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_02), BM_SETCHECK, (tmp16 & (1<<2))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_03), BM_SETCHECK, (tmp16 & (1<<3))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_04), BM_SETCHECK, (tmp16 & (1<<4))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_05), BM_SETCHECK, (tmp16 & (1<<5))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_06), BM_SETCHECK, (tmp16 & (1<<6))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_07), BM_SETCHECK, (tmp16 & (1<<7))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_08), BM_SETCHECK, (tmp16 & (1<<8))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_09), BM_SETCHECK, (tmp16 & (1<<9))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_10), BM_SETCHECK, (tmp16 & (1<<10))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_11), BM_SETCHECK, (tmp16 & (1<<11))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_12), BM_SETCHECK, (tmp16 & (1<<12))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_13), BM_SETCHECK, (tmp16 & (1<<13))?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_WIFI_ECHANNEL_14), BM_SETCHECK, (tmp16 & (1<<14))?BST_CHECKED:BST_UNCHECKED, 0);

	SetDlgItemText(hwnd, IDC_WIFI_044, hex4w(read16(data, 0x44)));
	SetDlgItemText(hwnd, IDC_WIFI_046, hex4w(read16(data, 0x46)));
	SetDlgItemText(hwnd, IDC_WIFI_048, hex4w(read16(data, 0x48)));
	SetDlgItemText(hwnd, IDC_WIFI_04A, hex4w(read16(data, 0x4A)));
	SetDlgItemText(hwnd, IDC_WIFI_04C, hex4w(read16(data, 0x4C)));
	SetDlgItemText(hwnd, IDC_WIFI_04E, hex4w(read16(data, 0x4E)));
	SetDlgItemText(hwnd, IDC_WIFI_050, hex4w(read16(data, 0x50)));
	SetDlgItemText(hwnd, IDC_WIFI_052, hex4w(read16(data, 0x52)));
	SetDlgItemText(hwnd, IDC_WIFI_054, hex4w(read16(data, 0x54)));
	SetDlgItemText(hwnd, IDC_WIFI_056, hex4w(read16(data, 0x56)));
	SetDlgItemText(hwnd, IDC_WIFI_058, hex4w(read16(data, 0x58)));
	SetDlgItemText(hwnd, IDC_WIFI_05A, hex4w(read16(data, 0x5A)));
	SetDlgItemText(hwnd, IDC_WIFI_05C, hex4w(read16(data, 0x5C)));
	SetDlgItemText(hwnd, IDC_WIFI_05E, hex4w(read16(data, 0x5E)));
	SetDlgItemText(hwnd, IDC_WIFI_060, hex4w(read16(data, 0x60)));
	SetDlgItemText(hwnd, IDC_WIFI_062, hex4w(read16(data, 0x62)));

	for (int i = 0; i < 0x69; i++)
		SetDlgItemText(hwnd, IDC_WIFI_BB + i, hex2w(read8(data, 0x64+i)));

	if (read8(data, 0x44) == 2)
	{
		for (int i = 0; i < (0x24 / 3); i++)
			SetDlgItemText(hwnd, IDC_WIFI_RF2_INIT + i, hex6w(read24(data, 0xCE+(i*3))));

		for (int i = 0; i < (0x54 / 3 / 2); i++)
		{
			// check
			SetDlgItemText(hwnd, IDC_WIFI_RF2_56_CH + i, hex6w(read24(data, 0xF2+(i*3))));
			SetDlgItemText(hwnd, IDC_WIFI_RF2_56_CH + i + 14, hex6w(read24(data, (0xF2+3*14)+(i*3))));
		}

		for (int i = 0; i < 0x0E; i++)
		{
			SetDlgItemText(hwnd, IDC_WIFI_RF2_BB_RF_CH + i, hex2w(read8(data, 0x146+i)));
			SetDlgItemText(hwnd, IDC_WIFI_RF2_BB_RF_CH + i + 14, hex2w(read8(data, 0x154+i)));
		}
	}
	else
		if (read8(gFirmwareData, 0x44) == 3)
		{
			// TODO
		}

	return true;
}

BOOL CALLBACK wifiDlgProc(HWND hdlg,UINT msg, WPARAM wParam,LPARAM lParam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			SetWindowText(hdlg, "WiFi Settings");
			u16 size = sizeof(TwifiDlg1)/sizeof(TwifiDlg1[0]);
			for (int i = 0; i < size; i++)
			{
				CreateWindow(TwifiDlg1[i].wclass, TwifiDlg1[i].wtitle, WS_CHILD | WS_VISIBLE | TwifiDlg1[i].style, 
					TwifiDlg1[i].x, TwifiDlg1[i].y, TwifiDlg1[i].w, TwifiDlg1[i].h, hdlg, (HMENU)TwifiDlg1[i].id, gInstance, NULL);
			}

			u32 x = 40, y = Y_WIFI_BB+15;
			//=========================================================
			for (int i = 0; i < 0x69; i++)
			{
				CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER, 
					x, y, 20, 20, hdlg, (HMENU)(IDC_WIFI_BB + i), gInstance, NULL);
				x += 25;
				if ((i > 20) && ((i % 26) == 0))
				{
					x = 40;
					y +=25;
				}
			}

			if (read8(gFirmwareData, 0x44) == 2)
			{
				u8 rfIndexes[] = {0, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 1, 2, 3};
				CreateWindow("BUTTON", "RF chip Type 2", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 
					5, Y_WIFI_RF, 720, 290, hdlg, (HMENU)NULL, gInstance, NULL);
				CreateWindow("BUTTON", "RFx Initial values...", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 
					15, Y_WIFI_RF+15, 700, 75, hdlg, (HMENU)NULL, gInstance, NULL);
				y = 0; x = 60;
				for (int i = 0; i < (0x24 / 3); i++)
				{
					char buf[10] = { 0 };
					sprintf(buf, "%02X", rfIndexes[i]);
					CreateWindow("STATIC", buf, WS_CHILD | WS_VISIBLE, 
						x, Y_WIFI_RF + 35 + y, 30, 18, hdlg, (HMENU)NULL, gInstance, NULL);
					CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER, 
						x + 18, Y_WIFI_RF + 35 + y, 65, 18, hdlg, (HMENU)(IDC_WIFI_RF2_INIT + i), gInstance, NULL);
					x += 105;
					if (i == 5)
					{
						y = 25;
						x = 60;
					}
				}

				CreateWindow("BUTTON", "RF values (channel 1..14)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 
											15, Y_WIFI_RF+95, 700, 115, hdlg, (HMENU)NULL, gInstance, NULL);
				CreateWindow("STATIC", "RF5", WS_CHILD | WS_VISIBLE, 25, Y_WIFI_RF + 115, 30, 18, hdlg, (HMENU)NULL, gInstance, NULL);
				CreateWindow("STATIC", "RF6", WS_CHILD | WS_VISIBLE, 25, Y_WIFI_RF + 160, 30, 18, hdlg, (HMENU)NULL, gInstance, NULL);
				y = 0; x = 60;
				for (int i = 0; i < 14; i++)
				{
					CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER, 
						x, Y_WIFI_RF + 115 + y, 65, 18, hdlg, (HMENU)(IDC_WIFI_RF2_56_CH + i), gInstance, NULL);
					x += 95;
					if (i == 6)
					{
						y = 22;
						x = 60;
					}
				}

				y = 0; x = 60;
				for (int i = 0; i < 14; i++)
				{
					CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER, 
						x, Y_WIFI_RF + 160 + y, 65, 18, hdlg, (HMENU)(IDC_WIFI_RF2_56_CH + i + 14), gInstance, NULL);
					x += 95;
					if (i == 6)
					{
						y = 22;
						x = 60;
					}
				}

				CreateWindow("BUTTON", "BB/RF 8bit values (channel 1..14)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 
											15, Y_WIFI_RF+210, 700, 70, hdlg, (HMENU)NULL, gInstance, NULL);
				CreateWindow("STATIC", "BB", WS_CHILD | WS_VISIBLE, 25, Y_WIFI_RF + 230, 30, 18, hdlg, (HMENU)NULL, gInstance, NULL);
				CreateWindow("STATIC", "RF9", WS_CHILD | WS_VISIBLE, 25, Y_WIFI_RF + 250, 30, 18, hdlg, (HMENU)NULL, gInstance, NULL);
				for (int i = 0; i < 14; i++)
				{
					CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER, 
						60+ (i*45), Y_WIFI_RF + 230, 35, 18, hdlg, (HMENU)(IDC_WIFI_RF2_BB_RF_CH + i), gInstance, NULL);
				}
				for (int i = 0; i < 14; i++)
				{
					CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER, 
						60+ (i*45), Y_WIFI_RF + 250, 35, 18, hdlg, (HMENU)(IDC_WIFI_RF2_BB_RF_CH + i + 14), gInstance, NULL);
				}
			}
			else
				if (read8(gFirmwareData, 0x44) == 3)
				{
					// TODO
				}

			size = sizeof(TwifiDlg2)/sizeof(TwifiDlg2[0]);
			for (int i = 0; i < size; i++)
			{
				CreateWindow(TwifiDlg2[i].wclass, TwifiDlg2[i].wtitle, WS_CHILD | WS_VISIBLE | TwifiDlg2[i].style, 
					TwifiDlg2[i].x, TwifiDlg2[i].y, TwifiDlg2[i].w, TwifiDlg2[i].h, hdlg, (HMENU)TwifiDlg2[i].id, gInstance, NULL);
			}
			refreshWiFiDlg(hdlg);
			break;
		}
		case WM_CTLCOLORDLG:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (BOOL)hbBackground;
		}

		case WM_CTLCOLORBTN:
			return (BOOL)hbBackground;

		case WM_CTLCOLOREDIT:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (BOOL)hbEditBackground;
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wParam;
			u32 id = GetWindowLong((HWND)lParam, GWL_ID);
			if ((id > IDC_WIFI_EDIT_STATIC) && (id < IDC_WIFI_EDIT_STATIC + 100))
			{
				SetTextColor(hdc,RGB(200,200,200));
				SetBkColor(hdc,RGB(60,60,60));
				return (LRESULT)hbEditBackground;
			}
			SetTextColor(hdc,RGB(0xFF,0xFF,0xFF));
			SetBkColor(hdc,RGB(0x6D,0x70, 0xA0));
			return (LRESULT)hbBackground;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					EndDialog(hdlg, 1);
				break;

				case IDCANCEL:
					EndDialog(hdlg, 0);
				break;

			}
		break;
	}

	return FALSE;
}

bool refreshUserDlg(HWND hwnd)
{
	u8		*data = (u8*)(gFirmwareData + (header.user_settings_offset * 8));
	char	buf[256] = { 0 };

	wcstombs(buf, (wchar_t *)(data+0x06), read16(data,0x1A));
	SetDlgItemText(hwnd, IDC_NICKNAME, buf);

	memset(buf, 0, sizeof(buf));
	wcstombs(buf, (wchar_t *)(data+0x1C), read16(data,0x50));
	SetDlgItemText(hwnd, IDC_MESSAGE, buf);

	SendMessage(GetDlgItem(hwnd, IDC_BIRTH_DAY), CB_SETCURSEL, data[0x04]-1, 0);
	SendMessage(GetDlgItem(hwnd, IDC_BIRTH_MONTH), CB_SETCURSEL, data[0x03]-1, 0);

	// lang
	if ((header.console_type == 0x43) || (header.console_type == 0x63))
	{
		SendMessage(GetDlgItem(hwnd, IDC_LANGUAGE), CB_SETCURSEL, read16(data,0x75)&0x07, 0);
	}
	else
	{
		SendMessage(GetDlgItem(hwnd, IDC_LANGUAGE), CB_SETCURSEL, read16(data,0x64)&0x07, 0);
	}

	SendMessage(GetDlgItem(hwnd, IDC_ALARM_ON), BM_SETCHECK, data[0x56]?BST_CHECKED:BST_UNCHECKED, 0);
	SendMessage(GetDlgItem(hwnd, IDC_ALARM_HOUR), CB_SETCURSEL, data[0x52], 0);
	SendMessage(GetDlgItem(hwnd, IDC_ALARM_MIN), CB_SETCURSEL, data[0x53], 0);

	currColor = read8(data, 0x02);

	SendMessage(GetDlgItem(hwnd, IDC_GBA_MODE_SCREENS), CB_SETCURSEL, (read16(data,0x64) >> 3)&0x01, 0);
	SendMessage(GetDlgItem(hwnd, IDC_BRIGHTLIGHT_LEVEL), CB_SETCURSEL, (read16(data,0x64) >> 4)&0x03, 0);
	SendMessage(GetDlgItem(hwnd, IDC_BOOTMENU), CB_SETCURSEL, (read16(data,0x64) >> 6)&0x01, 0);

	SetDlgItemText(hwnd, IDC_TADC_X1, hex2w(read16(data,0x58)&0xFFF));
	SetDlgItemText(hwnd, IDC_TADC_Y1, hex2w(read16(data,0x5A)&0xFFF));
	SetDlgItemText(hwnd, IDC_TADC_X2, hex2w(read16(data,0x5E)&0xFFF));
	SetDlgItemText(hwnd, IDC_TADC_Y2, hex2w(read16(data,0x60)&0xFFF));

	SetDlgItemText(hwnd, IDC_TSCR_X1, hex2w(read16(data,0x5C)&0xFF));
	SetDlgItemText(hwnd, IDC_TSCR_Y1, hex2w(read16(data,0x5F)&0xFF));
	SetDlgItemText(hwnd, IDC_TSCR_X2, hex2w(read16(data,0x62)&0xFF));
	SetDlgItemText(hwnd, IDC_TSCR_Y2, hex2w(read16(data,0x64)&0xFF));

	SetDlgItemInt(hwnd, IDC_RTC_OFFSET, read32(data,0x68), true);
	SetDlgItemInt(hwnd, IDC_YEAR_BOOT, 2000+read8(data,0x66), false);
	SetDlgItemInt(hwnd, IDC_UPDATE_COUNTER, read16(data,0x70), false);
	SetDlgItemText(hwnd, IDC_USER_CRC16, hex4(read16(data,0x72)));

	return true;
}

BOOL CALLBACK userDlgProc(HWND hdlg,UINT msg, WPARAM wParam,LPARAM lParam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			SetWindowText(hdlg, "User settings");
			u16 size = sizeof(TuserDlg1)/sizeof(TuserDlg1[0]);
			for (int i = 0; i < size; i++)
			{
				CreateWindow(TuserDlg1[i].wclass, TuserDlg1[i].wtitle, WS_CHILD | WS_VISIBLE | TuserDlg1[i].style, 
					TuserDlg1[i].x, TuserDlg1[i].y, TuserDlg1[i].w, TuserDlg1[i].h, hdlg, (HMENU)TuserDlg1[i].id, gInstance, NULL);
			}

			u32 y_ofs = Y_FAV_COLOR + 15, x_ofs = 0;
			for (int i = 0; i < 16; i++)
			{
				char buf[22] = { 0 };
				sprintf(buf,"%02i",i);
				CreateWindow("BUTTON", buf, WS_CHILD | WS_TABSTOP | WS_VISIBLE | BS_OWNERDRAW, 25 + x_ofs, 5 + y_ofs, 40, 40, hdlg, (HMENU)(IDC_COLOR+i), gInstance, NULL);
				x_ofs += 45;
				if (i == 7)
				{
					x_ofs = 0;
					y_ofs += 45;
				}
			}

			size = sizeof(TuserDlg2)/sizeof(TuserDlg2[0]);
			for (int i = 0; i < size; i++)
			{
				CreateWindow(TuserDlg2[i].wclass, TuserDlg2[i].wtitle, WS_CHILD | WS_VISIBLE | TuserDlg2[i].style, 
					TuserDlg2[i].x, TuserDlg2[i].y, TuserDlg2[i].w, TuserDlg2[i].h, hdlg, (HMENU)TuserDlg2[i].id, gInstance, NULL);
			}

			HWND tmp_wnd = GetDlgItem(hdlg, IDC_BIRTH_DAY);
			for (int i = 1; i < 32; i++)
			{
				char bf[10] = {0};
				_itoa(i, bf, 10);
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)bf);
			}

			tmp_wnd = GetDlgItem(hdlg, IDC_BIRTH_MONTH);
			for (int i = 1; i < 13; i++)
			{
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)months[i-1]);
			}

			tmp_wnd = GetDlgItem(hdlg, IDC_ALARM_HOUR);
			for (int i = 0; i < 24; i++)
			{
				char bf[10] = {0};
				itoa(i, bf, 10);
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)bf);
			}
			
			tmp_wnd = GetDlgItem(hdlg, IDC_ALARM_MIN);
			for (int i = 0; i < 60; i++)
			{
				char bf[10] = {0};
				itoa(i, bf, 10);
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)bf);
			}

			tmp_wnd = GetDlgItem(hdlg, IDC_LANGUAGE);
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Japanese");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"English");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"French");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"German");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Italian");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Spanish");
			if ((header.console_type == 0x43) || (header.console_type == 0x63))
				SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Chinese");

			tmp_wnd = GetDlgItem(hdlg, IDC_GBA_MODE_SCREENS);
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Upper");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Lower");

			tmp_wnd = GetDlgItem(hdlg, IDC_BRIGHTLIGHT_LEVEL);
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Low");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Med");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"High");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Max");

			tmp_wnd = GetDlgItem(hdlg, IDC_BOOTMENU);
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Manual/bootmenu");
			SendMessage(tmp_wnd, CB_ADDSTRING, 0, (LPARAM)"Autostart Cartridge");
			
			refreshUserDlg(hdlg);
			return TRUE;
		}

		case WM_DRAWITEM:
		{
			if ((wParam >= IDC_COLOR) && (wParam <= (IDC_COLOR + 15)))
			{
				LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lParam;
				FillRect(di->hDC, &di->rcItem, hbColors[wParam - IDC_COLOR]);
				
				if (di->itemState & ODS_FOCUS)
				{
					ExcludeClipRect(di->hDC, di->rcItem.left+4, di->rcItem.top+4, di->rcItem.right-4, di->rcItem.bottom-4);
					HPEN pen = CreatePen(PS_DASH, 1, RGB(255,255,255));
					SelectObject(di->hDC, pen);
					Rectangle(di->hDC, di->rcItem.left+2, di->rcItem.top+2, di->rcItem.right-2, di->rcItem.bottom-2);
				}
				ExcludeClipRect(di->hDC, di->rcItem.left+2, di->rcItem.top+2, di->rcItem.right-2, di->rcItem.bottom-2);
				if (di->itemState & ODS_FOCUS)
				{
					HPEN pen = CreatePen(PS_DASH, 3, RGB(0,0,0));
					SelectObject(di->hDC, pen);
					Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
				}
				else
				{
					if ((wParam - IDC_COLOR) == currColor)
					{
						HPEN pen = CreatePen(PS_DASH, 3, RGB(255,255,255));
						SelectObject(di->hDC, pen);
						Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);
					}
				}
				return TRUE;
			}

			break;
		}
		case WM_CTLCOLORDLG:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (BOOL)hbBackground;
		}

		case WM_CTLCOLORBTN:
			return (BOOL)hbBackground;

		case WM_CTLCOLOREDIT:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (BOOL)hbEditBackground;
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(0xFF,0xFF,0xFF));
			SetBkColor(hdc,RGB(0x6D,0x70, 0xA0));
			return (LRESULT)hbBackground;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					EndDialog(hdlg, 1);
				break;

				case IDCANCEL:
					EndDialog(hdlg, 0);
				break;

			}
		break;
	}
	return FALSE;
}

BOOL Browse()
{
	static OPENFILENAME ofn={0};
	static BOOL bSetInitialDir = FALSE;

	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = gMainWnd;
	ofn.lpstrFilter       = NULL;
	ofn.lpstrFilter       = "Binary (*.bin,*.rom)\0*.bin; *.rom\0All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex      = 1;
	ofn.lpstrFile         = firmware_path;
	ofn.nMaxFile          = MAX_PATH;
	ofn.lpstrTitle        = TEXT("Open firmware\0");
	ofn.lpstrFileTitle    = NULL;
	ofn.lpstrDefExt       = TEXT("*\0");
	ofn.Flags             = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST| OFN_HIDEREADONLY;
	ofn.lpstrInitialDir = NULL;

	return GetOpenFileName((LPOPENFILENAME)&ofn);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_CREATE:
		{
			u16 size = sizeof(TmainWnd)/sizeof(TmainWnd[0]);
			for (int i = 0; i < size; i++)
			{
				if (!CreateWindow(TmainWnd[i].wclass, TmainWnd[i].wtitle, WS_CHILD | WS_VISIBLE | TmainWnd[i].style, 
					TmainWnd[i].x, TmainWnd[i].y, TmainWnd[i].w, TmainWnd[i].h, hwnd, (HMENU)TmainWnd[i].id, gInstance, NULL)) 
				{
					INFO("ERROR: %03i - %s: %s\n", i, TmainWnd[i].wtitle, error());
					return -1;
				}
			}

			return 0L;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0L;
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wParam;
			u32 id = GetWindowLong((HWND)lParam, GWL_ID);

			if (id == IDC_BPATH)
			{
				SetTextColor(hdc,RGB(200,200,200));
				SetBkColor(hdc,RGB(60,60,60));
				return (LRESULT)hbEditBackground;
			}
			if ((id > IDC_H_EDIT_START) && (id < IDC_H_EDIT_LAST))
			{
				char buf2[512] = {0};
				GetWindowText((HWND)lParam, buf2, 512);
				if (strcmp(buf2, "not present  ") == 0)
				{
					SetTextColor(hdc,RGB(0xED,0xF5,0x11));
				}
				else
				if (strcmp(buf2, "N/A") == 0)
				{
					SetTextColor(hdc,RGB(0xFF,0x0F,0xFF));
				}
				else
				if (strcmp(buf2, "ERROR") == 0)
				{
					SetTextColor(hdc,RGB(0xFF,0x0F,0x0F));
				}
				else
				{
					SetTextColor(hdc,RGB(0xFF,0xFF,0xFF));
				}
				SetBkColor(hdc,RGB(60,60,60));
				return (LRESULT)hbEditBackground;
			}
			else
			{
				SetTextColor(hdc,RGB(0xFF,0xFF,0xFF));
				SetBkColor(hdc,RGB(0x6D,0x70, 0xA0));
				return (LRESULT)hbBackground;
			}
		}

		case WM_CTLCOLORBTN:
			return (LRESULT)hbBackground;

		case WM_CTLCOLOREDIT:
		{
			HDC hdc = (HDC)wParam;
			SetTextColor(hdc,RGB(200,200,200));
			SetBkColor(hdc,RGB(60,60,60));
			return (LRESULT)hbEditBackground;
		}

		case WM_COMMAND:
		{
			u32 id = GetWindowLong((HWND)lParam, GWL_ID);
			switch (id)
            {
				case IDC_BBROWSE:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						char tmp_buf[MAX_PATH] = { 0 };
						strcpy(tmp_buf, firmware_path);
						if (Browse())
						{
							if (loadFirmware())
							{
								EnableWindow(GetDlgItem(hwnd, IDC_BUSER), TRUE);
								EnableWindow(GetDlgItem(hwnd, IDC_BWIFI), TRUE);
								EnableWindow(GetDlgItem(hwnd, IDC_BWIFIAP), TRUE);
								return 0L;
							}
							MessageBox(NULL, "Error loading firmware", _TITLE, MB_OK | MB_ICONERROR);
							INFO("Error loading firmware\n");
						}
						strcpy(firmware_path, tmp_buf);
						return 0L;
					}
					break;

				case IDC_BEXIT:
					if (HIWORD(wParam) == BN_CLICKED)
						SendMessage(hwnd, WM_CLOSE, 0, 0);
					break;

				case IDC_BUSER:
				{
					LPDLGTEMPLATE dlgTemplate = (LPDLGTEMPLATE)malloc(1024);
					if (dlgTemplate)
					{
						memset(dlgTemplate, 0, 1024);
						dlgTemplate->style = DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
						dlgTemplate->cx = 205;
						dlgTemplate->cy = 300;
						if (DialogBoxIndirect(gInstance, dlgTemplate, hwnd, userDlgProc) == -1)
							INFO("ERROR: %s\n", error());
						free(dlgTemplate);
						return 0L;
					}
					INFO("ERROR: %s\n", error());
					return 0L;
				}
				case IDC_BWIFI:
				{
					LPDLGTEMPLATE dlgTemplate = (LPDLGTEMPLATE)malloc(1024);
					if (dlgTemplate)
					{
						memset(dlgTemplate, 0, 1024);
						dlgTemplate->style = DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
						dlgTemplate->cx = 365;
						dlgTemplate->cy = 300;
						if (DialogBoxIndirect(gInstance, dlgTemplate, hwnd, wifiDlgProc) == -1)
							INFO("ERROR: %s\n", error());
						free(dlgTemplate);
						return 0L;
					}
					INFO("ERROR: %s\n", error());
					return 0L;
				}

				case IDC_BWIFIAP:
				{
					LPDLGTEMPLATE dlgTemplate = (LPDLGTEMPLATE)malloc(1024);
					if (dlgTemplate)
					{
						memset(dlgTemplate, 0, 1024);
						dlgTemplate->style = DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
						dlgTemplate->cx = 365;
						dlgTemplate->cy = 350;
						if (DialogBoxIndirect(gInstance, dlgTemplate, hwnd, wifiAPDlgProc) == -1)
							INFO("ERROR: %s\n", error());
						free(dlgTemplate);
						return 0L;
					}
					INFO("ERROR: %s\n", error());
					return 0L;
				}
			}
		}
	}
	return DefWindowProc(hwnd,msg,wParam,lParam);
}

int RegClass(WNDPROC Proc, LPCTSTR szName)
{
	WNDCLASS	wc = { 0 };

	wc.style=CS_HREDRAW|CS_VREDRAW;
	wc.cbClsExtra=wc.cbWndExtra=0;
	wc.lpfnWndProc=Proc;
	wc.hInstance=gInstance;
	wc.hIcon=LoadIcon(gInstance, IDI_APPLICATION);
	wc.hCursor=LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)hbBackground;
	wc.lpszMenuName=(LPCTSTR)NULL;
	wc.lpszClassName=szName;
	return RegisterClass(&wc);
}

BOOL InitCommonCtrls()
{
	INITCOMMONCONTROLSEX	CommCtrl;
	
	CommCtrl.dwSize=sizeof INITCOMMONCONTROLSEX;
	CommCtrl.dwICC=ICC_WIN95_CLASSES|ICC_COOL_CLASSES|ICC_USEREX_CLASSES;
   	return InitCommonControlsEx(&CommCtrl);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	InitCommonCtrls();
	hbBackground = CreateSolidBrush(RGB(0x6D,0x70, 0xA0));
	hbEditBackground = CreateSolidBrush(RGB(60, 60, 60));
	OpenConsole();
	gInstance = hInstance;
	if (!RegClass(WndProc, MAINCLASS))
	{
		DeleteObject(hbEditBackground); DeleteObject(hbBackground);
		MessageBox(NULL, "Error registering class", _TITLE, MB_OK | MB_ICONERROR);
		return -1;
	}

	gFirmwareData = new u8 [512 * 1024];
	if (!gFirmwareData)
	{
		DeleteObject(hbEditBackground); DeleteObject(hbBackground);
		MessageBox(NULL, "Error allocating firmware data", _TITLE, MB_OK | MB_ICONERROR);
		UnregisterClass(MAINCLASS, gInstance);
		return -2;
	}
	memset(gFirmwareData, 0, 512*1024);

	gMainWnd=CreateWindow(MAINCLASS, _TITLE,
							WS_OVERLAPPED | WS_SYSMENU,
							gMainWndPos.left,gMainWndPos.top,
							gMainWndPos.right,gMainWndPos.bottom,
							NULL, (HMENU)NULL, hInstance,NULL);

	if (!gMainWnd)
	{
		DeleteObject(hbEditBackground); DeleteObject(hbBackground);
		MessageBox(NULL, "Error creating main window", _TITLE, MB_OK | MB_ICONERROR);
		UnregisterClass(MAINCLASS, gInstance);
		delete [] gFirmwareData;
		gFirmwareData = NULL;
		return -3;
	}

	for (int t = 0; t < 16; t++)
		hbColors[t] = CreateSolidBrush(firmColor[t]);

	ShowWindow(gMainWnd, SW_NORMAL);
	UpdateWindow(gMainWnd);
	while (GetMessage(&msg, 0,0,0))
	{
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	for (int t = 0; t < 16; t++)
	{
		DeleteObject(hbColors[t]);
	}
	DeleteObject(hbEditBackground);
	DeleteObject(hbBackground);
	
	delete [] gFirmwareData;
	gFirmwareData = NULL;
	UnregisterClass(MAINCLASS, gInstance);
	CloseConsole();

	return 0;
}