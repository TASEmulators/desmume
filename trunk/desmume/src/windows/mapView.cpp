/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2011 DeSmuME team

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

#include "mapView.h"
#include "resource.h"

#include <commctrl.h>
#include "../MMU.h"
#include "../NDSSystem.h"
#include "debug.h"
#include "main.h"
#include "windriver.h"

using namespace GPU_EXT;

struct mapview_struct
{
	u32	autoup_secs;
	bool autoup;

	u8 map;
	u16 lcd;
	u16 bitmap[1024*1024];
	bool clear;

	void render()
	{
		//we're going to make a copy of the gpu so that we don't wreck affine scroll params
		//hopefully we won't mess up anything else
		GPU *realGpu;
		if(lcd) realGpu = SubScreen.gpu;
		else realGpu = MainScreen.gpu;
		GPU &gpu = *realGpu;

		//forgive the gyrations, some of this junk in here is to remind us of gyrations we might have to go
		//through to avoid breaking the gpu struct

		gpu.currBgNum = map;
		gpu.debug = true;
		int temp = gpu.setFinalColorBck_funcNum;
		gpu.setFinalColorBck_funcNum = 0; //hax... why arent we copying gpu now?? i cant remember

		memset(bitmap,0,sizeof(bitmap));

		for(u32 i = 0; i < gpu.BGSize[map][1]; ++i)
		{
			gpu.currDst = (u8 *)bitmap + i*gpu.BGSize[map][0]*2;
			gpu.currLine = i;
			gpu.modeRender<false>(map);
		}
		gpu.debug = false;
		gpu.setFinalColorBck_funcNum = temp;

	}
};

mapview_struct	*MapView = NULL;


LRESULT MapView_OnPaint(mapview_struct * win, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	Lock lock;

	HDC          hdc;
	PAINTSTRUCT  ps;
	char text[80];
	u32 dispcnt = ((volatile u32 *)MMU.ARM9_REG)[(win->lcd*0x400)];
	u32 bgcnt =  ((volatile u16 *)MMU.ARM9_REG)[(8 + (win->map<<1) + (win->lcd*0x1000))>>1];
	BITMAPV4HEADER bmi;
	u16 lg;
	u16 ht;
	//BGxPARMS * parms;

	//CreateBitmapIndirect(&bmi);
	memset(&bmi, 0, sizeof(bmi));
	bmi.bV4Size = sizeof(bmi);
	bmi.bV4Planes = 1;
	bmi.bV4BitCount = 16;
	bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
	bmi.bV4RedMask = 0x001F;
	bmi.bV4GreenMask = 0x03E0;
	bmi.bV4BlueMask = 0x7C00;

	if(win->lcd)
	{
		lg = SubScreen.gpu->BGSize[win->map][0];
		ht = SubScreen.gpu->BGSize[win->map][1];
	}
	else
	{
		lg = MainScreen.gpu->BGSize[win->map][0];
		ht = MainScreen.gpu->BGSize[win->map][1];
	}
	bmi.bV4Width = lg;
	bmi.bV4Height = -ht;

	hdc = BeginPaint(hwnd, &ps);        

	sprintf(text, "%d %08X, %08X", (int)(dispcnt&7), (int)dispcnt, (int)bgcnt);
	SetWindowText(GetDlgItem(hwnd, IDC_MODE), text);

	if(!(bgcnt&(1<<7)))
		sprintf(text, "normal 16");
	else
	{
		if(!(dispcnt&(1<<30)))
			sprintf(text, "normal 256");
		else
		{
			switch(win->map)
			{
			case 0 :
				sprintf(text, "extended slot %d", (bgcnt&(1<<13))?2:0);
				break;
			case 1 :
				sprintf(text, "extended slot %d", (bgcnt&(1<<13))?3:1);
				break;
			default :
				sprintf(text, "extended slot %d", MainScreen.gpu->BGExtPalSlot[win->map]);
				break;
			}
		}
	}
	SetWindowText(GetDlgItem(hwnd, IDC_PAL), text);

	sprintf(text, "%d", (int)(bgcnt&3));
	SetWindowText(GetDlgItem(hwnd, IDC_PRIO), text);


	if((dispcnt>>8>>win->map)&1)
		SetWindowText(GetDlgItem(hwnd, IDC_VISIBLE), "true");
	else
		SetWindowText(GetDlgItem(hwnd, IDC_VISIBLE), "false");

	sprintf(text, "0x%08X", (int)(0x6000000 + ((bgcnt>>2)&0xF)*0x4000 + win->lcd*0x200000 +((dispcnt>>24)&7)*0x10000));
	SetWindowText(GetDlgItem(hwnd, IDC_CHAR), text);

	sprintf(text, "0x%08X", (int)(0x6000000 + 0x800*((bgcnt>>8)&0x1F) + win->lcd*0x200000 + ((dispcnt>>27)&7)*0x10000));
	SetWindowText(GetDlgItem(hwnd, IDC_SCR), text);

	//sprintf(text, "%d x %d",  MainScreen.gpu->BGPA[win->map], MainScreen.gpu->BGPB[win->map]);
	sprintf(text, "%d x %d",  (int)MainScreen.gpu->BGSize[win->map][0], (int)MainScreen.gpu->BGSize[win->map][1]);
	SetWindowText(GetDlgItem(hwnd, IDC_MSIZE), text);

	//if (win->map==2) {
	//	parms = &(MainScreen.gpu->dispx_st)->dispx_BG2PARMS;
	//} else {
	//	parms = &(MainScreen.gpu->dispx_st)->dispx_BG3PARMS;		
	//}
	//sprintf(text, "%d x %d", parms->BGxX, parms->BGxY);
	SetWindowText(GetDlgItem(hwnd, IDC_SCROLL), "useless");

	for(int i = 0; i < ARRAY_SIZE(win->bitmap); i++)
		win->bitmap[i] = 0x7C1F;

	win->render();

	if(win->clear)
	{
		RECT r; 
		r.left = 200; 
		r.top = 4; 
		r.right = 200 + 1024; 
		r.bottom = 200 + 1024;
		HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
		FillRect(hdc, &r, brush); 
		DeleteObject(brush);
		win->clear = false;
	}

	SetDIBitsToDevice(hdc, 200, 4, lg, ht, 0, 0, 0, ht, win->bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

	EndPaint(hwnd, &ps);

	return 0;
}

BOOL CALLBACK ViewMapsProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//bail out early if the dialog isnt initialized
	if(!MapView && message != WM_INITDIALOG)
		return false;

	switch (message)
	{
	case WM_INITDIALOG :
		{
			MapView = new mapview_struct;
			memset(MapView, 0, sizeof(MapView));
			MapView->clear = true;
			MapView->autoup_secs = 1;
			MapView->map = 0;
			MapView->lcd = 0;
			SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
				UDM_SETRANGE, 0, MAKELONG(99, 1));
			SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
				UDM_SETPOS32, 0, MapView->autoup_secs);
			HWND combo = GetDlgItem(hwnd, IDC_BG_SELECT);
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 0");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 1");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 2");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 3");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 0");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 1");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 2");
			SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 3");
			SendMessage(combo, CB_SETCURSEL, 0, 0);
		}
		return 1;
	case WM_CLOSE :
		{
			if(MapView->autoup)
			{
				KillTimer(hwnd, IDT_VIEW_MAP);
				MapView->autoup = false;
			}
			delete MapView;
			MapView = NULL;
			//INFO("Close Map view dialog\n");
			PostQuitMessage(0);
			return 0;
		}
	case WM_PAINT:
		MapView_OnPaint(MapView, hwnd, wParam, lParam);
		return 1;
	case WM_TIMER:
		SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
		return 1;
	case WM_COMMAND :
		switch (LOWORD (wParam))
		{
		case IDC_FERMER :
			SendMessage(hwnd, WM_CLOSE, 0, 0);
			return 1;
		case IDC_AUTO_UPDATE :
			if(MapView->autoup)
			{
				EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
				EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
				KillTimer(hwnd, IDT_VIEW_MAP);
				MapView->autoup = FALSE;
				return 1;
			}
			EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
			EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
			MapView->autoup = TRUE;
			SetTimer(hwnd, IDT_VIEW_MAP, MapView->autoup_secs*20, (TIMERPROC) NULL);
			return 1;
		case IDC_AUTO_UPDATE_SECS:
			{
				int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
				if (!MapView) 
				{
					SendMessage(hwnd, WM_INITDIALOG, 0, 0);
				}
				if (t != MapView->autoup_secs)
				{
					MapView->autoup_secs = t;
					if (MapView->autoup)
						SetTimer(hwnd, IDT_VIEW_MAP, 
						MapView->autoup_secs*20, (TIMERPROC) NULL);
				}
			}
			return 1;
		case IDC_REFRESH:
			InvalidateRect(hwnd, NULL, FALSE);
			return 1;
		case IDC_BG_SELECT :
			switch(HIWORD(wParam))
			{
			case CBN_SELCHANGE :
			case CBN_CLOSEUP :
				{
					u32 sel= SendMessage(GetDlgItem(hwnd, IDC_BG_SELECT), CB_GETCURSEL, 0, 0);
					switch(sel)
					{
					case 0 :
					case 1 :
					case 2 :
					case 3 :
						MapView->map = sel;
						MapView->lcd = 0;
						break;
					case 4 :
					case 5 :
					case 6 :
					case 7 :
						MapView->map = sel-4;
						MapView->lcd = 1;
						break;
					}
				}
				MapView->clear = true;
				InvalidateRect(hwnd, NULL, FALSE);
				return 1;
			}//switch et case
		}//switch
		return 1;
	}
	return false;
}
