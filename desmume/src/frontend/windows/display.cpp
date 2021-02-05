/*
Copyright (C) 2018 DeSmuME team

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

#include "display.h"

#include <MMSystem.h>
#include <WindowsX.h>

#include "main.h"
#include "windriver.h"
#include "winutil.h"

#include <cmath>

DDRAW ddraw;
GLDISPLAY gldisplay;
u32 displayMethod;

VideoInfo video;

int gpu_bpp = 18;


int frameskiprate = 0;
int lastskiprate = 0;
int emu_paused = 0;
bool frameAdvance = false;

bool SeparationBorderDrag = true;
int ScreenGapColor = 0xFFFFFF;
/*const u32 gapColors[16] = {
Color::Gray,
Color::Brown,
Color::Red,
Color::Pink,
Color::Orange,
Color::Yellow,
Color::LightGreen,
Color::Lime,
Color::Green,
Color::SeaGreen,
Color::SkyBlue,
Color::Blue,
Color::DarkBlue,
Color::DarkViolet,
Color::Purple,
Color::Fuchsia
};*/

RECT FullScreenRect, MainScreenRect, SubScreenRect, GapRect;
RECT MainScreenSrcRect, SubScreenSrcRect;

bool ForceRatio = true;
bool PadToInteger = false;
float screenSizeRatio = 1.0f;
bool vCenterResizedScr = true;

//triple buffering logic
struct DisplayBuffer
{
	DisplayBuffer()
		: buffer(NULL)
		, size(0)
	{
	}
	u32* buffer;
	size_t size; //[256*192*4];
} displayBuffers[3];

volatile int currDisplayBuffer = -1;
volatile int newestDisplayBuffer = -2;
slock_t *display_mutex = NULL;
sthread_t *display_thread = NULL;
volatile bool display_die = false;
HANDLE display_wakeup_event = INVALID_HANDLE_VALUE;
HANDLE display_done_event = INVALID_HANDLE_VALUE;
DWORD display_done_timeout = 500;

int displayPostponeType = 0;
DWORD displayPostponeUntil = ~0;
bool displayNoPostponeNext = false;

DWORD display_invoke_argument = 0;
void(*display_invoke_function)(DWORD) = NULL;
HANDLE display_invoke_ready_event = INVALID_HANDLE_VALUE;
HANDLE display_invoke_done_event = INVALID_HANDLE_VALUE;
DWORD display_invoke_timeout = 500;
CRITICAL_SECTION display_invoke_handler_cs;
static void InvokeOnMainThread(void(*function)(DWORD), DWORD argument);

int scanline_filter_a = 0;
int scanline_filter_b = 2;
int scanline_filter_c = 2;
int scanline_filter_d = 4;

#pragma pack(push,1)
struct pix24
{
	u8 b, g, r;
	FORCEINLINE pix24(u32 s) : b(s & 0xFF), g((s & 0xFF00) >> 8), r((s & 0xFF0000) >> 16) {}
};
struct pix16
{
	u16 c;
	FORCEINLINE pix16(u32 s) : c(((s & 0xF8) >> 3) | ((s & 0xFC00) >> 5) | ((s & 0xF80000) >> 8)) {}
};
struct pix15
{
	u16 c;
	FORCEINLINE pix15(u32 s) : c(((s & 0xF8) >> 3) | ((s & 0xF800) >> 6) | ((s & 0xF80000) >> 9)) {}
};
#pragma pack(pop)

void GetNdsScreenRect(RECT* r)
{
	RECT zero;
	SetRect(&zero, 0, 0, 0, 0);
	if (zero == MainScreenRect) *r = SubScreenRect;
	else if (zero == SubScreenRect || video.layout == 2) *r = MainScreenRect;
	else
	{
		RECT* dstRects[2] = { &MainScreenRect, &SubScreenRect };
		int left = min(dstRects[0]->left, dstRects[1]->left);
		int top = min(dstRects[0]->top, dstRects[1]->top);
		int right = max(dstRects[0]->right, dstRects[1]->right);
		int bottom = max(dstRects[0]->bottom, dstRects[1]->bottom);
		SetRect(r, left, top, right, bottom);
	}
}

struct DisplayLayoutInfo
{
	int vx, vy, vw, vh;
	float widthScale, heightScale;
	int bufferWidth, bufferHeight;
};
//performs aspect ratio letterboxing correction and integer clamping
DisplayLayoutInfo CalculateDisplayLayout(RECT rcClient, bool maintainAspect, bool maintainInteger, int targetWidth, int targetHeight)
{
	DisplayLayoutInfo ret;

	//do maths on the viewport and the native resolution and the user settings to get a display rectangle
	SIZE sz = { rcClient.right - rcClient.left, rcClient.bottom - rcClient.top };

	float widthScale = (float)sz.cx / targetWidth;
	float heightScale = (float)sz.cy / targetHeight;
	if (maintainAspect)
	{
		if (widthScale > heightScale) widthScale = heightScale;
		if (heightScale > widthScale) heightScale = widthScale;
	}
	if (maintainInteger)
	{
		widthScale = floorf(widthScale);
		heightScale = floorf(heightScale);
	}
	ret.vw = (int)(widthScale * targetWidth);
	ret.vh = (int)(heightScale * targetHeight);
	ret.vx = (sz.cx - ret.vw) / 2;
	ret.vy = (sz.cy - ret.vh) / 2;
	ret.widthScale = widthScale;
	ret.heightScale = heightScale;
	ret.bufferWidth = sz.cx;
	ret.bufferHeight = sz.cy;

	return ret;
}
//reformulates CalculateDisplayLayout() into a format more convenient for this purpose
RECT CalculateDisplayLayoutWrapper(RECT rcClient, int targetWidth, int targetHeight, int tbHeight, bool maximized)
{
	bool maintainInteger = !!PadToInteger;
	bool maintainAspect = !!ForceRatio;

	if (maintainInteger) maintainAspect = true;

	//nothing to do here if maintain aspect isnt chosen
	if (!maintainAspect) return rcClient;

	RECT rc = { rcClient.left, rcClient.top + tbHeight, rcClient.right, rcClient.bottom };
	DisplayLayoutInfo dli = CalculateDisplayLayout(rc, maintainAspect, maintainInteger, targetWidth, targetHeight);
	rc.left = rcClient.left + dli.vx;
	rc.top = rcClient.top + dli.vy;
	rc.right = rc.left + dli.vw;
	rc.bottom = rc.top + dli.vh + tbHeight;
	return rc;
}

template<typename T> static void doRotate(void* dst)
{
	u8* buffer = (u8*)dst;
	int size = video.size();
	u32* src = (u32*)video.finalBuffer();
	int width = video.width;
	int height = video.height;
	int pitch = ddraw.surfDescBack.lPitch;

	switch (video.rotation)
	{
	case 0:
	case 180:
	{
		if (pitch == 1024)
		{
			if (video.rotation == 0)
				if (sizeof(T) == sizeof(u32))
					memcpy(buffer, src, size * sizeof(u32));
				else
					for (int i = 0; i < size; i++)
						((T*)buffer)[i] = src[i];
			else // if(video.rotation==180)
				for (int i = 0, j = size - 1; j >= 0; i++, j--)
					((T*)buffer)[i] = src[j];
		}
		else
		{
			if (video.rotation == 0)
				if (sizeof(T) != sizeof(u32))
					for (int y = 0; y < height; y++)
					{
						for (int x = 0; x < width; x++)
							((T*)buffer)[x] = src[(y * width) + x];

						buffer += pitch;
					}
				else
					for (int y = 0; y < height; y++)
					{
						memcpy(buffer, &src[y * width], width * sizeof(u32));
						buffer += pitch;
					}
			else // if(video.rotation==180)
				for (int y = 0; y < height; y++)
				{
					for (int x = 0; x < width; x++)
						((T*)buffer)[x] = src[height*width - (y * width) - x - 1];

					buffer += pitch;
				}
		}
	}
	break;
	case 90:
	case 270:
	{
		if (video.rotation == 90)
			for (int y = 0; y < width; y++)
			{
				for (int x = 0; x < height; x++)
					((T*)buffer)[x] = src[(((height - 1) - x) * width) + y];

				buffer += pitch;
			}
		else
			for (int y = 0; y < width; y++)
			{
				for (int x = 0; x < height; x++)
					((T*)buffer)[x] = src[((x)* width) + (width - 1) - y];

				buffer += pitch;
			}
	}
	break;
	}
}
static void DD_FillRect(LPDIRECTDRAWSURFACE7 surf, int left, int top, int right, int bottom, DWORD color)
{
	RECT r;
	SetRect(&r, left, top, right, bottom);
	DDBLTFX fx;
	memset(&fx, 0, sizeof(DDBLTFX));
	fx.dwSize = sizeof(DDBLTFX);
	//fx.dwFillColor = color;
	fx.dwFillColor = 0; //color is just for debug
	surf->Blt(&r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
}

static void OGL_DrawTexture(RECT* srcRects, RECT* dstRects)
{
	//don't change the original rects
	RECT sRects[2];
	sRects[0] = RECT(srcRects[0]);
	sRects[1] = RECT(srcRects[1]);

	//use clear+scissor for gap
	if (video.screengap > 0)
	{
		//adjust client rect into scissor rect (0,0 at bottomleft)
		glScissor(dstRects[2].left, dstRects[2].bottom, dstRects[2].right - dstRects[2].left, dstRects[2].top - dstRects[2].bottom);

		u32 color_rev = (u32)ScreenGapColor;
		int r = (color_rev >> 0) & 0xFF;
		int g = (color_rev >> 8) & 0xFF;
		int b = (color_rev >> 16) & 0xFF;
		glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
		glEnable(GL_SCISSOR_TEST);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	//draw two screens
	glBegin(GL_QUADS);
	for (int i = 0; i<2; i++)
	{
		//none of this makes any goddamn sense. dont even try.
		int idx = i;
		int ofs = 0;
		switch (video.rotation)
		{
		case 0:
			break;
		case 90:
			ofs = 3;
			idx = 1 - i;
			std::swap(sRects[idx].right, sRects[idx].bottom);
			std::swap(sRects[idx].left, sRects[idx].top);
			break;
		case 180:
			idx = 1 - i;
			ofs = 2;
			break;
		case 270:
			std::swap(sRects[idx].right, sRects[idx].bottom);
			std::swap(sRects[idx].left, sRects[idx].top);
			ofs = 1;
			break;
		}
		float u1 = sRects[idx].left / (float)video.width;
		float u2 = sRects[idx].right / (float)video.width;
		float v1 = sRects[idx].top / (float)video.height;
		float v2 = sRects[idx].bottom / (float)video.height;
		float u[] = { u1,u2,u2,u1 };
		float v[] = { v1,v1,v2,v2 };

		glTexCoord2f(u[(ofs + 0) % 4], v[(ofs + 0) % 4]);
		glVertex2i(dstRects[i].left, dstRects[i].top);

		glTexCoord2f(u[(ofs + 1) % 4], v[(ofs + 1) % 4]);
		glVertex2i(dstRects[i].right, dstRects[i].top);

		glTexCoord2f(u[(ofs + 2) % 4], v[(ofs + 2) % 4]);
		glVertex2i(dstRects[i].right, dstRects[i].bottom);

		glTexCoord2f(u[(ofs + 3) % 4], v[(ofs + 3) % 4]);
		glVertex2i(dstRects[i].left, dstRects[i].bottom);
	}
	glEnd();
}
static void OGL_DoDisplay()
{
	if (!gldisplay.begin(MainWindow->getHWnd())) return;

	//the ds screen fills the texture entirely, so we dont have garbage at edge to worry about,
	//but we need to make sure this is clamped for when filtering is selected
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (gldisplay.filter)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	RECT rc;
	HWND hwnd = MainWindow->getHWnd();
	GetClientRect(hwnd, &rc);
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, (float)width, (float)height, 0.0f, -100.0f, 100.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//clear entire area, for cases where the screen is maximized
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	// get dest and src rects
	RECT dr[] = { MainScreenRect, SubScreenRect, GapRect };
	for (int i = 0; i<2; i++) //dont change gap rect, for some reason
	{
		ScreenToClient(hwnd, (LPPOINT)&dr[i].left);
		ScreenToClient(hwnd, (LPPOINT)&dr[i].right);
	}
	dr[2].bottom = height - dr[2].bottom;
	dr[2].top = height - dr[2].top;

	RECT srcRects[2];
	const bool isMainGPUFirst = (GPU->GetDisplayInfo().engineID[NDSDisplayID_Main] == GPUEngineID_Main);

	if (video.swap == 0)
	{
		srcRects[0] = MainScreenSrcRect;
		srcRects[1] = SubScreenSrcRect;
		if (osd) osd->swapScreens = false;
	}
	else if (video.swap == 1)
	{
		srcRects[0] = SubScreenSrcRect;
		srcRects[1] = MainScreenSrcRect;
		if (osd) osd->swapScreens = true;
	}
	else if (video.swap == 2)
	{
		srcRects[0] = (!isMainGPUFirst) ? SubScreenSrcRect : MainScreenSrcRect;
		srcRects[1] = (!isMainGPUFirst) ? MainScreenSrcRect : SubScreenSrcRect;
		if (osd) osd->swapScreens = !isMainGPUFirst;
	}
	else if (video.swap == 3)
	{
		srcRects[0] = (!isMainGPUFirst) ? MainScreenSrcRect : SubScreenSrcRect;
		srcRects[1] = (!isMainGPUFirst) ? SubScreenSrcRect : MainScreenSrcRect;
		if (osd) osd->swapScreens = isMainGPUFirst;
	}

	//printf("%d,%d %dx%d  -- %d,%d %dx%d\n",
	//	srcRects[0].left,srcRects[0].top, srcRects[0].right-srcRects[0].left, srcRects[0].bottom-srcRects[0].top,
	//	srcRects[1].left,srcRects[1].top, srcRects[1].right-srcRects[1].left, srcRects[1].bottom-srcRects[1].top
	//	);

	glEnable(GL_TEXTURE_2D);
	// draw DS display
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, video.width, video.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, video.finalBuffer());
	OGL_DrawTexture(srcRects, dr);

	// draw HUD
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2, 0, GL_BGRA, GL_UNSIGNED_BYTE, aggDraw.hud->buf().buf());
	OGL_DrawTexture(srcRects, dr);
	glDisable(GL_BLEND);

	gldisplay.showPage();

	gldisplay.end();
}
static void DD_DoDisplay()
{
	if (ddraw.surfDescBack.dwWidth != video.rotatedwidth() || ddraw.surfDescBack.dwHeight != video.rotatedheight())
		ddraw.createBackSurface(video.rotatedwidth(), video.rotatedheight());

	if (!ddraw.lock()) return;
	char* buffer = (char*)ddraw.surfDescBack.lpSurface;

	switch (ddraw.surfDescBack.ddpfPixelFormat.dwRGBBitCount)
	{
	case 32:
		doRotate<u32>(ddraw.surfDescBack.lpSurface);
		break;
	case 24:
		doRotate<pix24>(ddraw.surfDescBack.lpSurface);
		break;
	case 16:
		if (ddraw.surfDescBack.ddpfPixelFormat.dwGBitMask != 0x3E0)
			doRotate<pix16>(ddraw.surfDescBack.lpSurface);
		else
			doRotate<pix15>(ddraw.surfDescBack.lpSurface);
		break;
	case 0:
		break;
	default:
		INFO("Unsupported color depth: %i bpp\n", ddraw.surfDescBack.ddpfPixelFormat.dwRGBBitCount);
		//emu_halt();
		break;
	}
	if (!ddraw.unlock()) return;

	RECT* dstRects[2] = { &MainScreenRect, &SubScreenRect };
	RECT* srcRects[2];
	const bool isMainGPUFirst = (GPU->GetDisplayInfo().engineID[NDSDisplayID_Main] == GPUEngineID_Main);

	if (video.swap == 0)
	{
		srcRects[0] = &MainScreenSrcRect;
		srcRects[1] = &SubScreenSrcRect;
		if (osd) osd->swapScreens = false;
	}
	else if (video.swap == 1)
	{
		srcRects[0] = &SubScreenSrcRect;
		srcRects[1] = &MainScreenSrcRect;
		if (osd) osd->swapScreens = true;
	}
	else if (video.swap == 2)
	{
		srcRects[0] = (!isMainGPUFirst) ? &SubScreenSrcRect : &MainScreenSrcRect;
		srcRects[1] = (!isMainGPUFirst) ? &MainScreenSrcRect : &SubScreenSrcRect;
		if (osd) osd->swapScreens = !isMainGPUFirst;
	}
	else if (video.swap == 3)
	{
		srcRects[0] = (!isMainGPUFirst) ? &MainScreenSrcRect : &SubScreenSrcRect;
		srcRects[1] = (!isMainGPUFirst) ? &SubScreenSrcRect : &MainScreenSrcRect;
		if (osd) osd->swapScreens = isMainGPUFirst;
	}

	//this code fills in all the undrawn areas
	//it is probably a waste of time to keep black-filling all this, but we need to do it to be safe.
	{
		RECT wr;
		GetWindowRect(MainWindow->getHWnd(), &wr);
		RECT r;
		GetNdsScreenRect(&r);
		int left = r.left;
		int top = r.top;
		int right = r.right;
		int bottom = r.bottom;
		//printf("%d %d %d %d / %d %d %d %d\n",fullScreen.left,fullScreen.top,fullScreen.right,fullScreen.bottom,left,top,right,bottom);
		//printf("%d %d %d %d / %d %d %d %d\n",MainScreenRect.left,MainScreenRect.top,MainScreenRect.right,MainScreenRect.bottom,SubScreenRect.left,SubScreenRect.top,SubScreenRect.right,SubScreenRect.bottom);
		if (ddraw.OK())
		{
			DD_FillRect(ddraw.surface.primary, 0, 0, left, top, RGB(255, 0, 0)); //topleft
			DD_FillRect(ddraw.surface.primary, left, 0, right, top, RGB(128, 0, 0)); //topcenter
			DD_FillRect(ddraw.surface.primary, right, 0, wr.right, top, RGB(0, 255, 0)); //topright
			DD_FillRect(ddraw.surface.primary, 0, top, left, bottom, RGB(0, 128, 0));  //left
			DD_FillRect(ddraw.surface.primary, right, top, wr.right, bottom, RGB(0, 0, 255)); //right
			DD_FillRect(ddraw.surface.primary, 0, bottom, left, wr.bottom, RGB(0, 0, 128)); //bottomleft
			DD_FillRect(ddraw.surface.primary, left, bottom, right, wr.bottom, RGB(255, 0, 255)); //bottomcenter
			DD_FillRect(ddraw.surface.primary, right, bottom, wr.right, wr.bottom, RGB(0, 255, 255)); //bottomright
			if (video.layout == 1)
			{
				DD_FillRect(ddraw.surface.primary, SubScreenRect.left, wr.top, wr.right, SubScreenRect.top, RGB(0, 0, 0)); //Top gap left when centering the resized screen
				DD_FillRect(ddraw.surface.primary, SubScreenRect.left, SubScreenRect.bottom, wr.right, wr.bottom, RGB(0, 0, 0)); //Bottom gap left when centering the resized screen
			}
		}
	}

	for (int i = 0; i < 2; i++)
	{
		if (i && video.layout == 2)
			break;

		if (!ddraw.blt(dstRects[i], srcRects[i])) return;
	}

	if (video.layout == 1) return;
	if (video.layout == 2) return;

	// Gap
	if (video.screengap > 0)
	{
		//u32 color = gapColors[win_fw_config.fav_colour];
		//u32 color_rev = (((color & 0xFF) << 16) | (color & 0xFF00) | ((color & 0xFF0000) >> 16));
		u32 color_rev = (u32)ScreenGapColor;

		HDC dc;
		HBRUSH brush;

		dc = GetDC(MainWindow->getHWnd());
		brush = CreateSolidBrush(color_rev);

		FillRect(dc, &GapRect, brush);

		DeleteObject((HGDIOBJ)brush);
		ReleaseDC(MainWindow->getHWnd(), dc);
	}
}

void displayProc()
{
	slock_lock(display_mutex);

	//find a buffer to display
	int todo = newestDisplayBuffer;
	bool alreadyDisplayed = (todo == currDisplayBuffer);

	slock_unlock(display_mutex);

	//something new to display:
	if (!alreadyDisplayed) {
		//start displaying a new buffer
		currDisplayBuffer = todo;
		video.srcBuffer = (u8*)displayBuffers[currDisplayBuffer].buffer;
		video.srcBufferSize = displayBuffers[currDisplayBuffer].size;
	}
	DoDisplay();
}
void displayThread(void *arg)
{
	do
	{
		if ((MainWindow == NULL) || IsMinimized(MainWindow->getHWnd()))
		{
			WaitForSingleObject(display_wakeup_event, INFINITE);
		}
		else if ((emu_paused || !execute || !romloaded) && (!HudEditorMode && !CommonSettings.hud.ShowInputDisplay && !CommonSettings.hud.ShowGraphicalInputDisplay))
		{
			WaitForSingleObject(display_wakeup_event, 250);
		}
		else
		{
			WaitForSingleObject(display_wakeup_event, 10);
		}

		if (display_die)
		{
			break;
		}

		displayProc();
		SetEvent(display_done_event);
	} while (!display_die);
}

static void DoDisplay_DrawHud()
{
	osd->update();
	DrawHUD();
	osd->clear();
}

void Display()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();

	if (CommonSettings.single_core())
	{
		video.srcBuffer = (u8*)dispInfo.masterCustomBuffer;
		video.srcBufferSize = dispInfo.customWidth * dispInfo.customHeight * dispInfo.pixelBytes * 2;
		DoDisplay();
	}
	else
	{
		if (display_thread == NULL)
		{
			display_mutex = slock_new();
			display_thread = sthread_create(&displayThread, nullptr);
		}

		slock_lock(display_mutex);

		if (int diff = (currDisplayBuffer + 1) % 3 - newestDisplayBuffer)
			newestDisplayBuffer += diff;
		else newestDisplayBuffer = (currDisplayBuffer + 2) % 3;

		DisplayBuffer& db = displayBuffers[newestDisplayBuffer];
		size_t targetSize = dispInfo.customWidth * dispInfo.customHeight * dispInfo.pixelBytes * 2;
		if (db.size != targetSize)
		{
			free_aligned(db.buffer);
			db.buffer = (u32*)malloc_alignedPage(targetSize);
			db.size = targetSize;
		}
		memcpy(db.buffer, dispInfo.masterCustomBuffer, targetSize);

		slock_unlock(display_mutex);

		SetEvent(display_wakeup_event);
	}
}

//does a single display work unit. only to be used from the display thread
void DoDisplay()
{
	Lock lock(win_backbuffer_sync);

	if (displayPostponeType && !displayNoPostponeNext && (displayPostponeType < 0 || timeGetTime() < displayPostponeUntil))
		return;
	displayNoPostponeNext = false;

	//we have to do a copy here because we're about to draw the OSD onto it. bummer.
	if (gpu_bpp == 15)
		ColorspaceConvertBuffer555To8888Opaque<true, false>((u16 *)video.srcBuffer, video.buffer, video.srcBufferSize / 2);
	else
		ColorspaceConvertBuffer888XTo8888Opaque<true, false>((u32*)video.srcBuffer, video.buffer, video.srcBufferSize / 4);

	//some games use the backlight for fading effects
	const size_t pixCount = video.prefilterWidth * video.prefilterHeight / 2;
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
	ColorspaceApplyIntensityToBuffer32<false, false>(video.buffer, pixCount, displayInfo.backlightIntensity[NDSDisplayID_Main]);
	ColorspaceApplyIntensityToBuffer32<false, false>(video.buffer + pixCount, pixCount, displayInfo.backlightIntensity[NDSDisplayID_Touch]);

	// Lua draws to the HUD buffer, so clear it here instead of right before redrawing the HUD.
	aggDraw.hud->clear();
	if (AnyLuaActive())
	{
		if (sthread_isself(display_thread))
		{
			InvokeOnMainThread((void(*)(DWORD))
				CallRegisteredLuaFunctions, LUACALL_AFTEREMULATIONGUI);
		}
		else
		{
			CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);
		}
	}

	// draw hud
	DoDisplay_DrawHud();
	if (displayMethod == DISPMETHOD_DDRAW_HW || displayMethod == DISPMETHOD_DDRAW_SW)
	{
		// DirectDraw doesn't support alpha blending, so we must scale and overlay the HUD ourselves.
		T_AGG_RGBA target((u8*)video.buffer, video.prefilterWidth, video.prefilterHeight, video.prefilterWidth * 4);
		target.transformImage(aggDraw.hud->image<T_AGG_PF_RGBA>(), 0, 0, video.prefilterWidth, video.prefilterHeight);
	}

	//apply user's filter
	video.filter();

	if (displayMethod == DISPMETHOD_DDRAW_HW || displayMethod == DISPMETHOD_DDRAW_SW)
	{
		gldisplay.kill();
		DD_DoDisplay();
	}
	else
	{
		OGL_DoDisplay();
	}
}

void KillDisplay()
{
	display_die = true;
	SetEvent(display_wakeup_event);
	if (display_thread)
		sthread_join(display_thread);
}

void UpdateWndRects(HWND hwnd, RECT* newClientRect)
{
	POINT ptClient;
	RECT rc;

	GapRect = {0,0,0,0};

	bool maximized = IsZoomed(hwnd) != FALSE;

	int wndWidth, wndHeight;
	int defHeight = video.height;
	if (video.layout == 0)
		defHeight += video.scaledscreengap();
	float ratio;
	int oneScreenHeight, gapHeight;
	int tbheight;

	//if we're in the middle of resizing the window, GetClientRect will return the old rect
	if (newClientRect)
		rc = RECT(*newClientRect);
	else
		GetClientRect(hwnd, &rc);

	if (maximized)
		rc = FullScreenRect;

	tbheight = MainWindowToolbar->GetHeight();

	if (video.layout == 1) //horizontal
	{
		rc = CalculateDisplayLayoutWrapper(rc, (int)((float)GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / screenSizeRatio), GPU_FRAMEBUFFER_NATIVE_HEIGHT, tbheight, maximized);

		wndWidth = (rc.bottom - rc.top) - tbheight;
		wndHeight = (rc.right - rc.left);

		ratio = ((float)wndHeight / (float)512);
		oneScreenHeight = (int)std::round(((float)GPU_FRAMEBUFFER_NATIVE_WIDTH * ratio));
		int oneScreenWidth = (int)std::round(((float)GPU_FRAMEBUFFER_NATIVE_HEIGHT * ratio));
		int vResizedScrOffset = 0;

		// Main screen
		ptClient.x = rc.left;
		ptClient.y = rc.top;
		ClientToScreen(hwnd, &ptClient);
		MainScreenRect.left = ptClient.x;
		MainScreenRect.top = ptClient.y;
		ptClient.x = (rc.left + std::floor(oneScreenHeight * screenSizeRatio));
		ptClient.y = (rc.top + wndWidth);
		ClientToScreen(hwnd, &ptClient);
		MainScreenRect.right = ptClient.x;
		MainScreenRect.bottom = ptClient.y;

		// Sub screen
		ptClient.x = (rc.left + oneScreenHeight * screenSizeRatio);
		if (vCenterResizedScr && ForceRatio)
			vResizedScrOffset = (wndWidth - oneScreenWidth * (2 - screenSizeRatio)) / 2;
		ptClient.y = rc.top + vResizedScrOffset;
		ClientToScreen(hwnd, &ptClient);
		SubScreenRect.left = ptClient.x;
		SubScreenRect.top = ptClient.y;
		ptClient.x = (rc.left + oneScreenHeight * 2);
		if (ForceRatio)
			ptClient.y = (rc.top + vResizedScrOffset + oneScreenWidth * (2 - screenSizeRatio));
		else
			ptClient.y = (rc.top + wndWidth);
		ClientToScreen(hwnd, &ptClient);
		SubScreenRect.right = ptClient.x;
		SubScreenRect.bottom = ptClient.y;
	}
	else
		if (video.layout == 2) //one screen
		{
			rc = CalculateDisplayLayoutWrapper(rc, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, tbheight, maximized);

			wndWidth = (rc.bottom - rc.top) - tbheight;
			wndHeight = (rc.right - rc.left);

			ratio = ((float)wndHeight / (float)defHeight);
			oneScreenHeight = (int)((video.height) * ratio);

			// Main screen
			ptClient.x = rc.left;
			ptClient.y = rc.top;
			ClientToScreen(hwnd, &ptClient);
			MainScreenRect.left = ptClient.x;
			MainScreenRect.top = ptClient.y;
			ptClient.x = (rc.left + oneScreenHeight);
			ptClient.y = (rc.top + wndWidth);
			ClientToScreen(hwnd, &ptClient);
			MainScreenRect.right = ptClient.x;
			MainScreenRect.bottom = ptClient.y;
			SetRectEmpty(&SubScreenRect);
		}
		else
			if (video.layout == 0) //vertical
			{
				//apply logic to correct things if forced integer is selected
				if ((video.rotation == 90) || (video.rotation == 270))
				{
					rc = CalculateDisplayLayoutWrapper(rc, (GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2) + video.screengap, GPU_FRAMEBUFFER_NATIVE_WIDTH, tbheight, maximized);
				}
				else
				{
					rc = CalculateDisplayLayoutWrapper(rc, GPU_FRAMEBUFFER_NATIVE_WIDTH, (GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2) + video.screengap, tbheight, maximized);
				}

				if ((video.rotation == 90) || (video.rotation == 270))
				{
					wndWidth = (rc.bottom - rc.top) - tbheight;
					wndHeight = (rc.right - rc.left);
				}
				else
				{
					wndWidth = (rc.right - rc.left);
					wndHeight = (rc.bottom - rc.top) - tbheight;
				}

				ratio = ((float)wndHeight / (float)defHeight);

				oneScreenHeight = (int)((video.height / 2) * ratio);
				gapHeight = (wndHeight - (oneScreenHeight * 2));

				if ((video.rotation == 90) || (video.rotation == 270))
				{
					// Main screen
					ptClient.x = rc.left;
					ptClient.y = rc.top;
					ClientToScreen(hwnd, &ptClient);
					MainScreenRect.left = ptClient.x;
					MainScreenRect.top = ptClient.y;
					ptClient.x = (rc.left + oneScreenHeight);
					ptClient.y = (rc.top + wndWidth);
					ClientToScreen(hwnd, &ptClient);
					MainScreenRect.right = ptClient.x;
					MainScreenRect.bottom = ptClient.y;

					//if there was no specified screen gap, extend the top screen to cover the extra column
					if (video.screengap == 0) MainScreenRect.right += gapHeight;

					// Sub screen
					ptClient.x = (rc.left + oneScreenHeight + gapHeight);
					ptClient.y = rc.top;
					ClientToScreen(hwnd, &ptClient);
					SubScreenRect.left = ptClient.x;
					SubScreenRect.top = ptClient.y;
					ptClient.x = (rc.left + oneScreenHeight + gapHeight + oneScreenHeight);
					ptClient.y = (rc.top + wndWidth);
					ClientToScreen(hwnd, &ptClient);
					SubScreenRect.right = ptClient.x;
					SubScreenRect.bottom = ptClient.y;

					// Gap
					GapRect.left = (rc.left + oneScreenHeight);
					GapRect.top = rc.top;
					GapRect.right = (rc.left + oneScreenHeight + gapHeight);
					GapRect.bottom = (rc.top + wndWidth);
				}
				else
				{


					// Main screen
					ptClient.x = rc.left;
					ptClient.y = rc.top;
					ClientToScreen(hwnd, &ptClient);
					MainScreenRect.left = ptClient.x;
					MainScreenRect.top = ptClient.y;
					ptClient.x = (rc.left + wndWidth);
					ptClient.y = (rc.top + oneScreenHeight);
					ClientToScreen(hwnd, &ptClient);
					MainScreenRect.right = ptClient.x;
					MainScreenRect.bottom = ptClient.y;

					//if there was no specified screen gap, extend the top screen to cover the extra row
					if (video.screengap == 0) MainScreenRect.bottom += gapHeight;

					// Sub screen
					ptClient.x = rc.left;
					ptClient.y = (rc.top + oneScreenHeight + gapHeight);
					ClientToScreen(hwnd, &ptClient);
					SubScreenRect.left = ptClient.x;
					SubScreenRect.top = ptClient.y;
					ptClient.x = (rc.left + wndWidth);
					ptClient.y = (rc.top + oneScreenHeight + gapHeight + oneScreenHeight);
					ClientToScreen(hwnd, &ptClient);
					SubScreenRect.right = ptClient.x;
					SubScreenRect.bottom = ptClient.y;

					// Gap
					GapRect.left = rc.left;
					GapRect.top = (rc.top + oneScreenHeight);
					GapRect.right = (rc.left + wndWidth);
					GapRect.bottom = (rc.top + oneScreenHeight + gapHeight);
				}
			}

	MainScreenRect.top += tbheight;
	MainScreenRect.bottom += tbheight;
	SubScreenRect.top += tbheight;
	SubScreenRect.bottom += tbheight;
	GapRect.top += tbheight;
	GapRect.bottom += tbheight;
}

static void InvokeOnMainThread(void(*function)(DWORD), DWORD argument)
{
	ResetEvent(display_invoke_ready_event);
	display_invoke_argument = argument;
	display_invoke_function = function;
	PostMessage(MainWindow->getHWnd(), WM_CUSTINVOKE, 0, 0); // in case a modal dialog or menu is open
	SignalObjectAndWait(display_invoke_ready_event, display_invoke_done_event, display_invoke_timeout, FALSE);
	display_invoke_function = NULL;
}
void _ServiceDisplayThreadInvocation()
{
	Lock lock(display_invoke_handler_cs);
	DWORD res = WaitForSingleObject(display_invoke_ready_event, display_invoke_timeout);
	if (res != WAIT_ABANDONED && display_invoke_function)
		display_invoke_function(display_invoke_argument);
	display_invoke_function = NULL;
	SetEvent(display_invoke_done_event);
}

void TwiddleLayer(UINT ctlid, int core, int layer)
{
	GPUEngineBase *gpu = ((GPUEngineID)core == GPUEngineID_Main) ? (GPUEngineBase *)GPU->GetEngineMain() : (GPUEngineBase *)GPU->GetEngineSub();

	const bool newLayerState = !CommonSettings.dispLayers[core][layer];
	gpu->SetLayerEnableState(layer, newLayerState);
	MainWindow->checkMenu(ctlid, newLayerState);
}

void SetLayerMasks(int mainEngineMask, int subEngineMask)
{
	static const size_t numCores = sizeof(CommonSettings.dispLayers) / sizeof(CommonSettings.dispLayers[0]);
	static const size_t numLayers = sizeof(CommonSettings.dispLayers[0]) / sizeof(CommonSettings.dispLayers[0][0]);
	static const UINT ctlids[numCores][numLayers] = {
		{IDM_MBG0, IDM_MBG1, IDM_MBG2, IDM_MBG3, IDM_MOBJ},
		{IDM_SBG0, IDM_SBG1, IDM_SBG2, IDM_SBG3, IDM_SOBJ},
	};

	for (int core = 0; core < 2; core++)
	{
		GPUEngineBase *gpu = (GPUEngineID)core == GPUEngineID_Main ? (GPUEngineBase *)GPU->GetEngineMain() : (GPUEngineBase *)GPU->GetEngineSub();
		const int mask = core == 0 ? mainEngineMask : subEngineMask;
		for (size_t layer = 0; layer < numLayers; layer++)
		{
			const bool newLayerState = (mask >> layer) & 0x01 != 0;
			if (newLayerState != CommonSettings.dispLayers[core][layer])
			{
				gpu->SetLayerEnableState(layer, newLayerState);
				MainWindow->checkMenu(ctlids[core][layer], newLayerState);
			}
		}
	}
}
