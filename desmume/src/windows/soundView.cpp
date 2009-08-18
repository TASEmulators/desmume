/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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


#include "../SPU.h"
#include "../debug.h"
#include "../common.h"
#include "../matrix.h"
#include "resource.h"
#include "NDSSystem.h"
#include <algorithm>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "soundView.h"

#include <assert.h>

using namespace std;

//////////////////////////////////////////////////////////////////////////////

typedef struct SoundView_DataStruct
{
	SoundView_DataStruct() 
		: viewFirst8Channels(TRUE)
		, volModeAlternate(FALSE)
	{
	}

	HWND hDlg;

	BOOL viewFirst8Channels;
	BOOL volModeAlternate;
} SoundView_DataStruct;

SoundView_DataStruct * SoundView_Data = NULL;

//////////////////////////////////////////////////////////////////////////////

BOOL SoundView_Init()
{
	return 1;
}

void SoundView_DeInit()
{
}

//////////////////////////////////////////////////////////////////////////////

inline int chanOfs()
{
	return SoundView_Data->viewFirst8Channels ? 0 : 8;
}

BOOL SoundView_DlgOpen(HWND hParentWnd)
{
	HWND hDlg;

	SoundView_Data = new SoundView_DataStruct();
	if(SoundView_Data == NULL)
		return 0;

	hDlg = CreateDialogParam(hAppInst, MAKEINTRESOURCE(IDD_SOUND_VIEW), hParentWnd, SoundView_DlgProc, (LPARAM)SoundView_Data);
	if(hDlg == NULL)
	{
		delete SoundView_Data;
		SoundView_Data = NULL;
		return 0;
	}

	//SoundView_Data->hDlg = hDlg;

	ShowWindow(hDlg, SW_SHOW);
	UpdateWindow(hDlg);

	return 1;
}

void SoundView_DlgClose()
{
	if(SoundView_Data != NULL)
	{
		DestroyWindow(SoundView_Data->hDlg);
		delete SoundView_Data;
		SoundView_Data = NULL;
	}
}

BOOL SoundView_IsOpened()
{
	return (SoundView_Data != NULL);
}

HWND SoundView_GetHWnd()
{
	return SoundView_Data ? SoundView_Data->hDlg : NULL;
}

void SoundView_Refresh()
{
	if(SoundView_Data == NULL || SPU_core == NULL)
		return;

	char buf[256];
	HWND hDlg = SoundView_Data->hDlg;
	static const int format_shift[] = { 2, 1, 3, 0 };
	static const double ARM7_CLOCK = 33513982;
	for(int chanId = 0; chanId < 8; chanId++) {
		int chan = chanId + chanOfs();
		channel_struct &thischan = SPU_core->channels[chan];

		SendDlgItemMessage(hDlg, IDC_SOUND0PANBAR+chanId, PBM_SETPOS, (WPARAM)spumuldiv7(128, thischan.pan), (LPARAM)0);
		if(thischan.status != CHANSTAT_STOPPED)
		{
			s32 vol = spumuldiv7(128, thischan.vol) >> thischan.datashift;
			SendDlgItemMessage(hDlg, IDC_SOUND0VOLBAR+chanId, PBM_SETPOS,
				(WPARAM)vol, (LPARAM)0);

			if(SoundView_Data->volModeAlternate) 
				sprintf(buf, "%d/%d", thischan.vol, 1 << thischan.datashift);
			else
				sprintf(buf, "%d", vol);
			SetDlgItemText(hDlg, IDC_SOUND0VOL+chanId, buf);

			if (thischan.pan == 0)
				strcpy(buf, "L");
			else if (thischan.pan == 64)
				strcpy(buf, "C");
			else if (thischan.pan == 127)
				strcpy(buf, "R");
			else if (thischan.pan < 64)
				sprintf(buf, "L%d", 64 - thischan.pan);
			else //if (thischan.pan > 64)
				sprintf(buf, "R%d", thischan.pan - 64);
			SetDlgItemText(hDlg, IDC_SOUND0PAN+chanId, buf);

			sprintf(buf, "%d", thischan.hold);
			SetDlgItemText(hDlg, IDC_SOUND0HOLD+chanId, buf);

			sprintf(buf, "%d", thischan.status);
			SetDlgItemText(hDlg, IDC_SOUND0BUSY+chanId, buf);

			const char* modes[] = { "Manual", "Loop Infinite", "One-Shot", "Prohibited" };
			sprintf(buf, "%d (%s)", thischan.repeat, modes[thischan.repeat]);
			SetDlgItemText(hDlg, IDC_SOUND0REPEATMODE+chanId, buf);

			if(thischan.format != 3) {
				const char* formats[] = { "PCM8", "PCM16", "IMA-ADPCM" };
				sprintf(buf, "%d (%s)", thischan.format, formats[thischan.format]);
				SetDlgItemText(hDlg, IDC_SOUND0FORMAT+chanId, buf);
			}
			else {
				if (chan < 8)
					sprintf(buf, "%d (PSG/Noise?)", thischan.format);
				else if (chan < 14)
					sprintf(buf, "%d (%.1f% Square)", thischan.format, (float)thischan.waveduty/8);
				else
					sprintf(buf, "%d (Noise)", thischan.format);
			}

			sprintf(buf, "$%07X", thischan.addr);
			SetDlgItemText(hDlg, IDC_SOUND0SAD+chanId, buf);

			sprintf(buf, "samp #%d", thischan.loopstart << format_shift[thischan.format]);
			SetDlgItemText(hDlg, IDC_SOUND0PNT+chanId, buf);

			sprintf(buf, "$%04X (%.1f Hz)", thischan.timer, (ARM7_CLOCK/2) / (double)(0x10000 - thischan.timer));
			SetDlgItemText(hDlg, IDC_SOUND0TMR+chanId, buf);

			sprintf(buf, "samp #%d / #%d", sputrunc(thischan.sampcnt), thischan.totlength << format_shift[thischan.format]);
			SetDlgItemText(hDlg, IDC_SOUND0POSLEN+chanId, buf);
		}
		else {
			SendDlgItemMessage(hDlg, IDC_SOUND0VOLBAR+chanId, PBM_SETPOS, (WPARAM)0, (LPARAM)0);
			strcpy(buf, "---");
			SetDlgItemText(hDlg, IDC_SOUND0VOL+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0PAN+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0HOLD+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0BUSY+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0REPEATMODE+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0FORMAT+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0SAD+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0PNT+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0TMR+chanId, buf);
			SetDlgItemText(hDlg, IDC_SOUND0POSLEN+chanId, buf);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////


static void updateMute_toSettings(HWND hDlg, int chan)
{
	for(int chanId = 0; chanId < 8; chanId++)
		CommonSettings.spu_muteChannels[chanId+chanOfs()] = IsDlgButtonChecked(hDlg, IDC_SOUND0MUTE+chanId) == BST_CHECKED;
}

static void updateMute_fromSettings(HWND hDlg)
{
	for(int chanId = 0; chanId < 8; chanId++)
			SendDlgItemMessage(hDlg, IDC_SOUND0MUTE+chanId, BM_SETCHECK, 
				CommonSettings.spu_muteChannels[chanId+chanOfs()] ? TRUE : FALSE,
				0);
}
static void SoundView_SwitchChanOfs(SoundView_DataStruct *data)
{
	if (data == NULL)
		return;

	HWND hDlg = data->hDlg;
	data->viewFirst8Channels = !data->viewFirst8Channels;
	SetWindowText(GetDlgItem(hDlg, IDC_SOUNDVIEW_CHANSWITCH),
		data->viewFirst8Channels ? "V" : "^");

	char buf[256];
	for(int chanId = 0; chanId < 8; chanId++) {
		int chan = chanId + chanOfs();
		sprintf(buf, "#%02d", chan);
		SetDlgItemText(hDlg, IDC_SOUND0ID+chanId, buf);
	}

	updateMute_fromSettings(hDlg);

	SoundView_Refresh();
}


static INT_PTR CALLBACK SoundView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG_PTR x;
	SoundView_DataStruct *data = (SoundView_DataStruct*)GetWindowLongPtr(hDlg, DWLP_USER);
	if((data == NULL) && (uMsg != WM_INITDIALOG))
		return 0;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			for(int chanId = 0; chanId < 8; chanId++) {
				SendDlgItemMessage(hDlg, IDC_SOUND0VOLBAR+chanId, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 128));
				SendDlgItemMessage(hDlg, IDC_SOUND0PANBAR+chanId, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 128));
			}

			for(int chanId = 0; chanId < 8; chanId++) {
				if(CommonSettings.spu_muteChannels[chanId])
					SendDlgItemMessage(hDlg, IDC_SOUND0MUTE+chanId, BM_SETCHECK, TRUE, 0);
			}

			if(data == NULL)
			{
				data = (SoundView_DataStruct*)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)data);
			}
			data->hDlg = hDlg;

			data->viewFirst8Channels = !data->viewFirst8Channels;
			SoundView_SwitchChanOfs(data);
			//SoundView_Refresh();
			//InvalidateRect(hDlg, NULL, FALSE); UpdateWindow(hDlg);
		}
		return 1;

	case WM_CLOSE:
	case WM_DESTROY:
		SoundView_DlgClose();
		return 1;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			SoundView_DlgClose();
			return 1;
		case IDC_BUTTON_VOLMODE:
			data->volModeAlternate = IsDlgButtonChecked(hDlg, IDC_BUTTON_VOLMODE);
			return 1;

		case IDC_SOUND0MUTE+0:
		case IDC_SOUND0MUTE+1:
		case IDC_SOUND0MUTE+2:
		case IDC_SOUND0MUTE+3:
		case IDC_SOUND0MUTE+4:
		case IDC_SOUND0MUTE+5:
		case IDC_SOUND0MUTE+6:
		case IDC_SOUND0MUTE+7:
			updateMute_toSettings(hDlg,LOWORD(wParam)-IDC_SOUND0MUTE);
			return 1;


		case IDC_SOUNDVIEW_CHANSWITCH:
			{
				SoundView_SwitchChanOfs(data);
			}
			return 1;
		}
		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
