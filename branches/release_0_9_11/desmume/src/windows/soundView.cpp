/*
	Copyright (C) 2009-2015 DeSmuME team

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

#include "soundView.h"

#include <assert.h>
#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>
#include <algorithm>

#include "../common.h"
#include "../NDSSystem.h"
#include "../debug.h"
#include "../matrix.h"
#include "../MMU.h"
#include "../SPU.h"

#include "resource.h"
#include "winutil.h"
#include "main.h"

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

	hDlg = CreateDialogParamW(hAppInst, MAKEINTRESOURCEW(IDD_SOUND_VIEW), hParentWnd, SoundView_DlgProc, (LPARAM)SoundView_Data);
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

static channel_struct oldchanel[16];
static SPU_struct::REGS::CAP oldCap[2];
static s32 volBar[16] = {0};
static u16 oldCtrl = 0xFFFF;

void SoundView_Refresh(bool forceRedraw)
{
	if(SoundView_Data == NULL || SPU_core == NULL)
		return;

	char buf[256];
	HWND hDlg = SoundView_Data->hDlg;
	static const int format_shift[] = { 2, 1, 3, 0 };
	static const u8 volume_shift[] = { 0, 1, 2, 4 };
	static const double ARM7_CLOCK = 33513982;

	for(int chanId = 0; chanId < 8; chanId++) {
		int chan = chanId + chanOfs();
		channel_struct &thischan = SPU_core->channels[chan];
		channel_struct &oldchan = oldchanel[chan];

		if (!forceRedraw && memcmp(&oldchan, &thischan, sizeof(channel_struct)) == 0) continue;

		InvalidateRect(GetDlgItem(hDlg, IDC_SOUND0PANBAR+chanId), NULL, FALSE);
		if(thischan.status != CHANSTAT_STOPPED)
		{
			volBar[chan] = spumuldiv7(128, thischan.vol) >> volume_shift[thischan.volumeDiv];
			InvalidateRect(GetDlgItem(hDlg, IDC_SOUND0VOLBAR+chanId), NULL, FALSE);

			if(SoundView_Data->volModeAlternate) 
				sprintf(buf, "%d/%d", thischan.vol, 1 << volume_shift[thischan.volumeDiv]);
			else
				sprintf(buf, "%d", volBar[chan]);
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
			#define _EMPTY "---"
			volBar[chan] = 0;
			InvalidateRect(GetDlgItem(hDlg, IDC_SOUND0VOLBAR+chanId), NULL, FALSE);
			SetDlgItemText(hDlg, IDC_SOUND0VOL+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0PAN+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0HOLD+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0BUSY+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0REPEATMODE+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0FORMAT+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0SAD+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0PNT+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0TMR+chanId, _EMPTY);
			SetDlgItemText(hDlg, IDC_SOUND0POSLEN+chanId, _EMPTY);
		}
		memcpy(&oldchan, &thischan, sizeof(channel_struct));
	} //chan loop

	//ctrl
	u16 ctrl = _MMU_ARM7_read16(0x04000500);
	if (oldCtrl != ctrl)
	{
		CheckDlgItem(hDlg,IDC_SNDCTRL_ENABLE,SPU_core->regs.masteren!=0);
		CheckDlgItem(hDlg,IDC_SNDCTRL_CH1NOMIX,SPU_core->regs.ctl_ch1bypass!=0);
		CheckDlgItem(hDlg,IDC_SNDCTRL_CH3NOMIX,SPU_core->regs.ctl_ch3bypass!=0);

		sprintf(buf,"%04X",ctrl);
		SetDlgItemText(hDlg,IDC_SNDCTRL_CTRL,buf);

		sprintf(buf,"%04X",SPU_core->regs.soundbias);
		SetDlgItemText(hDlg,IDC_SNDCTRL_BIAS,buf);

		sprintf(buf,"%02X",SPU_core->regs.mastervol);
		SetDlgItemText(hDlg,IDC_SNDCTRL_VOL,buf);

		sprintf(buf,"%01X",SPU_core->regs.ctl_left);
		SetDlgItemText(hDlg,IDC_SNDCTRL_LEFTOUT,buf);

		sprintf(buf,"%01X",SPU_core->regs.ctl_right);
		SetDlgItemText(hDlg,IDC_SNDCTRL_RIGHTOUT,buf);

		static const char* leftouttext[] = {"L-Mix","Ch1","Ch3","Ch1+3"};
		static const char* rightouttext[] = {"R-Mix","Ch1","Ch3","Ch1+3"};

		SetDlgItemText(hDlg,IDC_SNDCTRL_LEFTOUTTEXT,leftouttext[SPU_core->regs.ctl_left]);

		SetDlgItemText(hDlg,IDC_SNDCTRL_RIGHTOUTTEXT,rightouttext[SPU_core->regs.ctl_right]);
		oldCtrl = ctrl;
	}

	//cap0
	SPU_struct::REGS::CAP& cap0 = SPU_core->regs.cap[0];
	if (memcmp(&oldCap[0], &cap0, sizeof(SPU_struct::REGS::CAP)) != 0)
	{
		CheckDlgItem(hDlg,IDC_CAP0_ADD,cap0.add!=0);
		CheckDlgItem(hDlg,IDC_CAP0_SRC,cap0.source!=0);
		CheckDlgItem(hDlg,IDC_CAP0_ONESHOT,cap0.oneshot!=0);
		CheckDlgItem(hDlg,IDC_CAP0_TYPE,cap0.bits8!=0);
		CheckDlgItem(hDlg,IDC_CAP0_ACTIVE,cap0.active!=0);
		CheckDlgItem(hDlg,IDC_CAP0_RUNNING,cap0.runtime.running!=0);

		if(cap0.source) SetDlgItemText(hDlg,IDC_CAP0_SRCTEXT,"Ch2");
		else SetDlgItemText(hDlg,IDC_CAP0_SRCTEXT,"L-Mix");

		if(cap0.bits8) SetDlgItemText(hDlg,IDC_CAP0_TYPETEXT,"Pcm8");
		else SetDlgItemText(hDlg,IDC_CAP0_TYPETEXT,"Pcm16");

		sprintf(buf,"%02X",_MMU_ARM7_read08(0x04000508));
		SetDlgItemText(hDlg,IDC_CAP0_CTRL,buf);

		sprintf(buf,"%08X",cap0.dad);
		SetDlgItemText(hDlg,IDC_CAP0_DAD,buf);

		sprintf(buf,"%08X",cap0.len);
		SetDlgItemText(hDlg,IDC_CAP0_LEN,buf);

		sprintf(buf,"%08X",cap0.runtime.curdad);
		SetDlgItemText(hDlg,IDC_CAP0_CURDAD,buf);

		memcpy(&oldCap[0], &cap0, sizeof(SPU_struct::REGS::CAP));
	}

	//cap1
	SPU_struct::REGS::CAP& cap1 = SPU_core->regs.cap[1];
	if (memcmp(&oldCap[1], &cap1, sizeof(SPU_struct::REGS::CAP)) != 0)
	{
		CheckDlgItem(hDlg,IDC_CAP1_ADD,cap1.add!=0);
		CheckDlgItem(hDlg,IDC_CAP1_SRC,cap1.source!=0);
		CheckDlgItem(hDlg,IDC_CAP1_ONESHOT,cap1.oneshot!=0);
		CheckDlgItem(hDlg,IDC_CAP1_TYPE,cap1.bits8!=0);
		CheckDlgItem(hDlg,IDC_CAP1_ACTIVE,cap1.active!=0);
		CheckDlgItem(hDlg,IDC_CAP1_RUNNING,cap1.runtime.running!=0);

		if(cap1.source) SetDlgItemText(hDlg,IDC_CAP1_SRCTEXT,"Ch3"); //maybe call it "Ch3(+2)" if it fits
		else SetDlgItemText(hDlg,IDC_CAP1_SRCTEXT,"R-Mix");

		if(cap1.bits8) SetDlgItemText(hDlg,IDC_CAP1_TYPETEXT,"Pcm8");
		else SetDlgItemText(hDlg,IDC_CAP1_TYPETEXT,"Pcm16");

		sprintf(buf,"%02X",_MMU_ARM7_read08(0x04000509));
		SetDlgItemText(hDlg,IDC_CAP1_CTRL,buf);

		sprintf(buf,"%08X",cap1.dad);
		SetDlgItemText(hDlg,IDC_CAP1_DAD,buf);

		sprintf(buf,"%08X",cap1.len);
		SetDlgItemText(hDlg,IDC_CAP1_LEN,buf);

		sprintf(buf,"%08X",cap1.runtime.curdad);
		SetDlgItemText(hDlg,IDC_CAP1_CURDAD,buf);

		memcpy(&oldCap[1], &cap1, sizeof(SPU_struct::REGS::CAP));
	}
}

//////////////////////////////////////////////////////////////////////////////


static void updateMute_toSettings(HWND hDlg, int chan)
{
	for(int chanId = 0; chanId < 8; chanId++)
		CommonSettings.spu_muteChannels[chanId+chanOfs()] = IsDlgButtonChecked(hDlg, IDC_SOUND0MUTE+chanId) == BST_CHECKED;
}

static void updateMute_allFromSettings(HWND hDlg)
{
	for(int chanId = 0; chanId < 16; chanId++)
		CheckDlgItem(hDlg,IDC_SOUND0MUTE+chanId,CommonSettings.spu_muteChannels[chanId]);
}

static void updateMute_fromSettings(HWND hDlg)
{
	for(int chanId = 0; chanId < 8; chanId++)
		CheckDlgItem(hDlg,IDC_SOUND0MUTE+chanId,CommonSettings.spu_muteChannels[chanId+chanOfs()]);
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
	CheckDlgItem(hDlg,IDC_SOUND_CAPTURE_MUTED,CommonSettings.spu_captureMuted);

	SoundView_Refresh();
}

static LONG_PTR OldLevelBarProc = 0;
static LONG_PTR OldPanBarProc = 0;
static HBRUSH level_color = CreateSolidBrush(RGB(0, 255, 0));
static HBRUSH level_background = CreateSolidBrush(RGB(90, 90, 90));
static COLORREF pan_color = RGB(255, 0, 0);
static COLORREF pan_center_color = RGB(200, 200, 200);
static HBRUSH pan_background = (HBRUSH)COLOR_WINDOW;

INT_PTR CALLBACK LevelBarProc(HWND hBar, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_PAINT)
	{
		u8 chan = (u8)GetProp(hBar, "chan");
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hBar, &ps);
		u32 vol =  volBar[chan + chanOfs()];
		if (vol > 0)
		{
			RECT rc = {0, 0, vol, ps.rcPaint.bottom};
			FillRect(hdc, &rc, level_color);
		}
		if (vol < 128)
		{
			RECT rc2 = {vol + 1, 0, 128, ps.rcPaint.bottom};
			FillRect(hdc, &rc2, level_background);
		}
		EndPaint(hBar, &ps);
	}

	return CallWindowProc((WNDPROC)OldLevelBarProc, hBar, msg, wParam, lParam);
}

INT_PTR CALLBACK PanBarProc(HWND hBar, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_PAINT)
	{
		u8 chan = (u8)GetProp(hBar, "chan");
		channel_struct &thischan = SPU_core->channels[chan + chanOfs()];
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hBar, &ps);
		FillRect(hdc, &ps.rcPaint, pan_background);
		
		SelectObject(hdc, GetStockObject(DC_PEN));
		if (thischan.pan != 64)
		{
			SetDCPenColor(hdc, pan_center_color);
			MoveToEx(hdc, 64, 0, NULL);
			LineTo(hdc, 64, ps.rcPaint.bottom);
		}
		SetDCPenColor(hdc, pan_color);
		MoveToEx(hdc, thischan.pan, 0, NULL);
		LineTo(hdc, thischan.pan, ps.rcPaint.bottom);
		EndPaint(hBar, &ps);
	}

	return CallWindowProc((WNDPROC)OldPanBarProc, hBar, msg, wParam, lParam);
}

static INT_PTR CALLBACK SoundView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SoundView_DataStruct *data = (SoundView_DataStruct*)GetWindowLongPtr(hDlg, DWLP_USER);
	if((data == NULL) && (uMsg != WM_INITDIALOG))
		return 0;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			for(int chanId = 0; chanId < 8; chanId++) {
				HWND tmp = GetDlgItem(hDlg, IDC_SOUND0VOLBAR+chanId);
				OldLevelBarProc = SetWindowLongPtr(tmp, GWLP_WNDPROC, (LONG_PTR)LevelBarProc);
				SetProp(tmp, "chan", (HANDLE)chanId);

				tmp = GetDlgItem(hDlg, IDC_SOUND0PANBAR+chanId);
				OldPanBarProc = SetWindowLongPtr(tmp, GWLP_WNDPROC, (LONG_PTR)PanBarProc);
				SetProp(tmp, "chan", (HANDLE)chanId);
			}

			for(int chanId = 0; chanId < 8; chanId++) {
				if(CommonSettings.spu_muteChannels[chanId])
					SendDlgItemMessage(hDlg, IDC_SOUND0MUTE+chanId, BM_SETCHECK, TRUE, 0);
			}

			if(data == NULL)
			{
				data = (SoundView_DataStruct*)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)data);
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
		case IDC_SOUND_CAPTURE_MUTED:
			CommonSettings.spu_captureMuted = IsDlgButtonChecked(hDlg,IDC_SOUND_CAPTURE_MUTED) != 0;
			return 1;
		case IDC_SOUND_UNMUTE_ALL:
			for(int i=0;i<16;i++) CommonSettings.spu_muteChannels[i] = false;
			updateMute_allFromSettings(hDlg);
			return 1;
		case IDC_SOUND_ANALYZE_CAP:
			printf("WTF\n");
			for(int i=0;i<16;i++) CommonSettings.spu_muteChannels[i] = true;
			CommonSettings.spu_muteChannels[1] = false;
			CommonSettings.spu_muteChannels[3] = false;
			CommonSettings.spu_captureMuted = true;
			updateMute_allFromSettings(hDlg);
			CheckDlgItem(hDlg,IDC_SOUND_CAPTURE_MUTED,CommonSettings.spu_captureMuted);
			return 1;


		case IDC_SOUNDVIEW_CHANSWITCH:
			{
				SoundView_SwitchChanOfs(data);
				SoundView_Refresh(true);
			}
			return 1;
		}
		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
