/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2016 DeSmuME team

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

#include <commctrl.h>

#include "GPU.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "debug.h"

#include "resource.h"
#include "main.h"
#include "windriver.h"


struct mapview_struct
{
	u32	autoup_secs;
	bool autoup;

	GPULayerID layerID;
	GPUEngineID engineID;
	u16 bitmap[1024*1024];
	bool clear;

	void render()
	{
		GPUEngineBase *gpu = (engineID == GPUEngineID_Main) ? (GPUEngineBase *)GPU->GetEngineMain() : (GPUEngineBase *)GPU->GetEngineSub();
		memset(bitmap, 0, sizeof(bitmap));
		
		gpu->RenderLayerBG(layerID, bitmap);
	}
};

mapview_struct	*MapView = NULL;


LRESULT MapView_OnPaint(mapview_struct * win, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	Lock lock;

	HDC          hdc;
	PAINTSTRUCT  ps;
	char text[80];
	u32 dispcnt = ((volatile u32 *)MMU.ARM9_REG)[(win->engineID*0x400)];
	const BGLayerInfo &BGLayer = (win->engineID == GPUEngineID_Main) ? GPU->GetEngineMain()->GetBGLayerInfoByID(win->layerID) : GPU->GetEngineSub()->GetBGLayerInfoByID(win->layerID);
	BITMAPV4HEADER bmi;
	u16 lg = BGLayer.size.width;
	u16 ht = BGLayer.size.height;
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
	bmi.bV4Width = lg;
	bmi.bV4Height = -ht;

	hdc = BeginPaint(hwnd, &ps);        

	sprintf(text, "%d %08X, %08X", (int)(dispcnt&7), (int)dispcnt, (int)BGLayer.BGnCNT.value);
	SetWindowText(GetDlgItem(hwnd, IDC_MODE), text);

	if(BGLayer.BGnCNT.PaletteMode == 0)
		sprintf(text, "normal 16");
	else
	{
		if(!(dispcnt&(1<<30)))
			sprintf(text, "normal 256");
		else
		{
			sprintf(text, "extended slot %d", BGLayer.extPaletteSlot);
		}
	}
	SetWindowText(GetDlgItem(hwnd, IDC_PAL), text);

	sprintf(text, "%d", (int)(BGLayer.priority));
	SetWindowText(GetDlgItem(hwnd, IDC_PRIO), text);

	if(BGLayer.isVisible)
		SetWindowText(GetDlgItem(hwnd, IDC_VISIBLE), "true");
	else
		SetWindowText(GetDlgItem(hwnd, IDC_VISIBLE), "false");

	sprintf(text, "0x%08X", (int)(0x6000000 + (BGLayer.BGnCNT.CharacBase_Block)*0x4000 + win->engineID*0x200000 +((dispcnt>>24)&7)*0x10000));
	SetWindowText(GetDlgItem(hwnd, IDC_CHAR), text);

	sprintf(text, "0x%08X", (int)(0x6000000 + 0x800*(BGLayer.BGnCNT.ScreenBase_Block) + win->engineID*0x200000 + ((dispcnt>>27)&7)*0x10000));
	SetWindowText(GetDlgItem(hwnd, IDC_SCR), text);

	//sprintf(text, "%d x %d",  GPU->GetEngineMain()->BGPA[win->map], GPU->GetEngineMain()->BGPB[win->map]);
	sprintf(text, "%d x %d",  (int)lg, (int)ht);
	SetWindowText(GetDlgItem(hwnd, IDC_MSIZE), text);

	//if (win->map==2) {
	//	parms = &(GPU->GetEngineMain()->dispx_st)->dispx_BG2PARMS;
	//} else {
	//	parms = &(GPU->GetEngineMain()->dispx_st)->dispx_BG3PARMS;
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
			MapView->layerID = GPULayerID_BG0;
			MapView->engineID = GPUEngineID_Main;
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
						MapView->layerID = (GPULayerID)sel;
						MapView->engineID = GPUEngineID_Main;
						break;
					case 4 :
					case 5 :
					case 6 :
					case 7 :
						MapView->layerID = (GPULayerID)(sel - 4);
						MapView->engineID = GPUEngineID_Sub;
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
