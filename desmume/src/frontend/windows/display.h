/*
Copyright (C) 2018-2025 DeSmuME team

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

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "main.h"
#include "NDSSystem.h"
#include "debug.h"
#include "GPU.h"
#include "resource.h"
#include "lua-engine.h"
#include "video.h"
#include "frontend/modules/osd/agg/agg_osd.h"
#include "rthreads/rthreads.h"
#include "ddraw.h"
#include "ogl_display.h"

#define WIN32_FRAMEBUFFER_COUNT 3
#define WIN32_MAX_FRAME_WAIT_TIME 333 // Maximum time in milliseconds to wait for a video frame to render to a window before starting a new frame.


class Win32GPUEventHandler : public GPUEventHandlerDefault
{
protected:
	NDSColorFormat _colorFormatPending;
	size_t _widthPending;
	size_t _heightPending;
	size_t _pageCountPending;
	size_t _scaleFactorIntegerPending;

	bool _didColorFormatChange;
	bool _didWidthChange;
	bool _didHeightChange;
	bool _didPageCountChange;

	u8 _inProcessBufferIndex;
	u8 _latestAvailableBufferIndex;
	u8 _currentLockedPageIndex;

	slock_t *_mutexApplyGPUSettings;
	slock_t *_mutexVideoFetch;
	slock_t *_mutexFramebufferPage[WIN32_FRAMEBUFFER_COUNT];

public:
	Win32GPUEventHandler();
	~Win32GPUEventHandler();

	void SetFramebufferDimensions(size_t w, size_t h);
	void GetFramebufferDimensions(size_t& outWidth, size_t& outHeight);
	void SetFramebufferDimensionsByScaleFactorInteger(size_t scaleFactor);
	size_t GetFramebufferDimensionsByScaleFactorInteger();

	void SetColorFormat(NDSColorFormat colorFormat);
	NDSColorFormat GetColorFormat();

	void VideoFetchLock();
	void VideoFetchUnlock();

	// GPUEventHandler methods
	virtual void DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8& selectedBufferIndexInOut);
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo& latestDisplayInfo);
	virtual void DidApplyGPUSettingsBegin();
	virtual void DidApplyGPUSettingsEnd();
};

extern DDRAW ddraw;
extern GLDISPLAY gldisplay;
extern u32 displayMethod;
const u32 DISPMETHOD_DDRAW_HW = 1;
const u32 DISPMETHOD_DDRAW_SW = 2;
const u32 DISPMETHOD_OPENGL = 3;

extern Win32GPUEventHandler* WinGPUEvent;
extern int gpu_bpp;

extern int emu_paused;
extern bool frameAdvance;
extern int frameskiprate;
extern int lastskiprate;
extern VideoInfo video;

extern RECT FullScreenRect, MainScreenRect, SubScreenRect, GapRect;
const int kGapNone = 0;
const int kGapBorder = 5;
const int kGapNDS = 64; // extremely tilted (but some games seem to use this value)
const int kGapNDS2 = 90; // more normal viewing angle
const int kScale1point5x = 65535;
const int kScale2point5x = 65534;

extern bool ForceRatio;
extern bool PadToInteger;
extern float screenSizeRatio;
extern bool vCenterResizedScr;

extern bool SeparationBorderDrag;
extern int ScreenGapColor;

extern HANDLE display_wakeup_event;
extern HANDLE display_done_event;

extern int displayPostponeType;
extern DWORD displayPostponeUntil;
extern bool displayNoPostponeNext;

extern void(*display_invoke_function)(DWORD);
extern HANDLE display_invoke_ready_event;
extern HANDLE display_invoke_done_event;
extern DWORD display_invoke_timeout;
extern CRITICAL_SECTION display_invoke_handler_cs;

// Scanline filter parameters. The first set is from commandline.cpp, the second
// set is externed to scanline.cpp.
// TODO: When videofilter.cpp becomes Windows-friendly, remove the second set of
// variables, as they will no longer be necessary at that point.
extern int _scanline_filter_a;
extern int _scanline_filter_b;
extern int _scanline_filter_c;
extern int _scanline_filter_d;
extern int scanline_filter_a;
extern int scanline_filter_b;
extern int scanline_filter_c;
extern int scanline_filter_d;

void Display();
void DoDisplay();
void KillDisplay();
void SetDisplayNeedsBufferUpdates();

void GetNdsScreenRect(RECT* r);

void _ServiceDisplayThreadInvocation();
FORCEINLINE void ServiceDisplayThreadInvocations()
{
	if (display_invoke_function)
		_ServiceDisplayThreadInvocation();
}

void UpdateScreenRects();
void UpdateWndRects(HWND hwnd, RECT* newClientRect = NULL);
void TwiddleLayer(UINT ctlid, int core, int layer);
void SetLayerMasks(int mainEngineMask, int subEngineMask);

#endif