/*  main.cpp

	Copyright 2006 Theo Berkau
    Copyright (C) 2006-2009 DeSmuME team

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

// icon gradient: #f6f6fb to #8080c0
// RGB(246, 246, 251) to RGB(128, 128, 192)

#include "windriver.h"

#include <algorithm>
#include <string>
#include <vector>

#include <Winuser.h>
#include <Winnls.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <tchar.h>

#include <stdio.h>

//emulator core
#include "../common.h" //wtf this needs to disappear
#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../debug.h"
#include "../saves.h"
#include "../addons.h"
#include "../GPU_osd.h"
#include "../OGLRender.h"
#include "../rasterize.h"
#include "../gfx3d.h"
#include "../render3D.h"
#include "../gdbstub.h"
#include "../cheatSystem.h"
#include "../mic.h"
#include "../movie.h"
#include "../firmware.h"
#include "../lua-engine.h"
#include "../path.h"

//other random stuff
#include "recentroms.h"
#include "main.h"
#include "resource.h"
#include "CWindow.h"
#include "gthread.h"
#include "version.h"
#include "inputdx.h"
#include "console.h"
#include "throttle.h"
#include "hotkey.h"
#include "snddx.h"
#include "commandline.h"
#include "7zip.h"
#include "OpenArchive.h"
#include "utils/xstring.h"
#include "directx/ddraw.h"
#include "video.h"
#include "aggdraw.h"
#include "agg2d.h"
#include "winutil.h"

//tools and dialogs
#include "pathsettings.h"
#include "colorctrl.h"
#include "ginfo.h"
#include "IORegView.h"
#include "palView.h"
#include "tileView.h"
#include "oamView.h"
#include "mapview.h"
#include "matrixview.h"
#include "lightview.h"
#include "gbaslot_config.h"
#include "cheatsWin.h"
#include "memView.h"
#include "disView.h"
#include "FirmConfig.h"
#include "AboutBox.h"
#include "replay.h"
#include "ramwatch.h"
#include "ram_search.h"
#include "aviout.h"
#include "soundView.h"

using namespace std;

#define HAVE_REMOTE
#define WPCAP
#define PACKET_SIZE 65535

#include <pcap.h>
#include <remote-ext.h> //uh?

VideoInfo video;

#include "wx/wxprec.h"
#ifdef _M_X64  
	#ifdef __WXDEBUG__
		#pragma comment(lib,"wxmsw28d_core-x64.lib")
		#pragma comment(lib,"wxbase28d-x64.lib")
	#else
		#pragma comment(lib,"wxmsw28_core-x64.lib")
		#pragma comment(lib,"wxbase28-x64.lib")
	#endif
#else
	#ifdef __WXDEBUG__
		#pragma comment(lib,"wxmsw28d_core.lib")
		#pragma comment(lib,"wxbase28d.lib")
	#else
		#pragma comment(lib,"wxmsw28_core.lib")
		#pragma comment(lib,"wxbase28.lib")
	#endif
#endif
#pragma comment(lib,"comctl32.lib")
#include "../wxdlg/wxdlg3dViewer.h"

class wxDesmumeApp : public wxApp
{
public:
	//call me each frame or something.
	//sort of an idle routine
	static void frameUpdate()
	{
		if(!wxTheApp) return;
		wxDesmumeApp* self = ((wxDesmumeApp*)wxTheApp);
		self->DeletePendingObjects();
	}
};

IMPLEMENT_APP_NO_MAIN( wxDesmumeApp )

void wxTest() {
	//wxdlg3dViewer *viewer = new wxdlg3dViewer(NULL);
	//viewer->Show(true);
    //wxTestModeless *frame = new wxTestModeless(_T("Controls wxWidgets App"), 50, 50);
    //frame->Show(true);
}

#ifndef PUBLIC_RELEASE
#define DEVELOPER_MENU_ITEMS
#endif

const int kGapNone = 0;
const int kGapBorder = 5;
const int kGapNDS = 64; // extremely tilted (but some games seem to use this value)
const int kGapNDS2 = 90; // more normal viewing angle
const int kScale1point5x = 65535;
const int kScale2point5x = 65534;

static BOOL OpenCore(const char* filename);
BOOL Mic_DeInit_Physical();
BOOL Mic_Init_Physical();

static bool _cheatsDisabled = false;

void UpdateHotkeyAssignments();				//Appends hotkey mappings to corresponding menu items

HMENU mainMenu = NULL; //Holds handle to the main DeSmuME menu

DWORD hKeyInputTimer;

extern LRESULT CALLBACK RamSearchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitRamSearch();
void FilterUpdate(HWND hwnd, bool user=true);

CRITICAL_SECTION win_execute_sync;
volatile int win_sound_samplecounter = 0;

CRITICAL_SECTION win_backbuffer_sync;
volatile bool backbuffer_invalidate = false;

Lock::Lock() : m_cs(&win_execute_sync) { EnterCriticalSection(m_cs); }
Lock::Lock(CRITICAL_SECTION& cs) : m_cs(&cs) { EnterCriticalSection(m_cs); }
Lock::~Lock() { LeaveCriticalSection(m_cs); }

//===================== DirectDraw vars
LPDIRECTDRAW7			lpDDraw=NULL;
LPDIRECTDRAWSURFACE7	lpPrimarySurface=NULL;
LPDIRECTDRAWSURFACE7	lpBackSurface=NULL;
DDSURFACEDESC2			ddsd;
LPDIRECTDRAWCLIPPER		lpDDClipPrimary=NULL;
LPDIRECTDRAWCLIPPER		lpDDClipBack=NULL;

//===================== Input vars
#define WM_CUSTKEYDOWN	(WM_USER+50)
#define WM_CUSTKEYUP	(WM_USER+51)

#define WM_CUSTINVOKE	(WM_USER+52)

#ifndef __WISPSHRD_H
#define WM_TABLET_DEFBASE                    0x02C0
#define WM_TABLET_MAXOFFSET                  0x20
#define WM_TABLET_FLICK                      (WM_TABLET_DEFBASE + 11)
#define WM_TABLET_QUERYSYSTEMGESTURESTATUS   (WM_TABLET_DEFBASE + 12)
#define MICROSOFT_TABLETPENSERVICE_PROPERTY _T("MicrosoftTabletPenServiceProperty")
#define TABLET_DISABLE_PRESSANDHOLD        0x00000001
#define TABLET_DISABLE_PENTAPFEEDBACK      0x00000008
#define TABLET_DISABLE_PENBARRELFEEDBACK   0x00000010
#define TABLET_DISABLE_TOUCHUIFORCEON      0x00000100
#define TABLET_DISABLE_TOUCHUIFORCEOFF     0x00000200
#define TABLET_DISABLE_TOUCHSWITCH         0x00008000
#define TABLET_DISABLE_FLICKS              0x00010000
#define TABLET_ENABLE_FLICKSONCONTEXT      0x00020000
#define TABLET_ENABLE_FLICKLEARNINGMODE    0x00040000
#define TABLET_DISABLE_SMOOTHSCROLLING     0x00080000
#define TABLET_DISABLE_FLICKFALLBACKKEYS   0x00100000
#endif

void DesEnableMenuItem(HMENU hMenu, UINT uIDEnableItem, bool enable);
inline bool IsDlgCheckboxChecked(HWND hDlg, int id)
{
	return IsDlgButtonChecked(hDlg,id) == BST_CHECKED;
}

void CheckDlgItem(HWND hDlg, int id, bool checked)
{
	CheckDlgButton(hDlg, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

char SavName[MAX_PATH] = "";
char ImportSavName[MAX_PATH] = "";
char szClassName[ ] = "DeSmuME";
int romnum = 0;

DWORD threadID;

WINCLASS	*MainWindow=NULL;

//HWND hwnd;
//HDC  hdc;
HACCEL hAccel;
HINSTANCE hAppInst = NULL;
RECT FullScreenRect, MainScreenRect, SubScreenRect, GapRect;
RECT MainScreenSrcRect, SubScreenSrcRect;
int WndX = 0;
int WndY = 0;

extern HWND RamSearchHWnd;
static bool lostFocusPause = true;
static bool lastPauseFromLostFocus = false;
static bool FrameLimit = true;

std::vector<HWND> LuaScriptHWnds;
LRESULT CALLBACK LuaScriptProc(HWND, UINT, WPARAM, LPARAM);

#define MAX_RECENT_SCRIPTS 15 // must match value in luaconsole.cpp (belongs in a header, but I didn't want to create one just for this)
extern char Recent_Scripts[MAX_RECENT_SCRIPTS][1024];

//=========================== view tools
TOOLSCLASS	*ViewDisasm_ARM7 = NULL;
TOOLSCLASS	*ViewDisasm_ARM9 = NULL;
TOOLSCLASS	*ViewPalette = NULL;
TOOLSCLASS	*ViewTiles = NULL;
TOOLSCLASS	*ViewMaps = NULL;
TOOLSCLASS	*ViewOAM = NULL;
TOOLSCLASS	*ViewMatrices = NULL;
TOOLSCLASS	*ViewLights = NULL;

volatile bool execute = false;
volatile bool paused = true;
volatile BOOL pausedByMinimize = FALSE;
u32 glock = 0;

BOOL finished = FALSE;
bool romloaded = false;

void SetScreenGap(int gap);

void SetRotate(HWND hwnd, int rot, bool user=true);

bool ForceRatio = true;
bool SeparationBorderDrag = true;
int ScreenGapColor = 0xFFFFFF;
float DefaultWidth;
float DefaultHeight;
float widthTradeOff;
float heightTradeOff;

extern bool StylusAutoHoldPressed;
extern POINT winLastTouch;
extern bool userTouchesScreen;

/*__declspec(thread)*/ bool inFrameBoundary = false;

static int sndcoretype=SNDCORE_DIRECTX;
static int sndbuffersize=DESMUME_SAMPLE_RATE*8/60;
static int snd_synchmode=0;
static int snd_synchmethod=0;
int sndvolume=100;

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDDIRECTX,
	NULL
};

GPU3DInterface *core3DList[] = {
	&gpu3DNull,
	&gpu3Dgl,
	&gpu3DRasterize,
	NULL
};

bool autoframeskipenab=1;
int frameskiprate=0;
int lastskiprate=0;
int emu_paused = 0;
bool frameAdvance = false;
bool continuousframeAdvancing = false;
bool staterewindingenabled = false;

unsigned short windowSize = 0;

/* the firmware settings */
struct NDS_fw_config_data win_fw_config;

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

LRESULT CALLBACK GFX3DSettingsDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK SoundSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EmulationSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MicrophoneSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WifiSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

//struct configured_features {
//	u16 arm9_gdb_port;
//	u16 arm7_gdb_port;
//
//	const char *cflash_disk_image_file;
//};

static int KeyInDelayMSec = 0;
static int KeyInRepeatMSec = 8;

template<bool JOYSTICK>
static void InputTimer()
{
	bool S9xGetState (WORD KeyIdent);

	static DWORD lastTime = timeGetTime();
	DWORD currentTime = timeGetTime();

	static struct JoyState {
		bool wasPressed;
		DWORD firstPressedTime;
		DWORD lastPressedTime;
		WORD repeatCount;
	} joyState [256];
	static bool initialized = false;

	if(!initialized) {
		for(int i = 0; i < 256; i++) {
			joyState[i].wasPressed = false;
			joyState[i].repeatCount = 1;
		}
		initialized = true;
	}

	for (int z = 0; z < 256; z++) {
		int i = z | (JOYSTICK?0x8000:0);
		bool active = !S9xGetState(i);

		if (active) {
			bool keyRepeat = (currentTime - joyState[z].firstPressedTime) >= (DWORD)KeyInDelayMSec;
			if (!joyState[z].wasPressed || keyRepeat) {
				if (!joyState[z].wasPressed)
					joyState[z].firstPressedTime = currentTime;
				joyState[z].lastPressedTime = currentTime;
				if (keyRepeat && joyState[z].repeatCount < 0xffff)
					joyState[z].repeatCount++;
				int mods = GetInitialModifiers(i);
				WPARAM wparam = i | (mods << 8);
				PostMessage(MainWindow->getHWnd(), WM_CUSTKEYDOWN, wparam,(LPARAM)(joyState[z].repeatCount | (joyState[z].wasPressed ? 0x40000000 : 0)));
			}
		}
		else {
			joyState[z].repeatCount = 1;
			if (joyState[z].wasPressed)
			{
				int mods = GetInitialModifiers(i);
				WPARAM wparam = i | (mods << 8);
				PostMessage(MainWindow->getHWnd(), WM_CUSTKEYUP, wparam,(LPARAM)(joyState[z].repeatCount | (joyState[z].wasPressed ? 0x40000000 : 0)));
			}
		}
		joyState[z].wasPressed = active;
	}
	lastTime = currentTime;
}

VOID CALLBACK KeyInputTimer( UINT idEvent, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	InputTimer<false>();
	InputTimer<true>();
}

void ScaleScreen(float factor)
{
	if(windowSize == 0)
	{
		int defw = GetPrivateProfileInt("Video", "Window width", 256, IniName);
		int defh = GetPrivateProfileInt("Video", "Window height", 384, IniName);

		// fix for wrong rotation
		int w1x = 0; 
		int h1x = 0; 

		if (video.layout == 0)
		{
			w1x = video.rotatedwidthgap();
			h1x = video.rotatedheightgap();
		}
		else
			if (video.layout == 1)
			{
				w1x = video.rotatedwidthgap() * 2;
				h1x = video.rotatedheightgap() / 2;
			}
			else
				if (video.layout == 2)
				{
					w1x = video.rotatedwidthgap();
					h1x = video.rotatedheightgap() / 2;
				}
		if((defw > defh) != (w1x > h1x))
		{
			int temp = defw;
			defw = defh;
			defh = temp;
		}
		// fix for wrong gap

		if(defh*w1x < h1x*defw)
			defh = defw*h1x/w1x;
		else if(defw*h1x < w1x*defh)
			defw = defh*w1x/h1x;

		MainWindow->setClientSize(defw, defh);
	}
	else
	{
		if(factor==kScale1point5x)
			factor = 1.5f;
		else if(factor==kScale2point5x)
			factor = 2.5f;
		if (video.layout == 0)
			MainWindow->setClientSize((int)(video.rotatedwidthgap() * factor), (int)(video.rotatedheightgap() * factor));
		else
			if (video.layout == 1)
				MainWindow->setClientSize((int)(video.rotatedwidthgap() * factor * 2), (int)(video.rotatedheightgap() * factor / 2));
			else
				if (video.layout == 2)
					MainWindow->setClientSize((int)(video.rotatedwidthgap() * factor), (int)(video.rotatedheightgap() * factor / 2));
	}
}

void SetMinWindowSize()
{
	if(ForceRatio)
		MainWindow->setMinSize(video.rotatedwidth(), video.rotatedheight());
	else
		MainWindow->setMinSize(video.rotatedwidthgap(), video.rotatedheightgap());
}

static void GetNdsScreenRect(RECT* r)
{
	RECT zero; 
	SetRect(&zero,0,0,0,0);
	if(zero == MainScreenRect) *r = SubScreenRect;
	else if(zero == SubScreenRect) *r = MainScreenRect;
	else
	{
		RECT* dstRects [2] = {&MainScreenRect, &SubScreenRect};
		int left = min(dstRects[0]->left,dstRects[1]->left);
		int top = min(dstRects[0]->top,dstRects[1]->top);
		int right = max(dstRects[0]->right,dstRects[1]->right);
		int bottom = max(dstRects[0]->bottom,dstRects[1]->bottom);
		SetRect(r,left,top,right,bottom);
	}
}

void UnscaleScreenCoords(s32& x, s32& y)
{
	HWND hwnd = MainWindow->getHWnd();
	
	int defwidth = video.width;
	int defheight = video.height;

	POINT pt;
	pt.x = x; pt.y = y;
	ClientToScreen(hwnd,&pt);
	x = pt.x; y = pt.y;

	RECT r;
	GetNdsScreenRect(&r);
	int winwidth = (r.right-r.left), winheight = (r.bottom-r.top);

	x -= r.left;
	y -= r.top;

	if(winwidth == 0 || winheight == 0)
	{
		x = 0;
		y = 0;
		return;
	}

	if (video.layout == 0)
	{
		defheight += video.scaledscreengap();

		// translate from scaling (screen resolution to 256x384 or 512x192) 
		switch (video.rotation)
		{
			case 0:
			case 180:
				x = (x*defwidth) / winwidth;
				y = (y*defheight) / winheight;
				break ;
			case 90:
			case 270:
				x = (x*defheight) / winwidth;
				y = (y*defwidth) / winheight;
				break ;
		}
	}
	else
		if (video.layout == 1)
		{
			//INFO("----- coords x = %i, y = %i\n", x, y);
			x = (x*defwidth) / winwidth * 2;
			y = (y*defheight) / winheight / 2;
		}
		else
			if (video.layout == 2)
			{
				//INFO("----- coords x = %i, y = %i\n", x, y);
				x = (x*defwidth) / winwidth;
				y = (y*defheight) / winheight / 2;
			}

	x = video.dividebyratio(x);
	y = video.dividebyratio(y);
}

// input x,y should be windows client-space coords already at 1x scaling.
// output is in pixels relative to the top-left of the chosen screen.
// (-1 == top screen, 1 == bottom screen, 0 == absolute vertically aligned)
// the gap between screens (if any) is subtracted away from the output y.
void ToDSScreenRelativeCoords(s32& x, s32& y, int whichScreen)
{
	s32 tx=x, ty=y;

	if (video.layout == 0)
	{
		int gapSize = video.dividebyratio(video.scaledscreengap());
		// first deal with rotation
		switch(video.rotation)
		{
			case 90:
				x = ty;
				y = (383+gapSize)-tx;
				break;
			case 180:
				x = 255-tx;
				y = (383+gapSize)-ty;
				break;
			case 270:
				x = 255-ty;
				y = tx;
				break;
		}

		// then deal with screen gap
		if(y > 191 + gapSize)
			y -= gapSize;
		else if(y > 191 + gapSize/2)
			y = 192;
		else if(y > 191)
			y = 191;
	}

	// finally, make it relative to the correct screen
	if (video.layout == 0 || video.layout == 2)
	{
		if(whichScreen)
		{
			bool topOnTop = (video.swap == 0) || (video.swap == 2 && !MainScreen.offset) || (video.swap == 3 && MainScreen.offset);
			bool bottom = (whichScreen > 0);
			if(topOnTop)
				y += bottom ? -192 : 0;
			else
				y += (y < 192) ? (bottom ? 0 : 192) : (bottom ? 0 : -192);
		}
	}
	else if (video.layout == 1) // side-by-side
	{
		if(whichScreen)
		{
			bool topOnTop = (video.swap == 0) || (video.swap == 2 && !MainScreen.offset) || (video.swap == 3 && MainScreen.offset);
			bool bottom = (whichScreen > 0);
			if(topOnTop)
				x += bottom ? -256 : 0;
			else
				x += (x < 256) ? (bottom ? 0 : 256) : (bottom ? 0 : -256);
		}
		else
		{
			if(x >= 256)
			{
				x -= 256;
				y += 192;
			}
			else if(x < 0)
			{
				x += 256;
				y -= 192;
			}
		}
	}
}

// END Rotation definitions

//-----window style handling----
const u32 DWS_NORMAL = 0;
const u32 DWS_ALWAYSONTOP = 1;
const u32 DWS_LOCKDOWN = 2;
const u32 DWS_FULLSCREEN = 4;

static u32 currWindowStyle = DWS_NORMAL;
static void SetStyle(u32 dws)
{
	//pokefan's suggestion, there are a number of ways we could do this.
	//i sort of like this because it is very visually indicative of being locked down
	DWORD ws = GetWindowLong(MainWindow->getHWnd(),GWL_STYLE);
	ws &= ~(WS_CAPTION | WS_POPUP | WS_THICKFRAME | WS_DLGFRAME );
	if(dws & DWS_LOCKDOWN)
		ws |= WS_POPUP | WS_DLGFRAME;
	else if(!(dws&DWS_FULLSCREEN)) 
		ws |= WS_CAPTION | WS_THICKFRAME;

	SetWindowLong(MainWindow->getHWnd(),GWL_STYLE, ws);


	if((dws&DWS_FULLSCREEN)) 
		SetMenu(MainWindow->getHWnd(),NULL);
	else 
		SetMenu(MainWindow->getHWnd(),mainMenu);

	currWindowStyle = dws;
	HWND insertAfter = HWND_NOTOPMOST;
	if(dws & DWS_ALWAYSONTOP) 
		insertAfter = HWND_TOPMOST;
	SetWindowPos(MainWindow->getHWnd(), insertAfter, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

static u32 GetStyle() { return currWindowStyle; }

static void ToggleFullscreen()
{
	u32 style = GetStyle();
	style ^= DWS_FULLSCREEN;
	SetStyle(style);
	if(style&DWS_FULLSCREEN)
		ShowWindow(MainWindow->getHWnd(),SW_MAXIMIZE);
	else ShowWindow(MainWindow->getHWnd(),SW_NORMAL);
}
//---------



static HMENU GetMenuItemParent(UINT itemId, HMENU hMenu = mainMenu)
{
	MENUITEMINFO moo = {sizeof(MENUITEMINFO)};
	if(!GetMenuItemInfo(hMenu, itemId, FALSE, &moo))
		return NULL;
	int nItems = GetMenuItemCount(hMenu);
	for(int i = 0; i < nItems; i++)
	{
		MENUITEMINFO mii = {sizeof(MENUITEMINFO), MIIM_SUBMENU | MIIM_ID};
		GetMenuItemInfo(hMenu, i, TRUE, &mii);
		if(mii.wID == itemId)
			return hMenu;
		if(mii.hSubMenu)
			if(HMENU innerResult = GetMenuItemParent(itemId, mii.hSubMenu))
				return innerResult;
	}
	return NULL;
}

static void PopulateLuaSubmenu()
{
	static HMENU luasubmenu = GetMenuItemParent(IDC_NEW_LUA_SCRIPT);
	static int luasubmenuOriginalSize = GetMenuItemCount(luasubmenu);

	// delete any previously-dynamically-added menu items
	int i = luasubmenuOriginalSize;
	while(GetMenuItemCount(luasubmenu) > i)
		DeleteMenu(luasubmenu, i, MF_BYPOSITION);

	// add menu items for currently-open scripts
	char Str_Tmp [1024];
	int Flags = MF_BYPOSITION | MF_STRING;
	if(!LuaScriptHWnds.empty())
	{
		InsertMenu(luasubmenu, i++, MF_SEPARATOR, NULL, NULL);
		for(unsigned int j = 0; j < LuaScriptHWnds.size(); j++)
		{
			GetWindowText(LuaScriptHWnds[j], Str_Tmp, 1024);
			InsertMenu(luasubmenu, i++, MF_BYPOSITION | MF_STRING, IDC_LUASCRIPT_RESERVE_START+j, Str_Tmp);
		}
	}

	// add menu items for recently-opened-but-not-currently-open scripts
	int dividerI = i;
	for(unsigned int j = 0; j < MAX_RECENT_SCRIPTS; j++)
	{
		const char* pathPtr = Recent_Scripts[j];
		if(!*pathPtr)
			continue;

		HWND IsScriptFileOpen(const char* Path);
		if(IsScriptFileOpen(pathPtr))
			continue;

		// only show some of the path
		const char* pathPtrSearch;
		int slashesLeft = 2;
		for(pathPtrSearch = pathPtr + strlen(pathPtr) - 1; 
			pathPtrSearch != pathPtr && slashesLeft >= 0;
			pathPtrSearch--)
		{
			char c = *pathPtrSearch;
			if(c == '\\' || c == '/')
				slashesLeft--;
		}
		if(slashesLeft < 0)
			pathPtr = pathPtrSearch + 2;
		strcpy(Str_Tmp, pathPtr);

		if(i == dividerI)
			InsertMenu(luasubmenu, i++, MF_SEPARATOR, NULL, NULL);

		InsertMenu(luasubmenu, i++, MF_BYPOSITION | MF_STRING, IDD_LUARECENT_RESERVE_START+j, Str_Tmp);
	}
}



int CreateDDrawBuffers()
{
	Lock lock (win_backbuffer_sync);

	if (lpDDClipPrimary!=NULL) { IDirectDraw7_Release(lpDDClipPrimary); lpDDClipPrimary = NULL; }
	if (lpPrimarySurface != NULL) { IDirectDraw7_Release(lpPrimarySurface); lpPrimarySurface = NULL; }
	if (lpBackSurface != NULL) { IDirectDraw7_Release(lpBackSurface); lpBackSurface = NULL; }

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddsd.dwFlags = DDSD_CAPS;
	if (IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpPrimarySurface, NULL) != DD_OK) return -1;

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize          = sizeof(ddsd);
	ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN;

	ddsd.dwWidth         = video.rotatedwidth();
	ddsd.dwHeight        = video.rotatedheight();

	if (IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpBackSurface, NULL) != DD_OK) return -2;

	if (IDirectDraw7_CreateClipper(lpDDraw, 0, &lpDDClipPrimary, NULL) != DD_OK) return -3;
	if (IDirectDrawClipper_SetHWnd(lpDDClipPrimary, 0, MainWindow->getHWnd()) != DD_OK) return -4;
	if (IDirectDrawSurface7_SetClipper(lpPrimarySurface, lpDDClipPrimary) != DD_OK) return -5;

	backbuffer_invalidate = false;
	return 1;
}


template<typename T> static void doRotate(void* dst)
{
	u8* buffer = (u8*)dst;
	int size = video.size();
	u32* src = (u32*)video.finalBuffer();
	int width = video.width;
	int height = video.height;
	int pitch = ddsd.lPitch;
	switch(video.rotation)
	{
	case 0:
	case 180:
		{
			if(pitch == 1024)
			{
				if(video.rotation==0)
					if(sizeof(T) == sizeof(u32))
						memcpy(buffer, src, size * sizeof(u32));
					else
						for(int i = 0; i < size; i++)
							((T*)buffer)[i] = src[i];
				else // if(video.rotation==180)
					for(int i = 0, j=size-1; j>=0; i++,j--)
						((T*)buffer)[i] = src[j];
			}
			else
			{
				if(video.rotation==0)
					if(sizeof(T) != sizeof(u32))
						for(int y = 0; y < height; y++)
						{
							for(int x = 0; x < width; x++)
								((T*)buffer)[x] = src[(y * width) + x];

							buffer += pitch;
						}
					else
						for(int y = 0; y < height; y++)
						{
							memcpy(buffer, &src[y * width], width * sizeof(u32));
							buffer += pitch;
						}
				else // if(video.rotation==180)
					for(int y = 0; y < height; y++)
					{
						for(int x = 0; x < width; x++)
							((T*)buffer)[x] = src[height*width - (y * width) - x - 1];

						buffer += pitch;
					}
			}
		}
		break;
	case 90:
	case 270:
		{
			if(video.rotation == 90)
				for(int y = 0; y < width; y++)
				{
					for(int x = 0; x < height; x++)
						((T*)buffer)[x] = src[(((height-1)-x) * width) + y];

					buffer += pitch;
				}
			else
				for(int y = 0; y < width; y++)
				{
					for(int x = 0; x < height; x++)
						((T*)buffer)[x] = src[((x) * width) + (width-1) - y];

					buffer += pitch;
				}
		}
		break;
	}
}

void UpdateWndRects(HWND hwnd)
{
	POINT ptClient;
	RECT rc;

	bool maximized = IsZoomed(hwnd);

	int wndWidth, wndHeight;
	int defHeight = video.height;
	if(video.layout == 0)
		defHeight += video.scaledscreengap();
	float ratio;
	int oneScreenHeight, gapHeight;

	GetClientRect(hwnd, &rc);

	if(maximized)
		rc = FullScreenRect;


	if (video.layout == 1)
	{
		wndWidth = (rc.bottom - rc.top);
		wndHeight = (rc.right - rc.left);

		ratio = ((float)wndHeight / (float)defHeight);
		oneScreenHeight = (int)((video.height/2) * ratio);

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

		// Sub screen
		ptClient.x = (rc.left + oneScreenHeight);
		ptClient.y = rc.top;
		ClientToScreen(hwnd, &ptClient);
		SubScreenRect.left = ptClient.x;
		SubScreenRect.top = ptClient.y;
		ptClient.x = (rc.left + oneScreenHeight + oneScreenHeight);
		ptClient.y = (rc.top + wndWidth);
		ClientToScreen(hwnd, &ptClient);
		SubScreenRect.right = ptClient.x;
		SubScreenRect.bottom = ptClient.y;
	}
	else
	if (video.layout == 2)
	{

		wndWidth = (rc.bottom - rc.top);
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
	}
	else
	if (video.layout == 0)
	{
		if((video.rotation == 90) || (video.rotation == 270))
		{
			wndWidth = (rc.bottom - rc.top);
			wndHeight = (rc.right - rc.left);
		}
		else
		{
			wndWidth = (rc.right - rc.left);
			wndHeight = (rc.bottom - rc.top);
		}

		ratio = ((float)wndHeight / (float)defHeight);
		oneScreenHeight = (int)((video.height/2) * ratio);
		gapHeight = (wndHeight - (oneScreenHeight * 2));

		if((video.rotation == 90) || (video.rotation == 270))
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
			if(video.screengap == 0) MainScreenRect.right += gapHeight;

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
			if(video.screengap == 0) MainScreenRect.bottom += gapHeight;

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
}

void FixAspectRatio();

void LCDsSwap(int swapVal)
{
	if(swapVal == -1) swapVal = video.swap ^ 1; // -1 means to flip the existing setting
	if(swapVal < 0 || swapVal > 3) swapVal = 0;
	if(osd && !(swapVal & video.swap & 1)) osd->swapScreens = !osd->swapScreens; // 1-frame fixup
	video.swap = swapVal;
	WritePrivateProfileInt("Video", "LCDsSwap", video.swap, IniName);
}

void doLCDsLayout()
{
	HWND hwnd = MainWindow->getHWnd();

	bool maximized = IsZoomed(hwnd)==TRUE;

	if(maximized) ShowWindow(hwnd,SW_NORMAL);

	if(video.layout != 0)
	{
		// rotation is not supported in the alternate layouts
		if(video.rotation != 0)
			SetRotate(hwnd, 0, false);
	}

	osd->singleScreen = (video.layout == 2);

	RECT rc = { 0 };
	int oldheight, oldwidth;
	int newheight, newwidth;

	GetClientRect(hwnd, &rc);
	oldwidth = (rc.right - rc.left);
	oldheight = (rc.bottom - rc.top);
	newwidth = oldwidth;
	newheight = oldheight;

	if (video.layout == 0)
	{
		DesEnableMenuItem(mainMenu, IDC_ROTATE90, true);
		DesEnableMenuItem(mainMenu, IDC_ROTATE180, true);
		DesEnableMenuItem(mainMenu, IDC_ROTATE270, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NONE, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_BORDER, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NDSGAP, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NDSGAP2, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_DRAGEDIT, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORWHITE, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORGRAY, true);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORBLACK, true);

		if (video.layout_old == 1)
		{
			newwidth = oldwidth / 2;
			newheight = oldheight * 2;
		}
		else
			if (video.layout_old == 2)
			{
				newwidth = oldwidth;
				newheight = oldheight * 2;
			}
				else
				{
					newwidth = oldwidth;
					newheight = oldheight;
				}
		MainWindow->checkMenu(ID_LCDS_VERTICAL, true);
		MainWindow->checkMenu(ID_LCDS_HORIZONTAL, false);
		MainWindow->checkMenu(ID_LCDS_ONE, false);


	}
	else
	{
		DesEnableMenuItem(mainMenu, IDC_ROTATE90, false);
		DesEnableMenuItem(mainMenu, IDC_ROTATE180, false);
		DesEnableMenuItem(mainMenu, IDC_ROTATE270, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NONE, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_BORDER, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NDSGAP, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NDSGAP2, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_DRAGEDIT, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORWHITE, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORGRAY, false);
		DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORBLACK, false);

		if (video.layout == 1)
		{
			if (video.layout_old == 0)
			{
				newwidth = oldwidth * 2;
				newheight = oldheight / 2;
			}
			else
				if (video.layout_old == 2)
				{
					newwidth = oldwidth * 2;
					newheight = oldheight;
				}
				else
					{
						newwidth = oldwidth;
						newheight = oldheight;
					}
			MainWindow->checkMenu(ID_LCDS_HORIZONTAL, false);
			MainWindow->checkMenu(ID_LCDS_VERTICAL, true);
			MainWindow->checkMenu(ID_LCDS_ONE, false);
		}
		else
			if (video.layout == 2)
			{
				if (video.layout_old == 0)
				{
					newwidth = oldwidth;
					newheight = oldheight / 2;
				}
				else
					if (video.layout_old == 1)
					{
						newwidth = oldwidth / 2;
						newheight = oldheight;
					}
					else
						{
							newwidth = oldwidth;
							newheight = oldheight;
						}
				MainWindow->checkMenu(ID_LCDS_HORIZONTAL, false);
				MainWindow->checkMenu(ID_LCDS_VERTICAL, false);
				MainWindow->checkMenu(ID_LCDS_ONE, true);
			}
			else
				return;
	}

	video.layout_old = video.layout;
	WritePrivateProfileInt("Video", "LCDsLayout", video.layout, IniName);
	SetMinWindowSize();
	if(video.rotation == 90 || video.rotation == 270)
	{
		int temp = newwidth;
		newwidth = newheight;
		newheight = temp;
	}

	MainWindow->setClientSize(newwidth, newheight);
	FixAspectRatio();
	UpdateWndRects(hwnd);

	if(video.layout == 0)
	{
		// restore user-set rotation if we forcibly disabled it before
		if(video.rotation != video.rotation_userset)
			SetRotate(hwnd, video.rotation_userset, false);
	}

	if(maximized) ShowWindow(hwnd,SW_MAXIMIZE);
}

#pragma pack(push,1)
struct pix24
{
	u8 b,g,r;
	FORCEINLINE pix24(u32 s) : b(s&0xFF), g((s&0xFF00)>>8), r((s&0xFF0000)>>16) {}
};
struct pix16
{
	u16 c;
	FORCEINLINE pix16(u32 s) : c(((s&0xF8)>>3) | ((s&0xFC00)>>5) | ((s&0xF80000)>>8)) {}
};
struct pix15
{
	u16 c;
	FORCEINLINE pix15(u32 s) : c(((s&0xF8)>>3) | ((s&0xF800)>>6) | ((s&0xF80000)>>9)) {}
};
#pragma pack(pop)

static void DD_FillRect(LPDIRECTDRAWSURFACE7 surf, int left, int top, int right, int bottom, DWORD color)
{
	RECT r;
	SetRect(&r,left,top,right,bottom);
	DDBLTFX fx;
	memset(&fx,0,sizeof(DDBLTFX));
	fx.dwSize = sizeof(DDBLTFX);
	//fx.dwFillColor = color;
	fx.dwFillColor = 0; //color is just for debug
	surf->Blt(&r,NULL,NULL,DDBLT_COLORFILL | DDBLT_WAIT,&fx);
}

//the directdraw final presentation portion of display, including rotating
static void DD_DoDisplay()
{
	int res;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;
	res = lpBackSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);

	if(backbuffer_invalidate)
		goto CREATE;

	if(FAILED(res))
	{
		if (res==DDERR_SURFACELOST)
		{
			LOG("DirectDraw buffers is lost\n");
			if (FAILED(IDirectDrawSurface7_Restore(lpPrimarySurface))
			 || FAILED(IDirectDrawSurface7_Restore(lpBackSurface)))
			{
			CREATE:
				if(CreateDDrawBuffers() < 0)
					return;
			}
		}
		else
			return;
	}

	//res = lpBackSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
	//if(FAILED(res))
	//	return;

	char* buffer = (char*)ddsd.lpSurface;

	switch(ddsd.ddpfPixelFormat.dwRGBBitCount)
	{
	case 32:
		doRotate<u32>(ddsd.lpSurface);
		break;
	case 24:
		doRotate<pix24>(ddsd.lpSurface);
		break;
	case 16:
		if(ddsd.ddpfPixelFormat.dwGBitMask != 0x3E0)
			doRotate<pix16>(ddsd.lpSurface);
		else
			doRotate<pix15>(ddsd.lpSurface);
		break;
	default:
		{
			if(ddsd.ddpfPixelFormat.dwRGBBitCount != 0)
				INFO("Unsupported color depth: %i bpp\n", ddsd.ddpfPixelFormat.dwRGBBitCount);
			//emu_halt();
		}
		break;
	}


	lpBackSurface->Unlock((LPRECT)ddsd.lpSurface);

	RECT* dstRects [2] = {&MainScreenRect, &SubScreenRect};
	RECT* srcRects [2];

	if(video.swap == 0)
	{
		srcRects[0] = &MainScreenSrcRect;
		srcRects[1] = &SubScreenSrcRect;
		if(osd) osd->swapScreens = false;
	}
	else if(video.swap == 1)
	{
		srcRects[0] = &SubScreenSrcRect;
		srcRects[1] = &MainScreenSrcRect;
		if(osd) osd->swapScreens = true;
	}
	else if(video.swap == 2)
	{
		srcRects[0] = (MainScreen.offset) ? &SubScreenSrcRect : &MainScreenSrcRect;
		srcRects[1] = (MainScreen.offset) ? &MainScreenSrcRect : &SubScreenSrcRect;
		if(osd) osd->swapScreens = (MainScreen.offset != 0);
	}
	else if(video.swap == 3)
	{
		srcRects[0] = (MainScreen.offset) ? &MainScreenSrcRect : &SubScreenSrcRect;
		srcRects[1] = (MainScreen.offset) ? &SubScreenSrcRect : &MainScreenSrcRect;
		if(osd) osd->swapScreens = (SubScreen.offset != 0);
	}

	//this code fills in all the undrawn areas if we are in fullscreen mode.
	//it is probably a waste of time to keep black-filling all this, but we need to do it to be safe.
	if(IsZoomed(MainWindow->getHWnd()))
	{
		RECT fullScreen;
		GetWindowRect(MainWindow->getHWnd(),&fullScreen);
		RECT r;
		GetNdsScreenRect(&r);
		int left = r.left;
		int top = r.top;
		int right = r.right;
		int bottom = r.bottom;
		//printf("%d %d %d %d / %d %d %d %d\n",fullScreen.left,fullScreen.top,fullScreen.right,fullScreen.bottom,left,top,right,bottom);
		//printf("%d %d %d %d / %d %d %d %d\n",MainScreenRect.left,MainScreenRect.top,MainScreenRect.right,MainScreenRect.bottom,SubScreenRect.left,SubScreenRect.top,SubScreenRect.right,SubScreenRect.bottom);
		DD_FillRect(lpPrimarySurface,0,0,left,top,RGB(255,0,0)); //topleft
		DD_FillRect(lpPrimarySurface,left,0,right,top,RGB(128,0,0)); //topcenter
		DD_FillRect(lpPrimarySurface,right,0,fullScreen.right,top,RGB(0,255,0)); //topright
		DD_FillRect(lpPrimarySurface,0,top,left,bottom,RGB(0,128,0));  //left
		DD_FillRect(lpPrimarySurface,right,top,fullScreen.right,bottom,RGB(0,0,255)); //right
		DD_FillRect(lpPrimarySurface,0,bottom,left,fullScreen.bottom,RGB(0,0,128)); //bottomleft
		DD_FillRect(lpPrimarySurface,left,bottom,right,fullScreen.bottom,RGB(255,0,255)); //bottomcenter
		DD_FillRect(lpPrimarySurface,right,bottom,fullScreen.left,fullScreen.bottom,RGB(0,255,255)); //bottomright
	}

	for(int i = 0; i < 2; i++)
	{
		if(i && video.layout == 2)
			break;

		if(lpPrimarySurface->Blt(dstRects[i], lpBackSurface, srcRects[i], DDBLT_WAIT, 0) == DDERR_SURFACELOST)
		{
			LOG("DirectDraw buffers is lost\n");
			if(IDirectDrawSurface7_Restore(lpPrimarySurface) == DD_OK)
				IDirectDrawSurface7_Restore(lpBackSurface);
		}
	}

	if (video.layout == 1) return;
	if (video.layout == 2) return;

	// Gap
	if(video.screengap > 0)
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

//triple buffering logic
u16 displayBuffers[3][256*192*4];
volatile int currDisplayBuffer=-1;
volatile int newestDisplayBuffer=-2;
GMutex *display_mutex = NULL;
GThread *display_thread = NULL;
volatile bool display_die = false;
HANDLE display_wakeup_event = INVALID_HANDLE_VALUE;

int displayPostponeType = 0;
DWORD displayPostponeUntil = ~0;
bool displayNoPostponeNext = false;

DWORD display_invoke_argument = 0;
void (*display_invoke_function)(DWORD) = NULL;
HANDLE display_invoke_ready_event = INVALID_HANDLE_VALUE;
HANDLE display_invoke_done_event = INVALID_HANDLE_VALUE;
DWORD display_invoke_timeout = 500;
CRITICAL_SECTION display_invoke_handler_cs;

static void InvokeOnMainThread(void (*function)(DWORD), DWORD argument);

static void DoDisplay_DrawHud()
{
	osd->update();
	DrawHUD();
	osd->clear();
}

//does a single display work unit. only to be used from the display thread
static void DoDisplay(bool firstTime)
{
	Lock lock (win_backbuffer_sync);

	if(displayPostponeType && !displayNoPostponeNext && (displayPostponeType < 0 || timeGetTime() < displayPostponeUntil))
		return;
	displayNoPostponeNext = false;

	//convert pixel format to 32bpp for compositing
	//why do we do this over and over? well, we are compositing to 
	//filteredbuffer32bpp, and it needs to get refreshed each frame..
	const int size = video.size();
	u16* src = (u16*)video.srcBuffer;
	for(int i=0;i<size;i++)
 		video.buffer[i] = RGB15TO24_REVERSE(src[i]);

	if(firstTime)
	{
		//on single core systems, draw straight to the screen
		//we only do this once per emulated frame because we don't want to waste time redrawing
		//on such lousy computers
		if(CommonSettings.single_core())
		{
			aggDraw.hud->attach((u8*)video.buffer, 256, 384, 1024);
			DoDisplay_DrawHud();
		}
	}

	if(AnyLuaActive())
	{
		if(g_thread_self() == display_thread)
		{
			InvokeOnMainThread((void(*)(DWORD))
				CallRegisteredLuaFunctions, LUACALL_AFTEREMULATIONGUI);
		}
		else
		{
			CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);
		}
	}

	//apply user's filter
	video.filter();

	if(!CommonSettings.single_core())
	{
		//draw and composite the OSD (but not if we are drawing osd straight to screen)
		DoDisplay_DrawHud();
		T_AGG_RGBA target((u8*)video.finalBuffer(), video.width,video.height,video.width*4);
		target.transformImage(aggDraw.hud->image<T_AGG_PF_RGBA>(), 0,0,video.width,video.height);
		aggDraw.hud->clear();
	}
	
	DD_DoDisplay();
}

void displayProc()
{
	g_mutex_lock(display_mutex);

	//find a buffer to display
	int todo = newestDisplayBuffer;
	bool alreadyDisplayed = (todo == currDisplayBuffer);

	g_mutex_unlock(display_mutex);
	
	//something new to display:
	if(!alreadyDisplayed) {
		//start displaying a new buffer
		currDisplayBuffer = todo;
		video.srcBuffer = (u8*)displayBuffers[currDisplayBuffer];

		DoDisplay(true);
	}
	else
	{
		DoDisplay(false);
	}
}


void displayThread(void*)
{
	for(;;) {
		if(display_die) return;
		displayProc();
		//Sleep(10); //don't be greedy and use a whole cpu core, but leave room for 60fps 
		WaitForSingleObject(display_wakeup_event, 10); // same as sleep but lets something wake us up early
	}
}

void KillDisplay()
{
	display_die = true;
	SetEvent(display_wakeup_event);
	g_thread_join(display_thread);
}

void Display()
{
	if(CommonSettings.single_core())
	{
		video.srcBuffer = (u8*)GPU_screen;
		DoDisplay(true);
	}
	else
	{
		if(display_thread == NULL)
		{
			display_mutex = g_mutex_new();
			display_thread = g_thread_create( (GThreadFunc)displayThread,
											 NULL,
											 TRUE,
											 NULL);
		}

		g_mutex_lock(display_mutex);

		if(int diff = (currDisplayBuffer+1)%3 - newestDisplayBuffer)
			newestDisplayBuffer += diff;
		else newestDisplayBuffer = (currDisplayBuffer+2)%3;

		memcpy(displayBuffers[newestDisplayBuffer],GPU_screen,256*192*4);

		g_mutex_unlock(display_mutex);
	}
}



void CheckMessages()
{
	MSG msg;
	HWND hwnd = MainWindow->getHWnd();

	#ifdef WX_STUB
	wxDesmumeApp::frameUpdate();
	#endif

	while( PeekMessage( &msg, 0, 0, 0, PM_NOREMOVE ) )
	{
		if( GetMessage( &msg, 0,  0, 0)>0 )
		{
			if (RamWatchHWnd && IsDialogMessage(RamWatchHWnd, &msg))
			{
				if(msg.message == WM_KEYDOWN) // send keydown messages to the dialog (for accelerators, and also needed for the Alt key to work)
					SendMessage(RamWatchHWnd, msg.message, msg.wParam, msg.lParam);
				continue;
			}
			if (SoundView_GetHWnd() && IsDialogMessage(SoundView_GetHWnd(), &msg))
				continue;

			if(!TranslateAccelerator(hwnd,hAccel,&msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
}

static void InvokeOnMainThread(void (*function)(DWORD), DWORD argument)
{
	ResetEvent(display_invoke_ready_event);
	display_invoke_argument = argument;
	display_invoke_function = function;
	PostMessage(MainWindow->getHWnd(), WM_CUSTINVOKE, 0, 0); // in case a modal dialog or menu is open
	SignalObjectAndWait(display_invoke_ready_event, display_invoke_done_event, display_invoke_timeout, FALSE);
	display_invoke_function = NULL;
}
static void _ServiceDisplayThreadInvocation()
{
	Lock lock (display_invoke_handler_cs);
	DWORD res = WaitForSingleObject(display_invoke_ready_event, display_invoke_timeout);
	if(res != WAIT_ABANDONED && display_invoke_function)
		display_invoke_function(display_invoke_argument);
	display_invoke_function = NULL;
	SetEvent(display_invoke_done_event);
}
static FORCEINLINE void ServiceDisplayThreadInvocations()
{
	if(display_invoke_function)
		_ServiceDisplayThreadInvocation();
}


static struct MainLoopData
{
	u64 freq;
	int framestoskip;
	int framesskipped;
	int skipnextframe;
	u64 lastticks;
	u64 curticks;
	u64 diffticks;
	u64 fpsticks;
	HWND hwnd;
	int fps;
	int fps3d;
	int fpsframecount;
	int toolframecount;
} mainLoopData = {0};


static void StepRunLoop_Core()
{
	input_acquire();
	NDS_beginProcessingInput();
	{
		input_process();
		FCEUMOV_HandlePlayback();
		CallRegisteredLuaFunctions(LUACALL_BEFOREEMULATION);
	}
	NDS_endProcessingInput();
	FCEUMOV_HandleRecording();

	inFrameBoundary = false;
	{
		Lock lock;
		NDS_exec<false>();
		SPU_Emulate_user();
		win_sound_samplecounter = DESMUME_SAMPLE_RATE/60;
	}
	inFrameBoundary = true;
	DRV_AviVideoUpdate((u16*)GPU_screen);

	extern bool rewinding;

	if (staterewindingenabled) {

		if(rewinding)
			dorewind();
		else
			rewindsave();
	}

	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
	ServiceDisplayThreadInvocations();
}

static void StepRunLoop_Paused()
{
	paused = TRUE;
	Sleep(100);

	// periodically update single-core OSD when paused and in the foreground
	if(CommonSettings.single_core() && GetActiveWindow() == mainLoopData.hwnd)
	{
		video.srcBuffer = (u8*)GPU_screen;
		DoDisplay(true);
	}

	ServiceDisplayThreadInvocations();
}

static void StepRunLoop_User()
{
	const int kFramesPerToolUpdate = 1;

	Hud.fps = mainLoopData.fps;
	Hud.fps3d = mainLoopData.fps3d;

	Display();

	gfx3d.frameCtrRaw++;
	if(gfx3d.frameCtrRaw == 60) {
		mainLoopData.fps3d = gfx3d.frameCtr;
		gfx3d.frameCtrRaw = 0;
		gfx3d.frameCtr = 0;
	}


	mainLoopData.toolframecount++;
	if (mainLoopData.toolframecount == kFramesPerToolUpdate)
	{
		if(SoundView_IsOpened()) SoundView_Refresh();
		RefreshAllToolWindows();

		mainLoopData.toolframecount = 0;
	}

	Update_RAM_Search(); // Update_RAM_Watch() is also called.

	mainLoopData.fpsframecount++;
	QueryPerformanceCounter((LARGE_INTEGER *)&mainLoopData.curticks);
	bool oneSecond = mainLoopData.curticks >= mainLoopData.fpsticks + mainLoopData.freq;
	if(oneSecond) // TODO: print fps on screen in DDraw
	{
		mainLoopData.fps = mainLoopData.fpsframecount;
		mainLoopData.fpsframecount = 0;
		QueryPerformanceCounter((LARGE_INTEGER *)&mainLoopData.fpsticks);
	}

	if(nds.idleFrameCounter==0 || oneSecond) 
	{
		//calculate a 16 frame arm9 load average
		int load = 0;
		for(int i=0;i<16;i++)
			load = load/8 + nds.runCycleCollector[(i+nds.idleFrameCounter)&15]*7/8;
		load = std::min(100,std::max(0,(int)(load*100/1120380)));
		Hud.arm9load = load;
	}

	Hud.cpuloopIterationCount = nds.cpuloopIterationCount;
}

static void StepRunLoop_Throttle(bool allowSleep = true, int forceFrameSkip = -1)
{
	int skipRate = (forceFrameSkip < 0) ? frameskiprate : forceFrameSkip;
	int ffSkipRate = (forceFrameSkip < 0) ? 9 : forceFrameSkip;

	if(lastskiprate != skipRate)
	{
		lastskiprate = skipRate;
		mainLoopData.framestoskip = 0; // otherwise switches to lower frameskip rates will lag behind
	}

	if(!mainLoopData.skipnextframe || forceFrameSkip == 0 || frameAdvance || (continuousframeAdvancing && !FastForward))
	{
		mainLoopData.framesskipped = 0;

		if (mainLoopData.framestoskip > 0)
			mainLoopData.skipnextframe = 1;
	}
	else
	{
		mainLoopData.framestoskip--;

		if (mainLoopData.framestoskip < 1)
			mainLoopData.skipnextframe = 0;
		else
			mainLoopData.skipnextframe = 1;

		mainLoopData.framesskipped++;

		NDS_SkipNextFrame();
	}

	if(FastForward)
	{
		if(mainLoopData.framesskipped < ffSkipRate)
		{
			mainLoopData.skipnextframe = 1;
			mainLoopData.framestoskip = 1;
		}
		if (mainLoopData.framestoskip < 1)
			mainLoopData.framestoskip += ffSkipRate;
	}
	else if((/*autoframeskipenab && frameskiprate ||*/ FrameLimit) && allowSleep)
	{
		SpeedThrottle();
	}

	if (autoframeskipenab && frameskiprate)
	{
		if(!frameAdvance && !continuousframeAdvancing)
		{
			AutoFrameSkip_NextFrame();
			if (mainLoopData.framestoskip < 1)
				mainLoopData.framestoskip += AutoFrameSkip_GetSkipAmount(0,skipRate);
		}
	}
	else
	{
		if (mainLoopData.framestoskip < 1)
			mainLoopData.framestoskip += skipRate;
	}

	if (frameAdvance && allowSleep)
	{
		frameAdvance = false;
		emu_halt();
		SPU_Pause(1);
	}
	if(execute && emu_paused && !frameAdvance)
	{
		// safety net against running out of control in case this ever happens.
		Unpause(); Pause();
	}

	ServiceDisplayThreadInvocations();
}

DWORD WINAPI run()
{
	u32 cycles = 0;
	int wait = 0;
	u64 OneFrameTime = ~0;

	HWND hwnd = MainWindow->getHWnd();
	mainLoopData.hwnd = hwnd;

	inFrameBoundary = true;

	InitSpeedThrottle();

	osd->setRotate(video.rotation);
	//doLCDsLayout();
	if (DirectDrawCreateEx(NULL, (LPVOID*)&lpDDraw, IID_IDirectDraw7, NULL) != DD_OK)
	{
		MessageBox(hwnd,"Unable to initialize DirectDraw","Error",MB_OK);
		return -1;
	}

	if (IDirectDraw7_SetCooperativeLevel(lpDDraw,hwnd, DDSCL_NORMAL) != DD_OK)
	{
		MessageBox(hwnd,"Unable to set DirectDraw Cooperative Level","Error",MB_OK);
		return -1;
	}

	if (CreateDDrawBuffers()<1)
	{
		MessageBox(hwnd,"Unable to set DirectDraw buffers","Error",MB_OK);
		return -1;
	}

	QueryPerformanceFrequency((LARGE_INTEGER *)&mainLoopData.freq);
	QueryPerformanceCounter((LARGE_INTEGER *)&mainLoopData.lastticks);
	OneFrameTime = mainLoopData.freq / 60;

	while(!finished)
	{
		while(execute)
		{
			StepRunLoop_Core();
			StepRunLoop_User();
			StepRunLoop_Throttle();
			CheckMessages();
		}
		StepRunLoop_Paused();
		CheckMessages();
	}

	return 1;
}

void NDS_Pause(bool showMsg)
{
	if (!paused)
	{
		emu_halt();
		paused = TRUE;
		SPU_Pause(1);
		while (!paused) {}
		if (showMsg) INFO("Emulation paused\n");
	}
}

void NDS_UnPause(bool showMsg)
{
	if (romloaded && paused)
	{
		paused = FALSE;
		pausedByMinimize = FALSE;
		execute = TRUE;
		SPU_Pause(0);
		if (showMsg) INFO("Emulation unpaused\n");
	}
}


/***
 * Author: Hicoder
 * Function: UpdateSaveStateMenu
 * Date Added: April 20, 2009
 * Description: Updates the specified savestate menus
 * Known Usage:
 *				ResetSaveStateTimes
 *				LoadSaveStateInfo
 **/
void UpdateSaveStateMenu(int pos, char* txt)
{
	ModifyMenu(mainMenu,IDM_STATE_SAVE_F1 + pos, MF_BYCOMMAND | MF_STRING, IDM_STATE_SAVE_F1 + pos,  txt);
	ModifyMenu(mainMenu,IDM_STATE_LOAD_F1 + pos, MF_BYCOMMAND | MF_STRING, IDM_STATE_LOAD_F1 + pos,	 txt);
}

/***
 * Author: Hicoder
 * Function: ResetSaveStateTime
 * Date Added: April 20, 2009
 * Description: Resets the times for save states to blank
 * Known Usage:
 *				LoadRom
 **/
void ResetSaveStateTimes()
{
	char ntxt[64];
	for(int i = 0; i < NB_STATES;i++)
	{
		sprintf(ntxt,"%d %s", i+1, "");
		UpdateSaveStateMenu(i, ntxt);
	}
}

/***
 * Author: Hicoder
 * Function: LoadSaveStateInfo
 * Date Added: April 20, 2009
 * Description: Checks The Rom thats opening for save states asscociated with it
 * Known Usage:
 *				LoadRom
 **/
void LoadSaveStateInfo()
{
	scan_savestates();
	char ntxt[128];
	for(int i = 0; i < NB_STATES; i++)
	{
		if(savestates[i].exists)
		{
			sprintf(ntxt, "&%d    %s", i+1, savestates[i].date);
			UpdateSaveStateMenu(i, ntxt);
		}
	}
}

static BOOL LoadROM(const char * filename, const char * logicalName)
{
	ResetSaveStateTimes();
	NDS_Pause();
	//if (strcmp(filename,"")!=0) INFO("Attempting to load ROM: %s\n",filename);

	if (NDS_LoadROM(filename, logicalName) > 0)
	{
		INFO("Loading %s was successful\n",logicalName);
		LoadSaveStateInfo();
		lagframecounter=0;
		UpdateRecentRoms(logicalName);
		osd->setRotate(video.rotation);
		if (AutoRWLoad)
		{
			//Open Ram Watch if its auto-load setting is checked
			OpenRWRecentFile(0);	
			RamWatchHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_RAMWATCH), MainWindow->getHWnd(), (DLGPROC) RamWatchProc);
		}
		if (autoframeskipenab && frameskiprate) AutoFrameSkip_IgnorePreviousDelay();

		return TRUE;		
	}
	INFO("Loading %s FAILED.\n",logicalName);
	return FALSE;
}

void OpenRecentROM(int listNum)
{
	if (listNum > MAX_RECENT_ROMS) return; //Just in case
	char filename[MAX_PATH];
	strcpy(filename, RecentRoms[listNum].c_str());
	//LOG("Attempting to load %s\n",filename);
	if(OpenCore(filename))
	{
		romloaded = TRUE;
	}
	else
	//Rom failed to load, ask the user how to handle it
	{
		string str = "Could not open ";
		str.append(filename);
		str.append("\n\nRemove from list?");
		if (MessageBox(MainWindow->getHWnd(), str.c_str(), "File error", MB_YESNO) == IDYES)
		{
			RemoveRecentRom(RecentRoms[listNum]);
		}
	}

	NDS_UnPause();
}

/*
* The thread handling functions needed by the GDB stub code.
*/
void *
createThread_gdb( void (APIENTRY *thread_function)( void *data),
				 void *thread_data) {
					 void *new_thread = CreateThread( NULL, 0,
						 (LPTHREAD_START_ROUTINE)thread_function, thread_data,
						 0, NULL);

					 return new_thread;
}

void
joinThread_gdb( void *thread_handle) {
}


int MenuInit()
{
	mainMenu = LoadMenu(hAppInst, MAKEINTRESOURCE(MENU_PRINCIPAL)); //Load Menu, and store handle
	if (!MainWindow->setMenu(mainMenu)) return 0;

	InitRecentRoms();

	ResetSaveStateTimes();

#ifndef DEVELOPER_MENU_ITEMS
	// menu items that are only useful for desmume developers (maybe)
	HMENU fileMenu = GetSubMenu(mainMenu, 0);
	DeleteMenu(fileMenu, IDM_FILE_RECORDUSERSPUWAV, MF_BYCOMMAND);
#endif

	return 1;
}

void MenuDeinit()
{
	MainWindow->setMenu(NULL);
	DestroyMenu(mainMenu);
}

typedef int (WINAPI *setLanguageFunc)(LANGID id);

void SetLanguage(int langid)
{
	HMODULE kernel32 = LoadLibrary("kernel32.dll");
	FARPROC _setThreadUILanguage = (FARPROC)GetProcAddress(kernel32,"SetThreadUILanguage");
	setLanguageFunc setLanguage = _setThreadUILanguage?(setLanguageFunc)_setThreadUILanguage:(setLanguageFunc)SetThreadLocale;

	switch(langid)
	{
	case 0:
		// English
		setLanguage(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
		break;
	case 1:
		// French       
		setLanguage(MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH), SORT_DEFAULT));
		break;
	case 3:
		// Chinese
		setLanguage(MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_DEFAULT));
		break;
	case 4:
		// Italian
		setLanguage(MAKELCID(MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN), SORT_DEFAULT));
		break;
	case 5:
		// Japanese
		setLanguage(MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT));
		break;

	default:
		break;
	}

	FreeLibrary(kernel32);
}

void SaveLanguage(int langid)
{
	char text[80];

	sprintf(text, "%d", langid);
	WritePrivateProfileString("General", "Language", text, IniName);
}

void CheckLanguage(UINT id)
{
	int i;
	for (i = IDC_LANGENGLISH; i < IDC_LANGJAPANESE+1; i++)
		MainWindow->checkMenu(i, false);

	MainWindow->checkMenu(id, true);
}

void ChangeLanguage(int id)
{
	SetLanguage(id);

	MenuDeinit();
	MenuInit();
}


static void ExitRunLoop()
{
	finished = TRUE;
	emu_halt();
}

class WinDriver : public BaseDriver
{
	virtual bool WIFI_Host_InitSystem() 
	{
		#ifdef EXPERIMENTAL_WIFI_COMM
			WSADATA wsaData; 	 
			WORD version = MAKEWORD(1,1); 	 
			if (WSAStartup(version, &wsaData)) 	 
			{ 	 
				printf("Failed initializing WSA.\n"); 	 
				return false ; 	 
			}
			//require wpcap.dll
			HMODULE temp = LoadLibrary("wpcap.dll");
			if(temp == NULL) {
				printf("Failed initializing wpcap.dll - SoftAP support disabled.\n");
				return false;
			}
			FreeLibrary(temp);
			return true;
		#else
			return false;
		#endif
	}
	virtual void WIFI_Host_ShutdownSystem() 
	{
		#ifdef EXPERIMENTAL_WIFI_COMM 	 
			WSACleanup(); 	 
		#endif
	}

	virtual bool AVI_IsRecording()
	{
		return ::AVI_IsRecording();
	}

	virtual bool WAV_IsRecording()
	{
		return ::WAV_IsRecording();
	}

	virtual void USR_InfoMessage(const char *message)
	{
		LOG("%s\n", message);
		osd->addLine(message);
	}

	virtual void AVI_SoundUpdate(void* soundData, int soundLen) { 
		::DRV_AviSoundUpdate(soundData, soundLen);
	}

	virtual void USR_SetDisplayPostpone(int milliseconds, bool drawNextFrame)
	{
		displayPostponeType = milliseconds;
		displayPostponeUntil = timeGetTime() + milliseconds;
		displayNoPostponeNext |= drawNextFrame;
	}

	virtual void USR_RefreshScreen()
	{
		// postpone updates except this one for at least 0.5 seconds
		displayNoPostponeNext = true;
		if(displayPostponeType >= 0)
		{
			static const int minPostponeTime = 500;
			DWORD time_now = timeGetTime();
			if(displayPostponeType == 0 || (int)displayPostponeUntil - (int)time_now < minPostponeTime)
				displayPostponeUntil = time_now + minPostponeTime;
			if(displayPostponeType == 0)
				displayPostponeType = minPostponeTime;
		}

		Display();

		// in multi-core mode now the display thread will probably
		// wait for an invocation in this thread to happen,
		// so handle that ASAP
		if(!CommonSettings.single_core())
		{
			ResetEvent(display_invoke_ready_event);
			SetEvent(display_wakeup_event);
			if(AnyLuaActive())
				_ServiceDisplayThreadInvocation();
		}
	}

	virtual void EMU_PauseEmulation(bool pause)
	{
		if(pause)
			Pause();
		else
			Unpause();
	}

	virtual bool EMU_IsEmulationPaused()
	{
		return emu_paused!=0;
	}

	virtual bool EMU_IsFastForwarding()
	{
		return FastForward!=0;
	}

	virtual bool EMU_HasEmulationStarted()
	{
		return romloaded;
	}

	virtual bool EMU_IsAtFrameBoundary()
	{
		return inFrameBoundary;
	}

	virtual eStepMainLoopResult EMU_StepMainLoop(bool allowSleep, bool allowPause, int frameSkip, bool disableUser, bool disableCore)
	{
		// this bit is here to handle calls through LUACALL_BEFOREEMULATION and in case Lua errors out while we're processing input
		struct Scope{ bool m_on;
			Scope() : m_on(NDS_isProcessingUserInput()) { if(m_on) NDS_suspendProcessingInput(true); }
			~Scope() { if(m_on || NDS_isProcessingUserInput()) NDS_suspendProcessingInput(false); }
		} scope;

		ServiceDisplayThreadInvocations();

		CheckMessages();

		if(!romloaded)
			return ESTEP_DONE;

		if(!execute && allowPause)
		{
			StepRunLoop_Paused();
			return ESTEP_CALL_AGAIN;
		}

		if(!disableCore)
		{
			StepRunLoop_Throttle(allowSleep, frameSkip);
			StepRunLoop_Core();
		}

		if(!disableUser)
			StepRunLoop_User();

		return ESTEP_DONE;
	}

	virtual void DEBUG_UpdateIORegView(eDebug_IOReg category)
	{
		extern bool anyLiveIORegViews;
		if(anyLiveIORegViews)
			RefreshAllIORegViews();
	}
};

std::string GetPrivateProfileStdString(LPCSTR lpAppName,LPCSTR lpKeyName,LPCSTR lpDefault)
{
	static char buf[65536];
	GetPrivateProfileString(lpAppName, lpKeyName, lpDefault, buf, 65536, IniName);
	return buf;
}

static void RefreshMicSettings()
{
	Mic_DeInit_Physical();
	if(CommonSettings.micMode == TCommonSettings::Sample)
	{
		if(!LoadSample(MicSampleName))
		{
			MessageBox(NULL, "Unable to read the mic sample", "DeSmuME", (MB_OK | MB_ICONEXCLAMATION));
		}
	}
	else
	{
		LoadSample(NULL);
		if(CommonSettings.micMode == TCommonSettings::Physical)
		{
			Mic_Init_Physical();
		}
	}
}

#define GPU3D_NULL_SAVED -1
#define GPU3D_DEFAULT  GPU3D_OPENGL

DWORD wmTimerRes;
int _main()
{
	//7zup initialization
	InitDecoder();

#ifdef HAVE_WX
	wxInitialize();
#endif

	driver = new WinDriver();

	InitializeCriticalSection(&win_execute_sync);
	InitializeCriticalSection(&win_backbuffer_sync);
	InitializeCriticalSection(&display_invoke_handler_cs);
	display_invoke_ready_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	display_invoke_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	display_wakeup_event = CreateEvent(NULL, FALSE, FALSE, NULL);

#ifdef GDB_STUB
	gdbstub_handle_t arm9_gdb_stub;
	gdbstub_handle_t arm7_gdb_stub;
	struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
	struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
	struct armcpu_ctrl_iface *arm9_ctrl_iface;
	struct armcpu_ctrl_iface *arm7_ctrl_iface;
#endif
//	struct configured_features my_config;

	extern bool windows_opengl_init();
	oglrender_init = windows_opengl_init;


	//try and detect this for users who don't specify it on the commandline
	//(can't say I really blame them)
	//this helps give a substantial speedup for singlecore users
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	CommonSettings.num_cores = systemInfo.dwNumberOfProcessors;

	_cheatsDisabled = false;

	char text[80];

	GetINIPath();

	addon_type = GetPrivateProfileInt("GBAslot", "type", NDS_ADDON_NONE, IniName);
	win32_CFlash_cfgMode = GetPrivateProfileInt("GBAslot.CFlash", "fileMode", 2, IniName);
	win32_CFlash_cfgDirectory = GetPrivateProfileStdString("GBAslot.CFlash", "path", "");
	win32_CFlash_cfgFileName = GetPrivateProfileStdString("GBAslot.CFlash", "filename", "");
	GetPrivateProfileString("GBAslot.GBAgame", "filename", "", GBAgameName, MAX_PATH, IniName);

	WIN_InstallCFlash();

	ColorCtrl_Register();
	if (!RegWndClass("DeSmuME", WindowProcedure, CS_DBLCLKS, LoadIcon(hAppInst, MAKEINTRESOURCE(ICONDESMUME))))
	{
		MessageBox(NULL, "Error registering windows class", "DeSmuME", MB_OK);
		exit(-1);
	}

	u32 style = DWS_NORMAL;
	if(GetPrivateProfileBool("Video","Window Always On Top", false, IniName)) style |= DWS_ALWAYSONTOP;
	if(GetPrivateProfileBool("Video","Window Lockdown", false, IniName)) style |= DWS_LOCKDOWN;

	windowSize = GetPrivateProfileInt("Video","Window Size", 0, IniName);
	video.rotation =  GetPrivateProfileInt("Video","Window Rotate", 0, IniName);
	video.rotation_userset =  GetPrivateProfileInt("Video","Window Rotate Set", video.rotation, IniName);
	ForceRatio = GetPrivateProfileBool("Video","Window Force Ratio", 1, IniName);
	WndX = GetPrivateProfileInt("Video","WindowPosX", CW_USEDEFAULT, IniName);
	WndY = GetPrivateProfileInt("Video","WindowPosY", CW_USEDEFAULT, IniName);
	if(WndX < -10000) WndX = CW_USEDEFAULT; // fix for missing window problem
	if(WndY < -10000) WndY = CW_USEDEFAULT; // (happens if you close desmume while it's minimized)
	video.width = GetPrivateProfileInt("Video", "Width", 256, IniName);
	video.height = GetPrivateProfileInt("Video", "Height", 384, IniName);
	video.layout_old = video.layout = GetPrivateProfileInt("Video", "LCDsLayout", 0, IniName);
	if (video.layout > 2)
	{
		video.layout = video.layout_old = 0;
	}
	video.swap = GetPrivateProfileInt("Video", "LCDsSwap", 0, IniName);
	
	CommonSettings.hud.FpsDisplay = GetPrivateProfileBool("Display","Display Fps", false, IniName);
	CommonSettings.hud.FrameCounterDisplay = GetPrivateProfileBool("Display","FrameCounter", false, IniName);
	CommonSettings.hud.ShowInputDisplay = GetPrivateProfileBool("Display","Display Input", false, IniName);
	CommonSettings.hud.ShowGraphicalInputDisplay = GetPrivateProfileBool("Display","Display Graphical Input", false, IniName);
	CommonSettings.hud.ShowLagFrameCounter = GetPrivateProfileBool("Display","Display Lag Counter", false, IniName);
	CommonSettings.hud.ShowMicrophone = GetPrivateProfileBool("Display","Display Microphone", false, IniName);

	CommonSettings.micMode = (TCommonSettings::MicMode)GetPrivateProfileInt("MicSettings", "MicMode", (int)TCommonSettings::InternalNoise, IniName);
	GetPrivateProfileString("MicSettings", "MicSampleFile", "micsample.raw", MicSampleName, MAX_PATH, IniName);
	RefreshMicSettings();
	
	video.screengap = GetPrivateProfileInt("Display", "ScreenGap", 0, IniName);
	SeparationBorderDrag = GetPrivateProfileBool("Display", "Window Split Border Drag", true, IniName);
	ScreenGapColor = GetPrivateProfileInt("Display", "ScreenGapColor", 0xFFFFFF, IniName);
	FrameLimit = GetPrivateProfileBool("FrameLimit", "FrameLimit", true, IniName);
	CommonSettings.showGpu.main = GetPrivateProfileInt("Display", "MainGpu", 1, IniName) != 0;
	CommonSettings.showGpu.sub = GetPrivateProfileInt("Display", "SubGpu", 1, IniName) != 0;
	lostFocusPause = GetPrivateProfileBool("Focus", "BackgroundPause", false, IniName);
	
	//Get Ram-Watch values
	RWSaveWindowPos = GetPrivateProfileBool("RamWatch", "SaveWindowPos", false, IniName);
	ramw_x = GetPrivateProfileInt("RamWatch", "RWWindowPosX", 0, IniName);
	ramw_y = GetPrivateProfileInt("RamWatch", "RWWindowPosY", 0, IniName);
	AutoRWLoad = GetPrivateProfileBool("RamWatch", "Auto-load", false, IniName);
	for(int i = 0; i < MAX_RECENT_WATCHES; i++)
	{
		char str[256];
		sprintf(str, "Recent Watch %d", i+1);
		GetPrivateProfileString("Watches", str, "", &rw_recent_files[i][0], 1024, IniName);
	}


	for(int i = 0; i < MAX_RECENT_SCRIPTS; i++)
	{
		char str[256];
		sprintf(str, "Recent Lua Script %d", i+1);
		GetPrivateProfileString("Scripting", str, "", &Recent_Scripts[i][0], 1024, IniName);
	}


	//i think we should override the ini file with anything from the commandline
	CommandLine cmdline;
	cmdline.loadCommonOptions();
	if(!cmdline.parse(__argc,__argv)) {
		cmdline.errorHelp(__argv[0]);
		return 1;
	}

	Desmume_InitOnce();

	//in case this isnt actually a singlecore system, but the user requested it
	//then restrict ourselves to one core
	if(CommonSettings.single_core())
		SetProcessAffinityMask(GetCurrentProcess(),1);

	MainWindow = new WINCLASS(CLASSNAME, hAppInst);
	if (!MainWindow->create((char*)EMU_DESMUME_NAME_AND_VERSION(), WndX, WndY, video.width,video.height+video.screengap,
		WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
		NULL))
	{
		MessageBox(NULL, "Error creating main window", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	//disable wacky stylus stuff
	GlobalAddAtom("MicrosoftTabletPenServiceProperty");
	SetProp(MainWindow->getHWnd(),"MicrosoftTabletPenServiceProperty",(HANDLE)1);

	gpu_SetRotateScreen(video.rotation);

	//default the firmware settings, they may get changed later
	NDS_FillDefaultFirmwareConfigData( &win_fw_config);

	GetPrivateProfileString("General", "Language", "0", text, 80, IniName);
	SetLanguage(atoi(text));

	//hAccel = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_MAIN_ACCEL)); //Now that we have a hotkey system we down need the Accel table.  Not deleting just yet though

	if(MenuInit() == 0)
	{
		MessageBox(NULL, "Error creating main menu", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	SetStyle(style);

	SetMinWindowSize();

	ScaleScreen(windowSize);

	DragAcceptFiles(MainWindow->getHWnd(), TRUE);

#ifdef EXPERIMENTAL_WIFI_COMM
	EnableMenuItem(mainMenu, IDM_WIFISETTINGS, MF_ENABLED);
#endif


	InitCustomKeys(&CustomKeys);
	Hud.reset();

	void input_init();
	input_init();

	if (addon_type == NDS_ADDON_GUITARGRIP) Guitar.Enabled = true;

	LOG("Init NDS\n");

	GInfo_Init();

	SoundView_Init();

	ViewDisasm_ARM7 = new TOOLSCLASS(hAppInst, IDD_DESASSEMBLEUR_VIEWER7, (DLGPROC) ViewDisasm_ARM7Proc);
	ViewDisasm_ARM9 = new TOOLSCLASS(hAppInst, IDD_DESASSEMBLEUR_VIEWER9, (DLGPROC) ViewDisasm_ARM9Proc);
	ViewPalette = new TOOLSCLASS(hAppInst, IDD_PAL, (DLGPROC) ViewPalProc);
	ViewTiles = new TOOLSCLASS(hAppInst, IDD_TILE, (DLGPROC) ViewTilesProc);
	ViewMaps = new TOOLSCLASS(hAppInst, IDD_MAP, (DLGPROC) ViewMapsProc);
	ViewOAM = new TOOLSCLASS(hAppInst, IDD_OAM, (DLGPROC) ViewOAMProc);
	ViewMatrices = new TOOLSCLASS(hAppInst, IDD_MATRIX_VIEWER, (DLGPROC) ViewMatricesProc);
	ViewLights = new TOOLSCLASS(hAppInst, IDD_LIGHT_VIEWER, (DLGPROC) ViewLightsProc);


	cmdline.process_addonCommands();
	if(cmdline.is_cflash_configured)
	{
	    addon_type = NDS_ADDON_CFLASH;
		//push the commandline-provided options into the current config slots
		if(CFlash_Mode == ADDON_CFLASH_MODE_Path)
			win32_CFlash_cfgDirectory = CFlash_Path;
		else 
			win32_CFlash_cfgFileName = CFlash_Path;
	}

	if(cmdline.gbaslot_rom != "")
	{
		addon_type = NDS_ADDON_GBAGAME;
		strcpy(GBAgameName, cmdline.gbaslot_rom.c_str());
	}

	switch (addon_type)
	{
	case NDS_ADDON_NONE:
		break;
	case NDS_ADDON_CFLASH:
		break;
	case NDS_ADDON_RUMBLEPAK:
		break;
	case NDS_ADDON_GBAGAME:
		if (!strlen(GBAgameName))
		{
			addon_type = NDS_ADDON_NONE;
			break;
		}
		// TODO: check for file exist
		break;
	case NDS_ADDON_GUITARGRIP:
		break;
	case NDS_ADDON_EXPMEMORY:
		break;
	default:
		addon_type = NDS_ADDON_NONE;
		break;
	}
	addonsChangePak(addon_type);

	CommonSettings.wifi.mode = GetPrivateProfileInt("Wifi", "Mode", 0, IniName);
	CommonSettings.wifi.infraBridgeAdapter = GetPrivateProfileInt("Wifi", "BridgeAdapter", 0, IniName);

#ifdef GDB_STUB
	if ( cmdline.arm9_gdb_port != 0) {
		arm9_gdb_stub = createStub_gdb( cmdline.arm9_gdb_port,
			&arm9_memio, &arm9_direct_memory_iface);

		if ( arm9_gdb_stub == NULL) {
			MessageBox(MainWindow->getHWnd(),"Failed to create ARM9 gdbstub","Error",MB_OK);
			return -1;
		}
	}
	if ( cmdline.arm7_gdb_port != 0) {
		arm7_gdb_stub = createStub_gdb( cmdline.arm7_gdb_port,
			&arm7_memio,
			&arm7_base_memory_iface);

		if ( arm7_gdb_stub == NULL) {
			MessageBox(MainWindow->getHWnd(),"Failed to create ARM7 gdbstub","Error",MB_OK);
			return -1;
		}
	}

	NDS_Init( arm9_memio, &arm9_ctrl_iface,
		arm7_memio, &arm7_ctrl_iface);
#else
	NDS_Init ();
#endif

	osd->singleScreen = (video.layout == 2);

	/*
	* Activate the GDB stubs
	* This has to come after the NDS_Init where the cpus are set up.
	*/
#ifdef GDB_STUB
	if ( cmdline.arm9_gdb_port != 0) {
		activateStub_gdb( arm9_gdb_stub, arm9_ctrl_iface);
	}
	if ( cmdline.arm7_gdb_port != 0) {
		activateStub_gdb( arm7_gdb_stub, arm7_ctrl_iface);
	}
#endif
	
	GetPrivateProfileString("General", "Language", "0", text, 80, IniName);	
	CheckLanguage(IDC_LANGENGLISH+atoi(text));

	GetPrivateProfileString("Video", "FrameSkip", "AUTO0", text, 80, IniName);

	if(!strncmp(text, "AUTO", 4))
	{
		autoframeskipenab=1;
		if(!text[4])
			frameskiprate=9;
		else
			frameskiprate=atoi(text+4);
		if(frameskiprate < 0)
			frameskiprate = 9;
	}
	else
	{
		autoframeskipenab=0;
		frameskiprate=atoi(text);
	}

	if (KeyInDelayMSec == 0) {
		DWORD dwKeyboardDelay;
		SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &dwKeyboardDelay, 0);
		KeyInDelayMSec = 250 * (dwKeyboardDelay + 1);
	}
	if (KeyInRepeatMSec == 0) {
		DWORD dwKeyboardSpeed;
		SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &dwKeyboardSpeed, 0);
		KeyInRepeatMSec = (int)(1000.0/(((30.0-2.5)/31.0)*dwKeyboardSpeed+2.5));
	}
	if (KeyInRepeatMSec < (int)wmTimerRes)
		KeyInRepeatMSec = (int)wmTimerRes;
	if (KeyInDelayMSec < KeyInRepeatMSec)
		KeyInDelayMSec = KeyInRepeatMSec;

	hKeyInputTimer = timeSetEvent (KeyInRepeatMSec, 0, KeyInputTimer, 0, TIME_PERIODIC);

	cur3DCore = GetPrivateProfileInt("3D", "Renderer", GPU3D_DEFAULT, IniName);
	if(cur3DCore == GPU3D_NULL_SAVED)
		cur3DCore = GPU3D_NULL;
	else if(cur3DCore == GPU3D_NULL) // this value shouldn't be saved anymore
		cur3DCore = GPU3D_DEFAULT;
	CommonSettings.GFX3D_HighResolutionInterpolateColor = GetPrivateProfileBool("3D", "HighResolutionInterpolateColor", 1, IniName);
	CommonSettings.GFX3D_EdgeMark = GetPrivateProfileBool("3D", "EnableEdgeMark", 1, IniName);
	CommonSettings.GFX3D_Fog = GetPrivateProfileBool("3D", "EnableFog", 1, IniName);
	//CommonSettings.gfx3d_flushMode = GetPrivateProfileInt("3D", "AlternateFlush", 0, IniName);
	Change3DCoreWithFallbackAndSave(cur3DCore);

#ifdef BETA_VERSION
	EnableMenuItem (mainMenu, IDM_SUBMITBUGREPORT, MF_GRAYED);
#endif
	LOG("Init sound core\n");
	sndcoretype = GetPrivateProfileInt("Sound","SoundCore2", SNDCORE_DIRECTX, IniName);
	sndbuffersize = GetPrivateProfileInt("Sound","SoundBufferSize2", DESMUME_SAMPLE_RATE*8/60, IniName);
	CommonSettings.spuInterpolationMode = (SPUInterpolationMode)GetPrivateProfileInt("Sound","SPUInterpolation", 1, IniName);

	EnterCriticalSection(&win_execute_sync);
	int spu_ret = SPU_ChangeSoundCore(sndcoretype, sndbuffersize);
	LeaveCriticalSection(&win_execute_sync);
	if(spu_ret != 0)
	{
		MessageBox(MainWindow->getHWnd(),"Unable to initialize DirectSound","Error",MB_OK);
		return -1;
	}

	sndvolume = GetPrivateProfileInt("Sound","Volume",100, IniName);
	SPU_SetVolume(sndvolume);

	snd_synchmode = GetPrivateProfileInt("Sound","SynchMode",0,IniName);
	snd_synchmethod = GetPrivateProfileInt("Sound","SynchMethod",0,IniName);
	{
		Lock lock;
		SPU_SetSynchMode(snd_synchmode,snd_synchmethod);
	}

	CommonSettings.DebugConsole = GetPrivateProfileBool("Emulation", "DebugConsole", FALSE, IniName);
	CommonSettings.EnsataEmulation = GetPrivateProfileBool("Emulation", "EnsataEmulation", FALSE, IniName);
	CommonSettings.UseExtBIOS = GetPrivateProfileBool("BIOS", "UseExtBIOS", FALSE, IniName);
	GetPrivateProfileString("BIOS", "ARM9BIOSFile", "bios9.bin", CommonSettings.ARM9BIOS, 256, IniName);
	GetPrivateProfileString("BIOS", "ARM7BIOSFile", "bios7.bin", CommonSettings.ARM7BIOS, 256, IniName);
	CommonSettings.SWIFromBIOS = GetPrivateProfileBool("BIOS", "SWIFromBIOS", FALSE, IniName);

	CommonSettings.UseExtFirmware = GetPrivateProfileBool("Firmware", "UseExtFirmware", FALSE, IniName);
	GetPrivateProfileString("Firmware", "FirmwareFile", "firmware.bin", CommonSettings.Firmware, 256, IniName);
	CommonSettings.BootFromFirmware = GetPrivateProfileBool("Firmware", "BootFromFirmware", FALSE, IniName);

	video.currentfilter = GetPrivateProfileInt("Video", "Filter", video.NONE, IniName);
	FilterUpdate(MainWindow->getHWnd(),false);

	/* Read the firmware settings from the init file */
	win_fw_config.fav_colour = GetPrivateProfileInt("Firmware","favColor", 10, IniName);
	win_fw_config.birth_month = GetPrivateProfileInt("Firmware","bMonth", 7, IniName);
	win_fw_config.birth_day = GetPrivateProfileInt("Firmware","bDay", 15, IniName);
	win_fw_config.language = GetPrivateProfileInt("Firmware","Language", 1, IniName);

	{
		/*
		* Read in the nickname and message.
		* Convert the strings into Unicode UTF-16 characters.
		*/
		char temp_str[27];
		int char_index;
		GetPrivateProfileString("Firmware","nickName", "yopyop", temp_str, 11, IniName);
		win_fw_config.nickname_len = strlen( temp_str);

		if ( win_fw_config.nickname_len == 0) {
			strcpy( temp_str, "yopyop");
			win_fw_config.nickname_len = strlen( temp_str);
		}

		for ( char_index = 0; char_index < win_fw_config.nickname_len; char_index++) {
			win_fw_config.nickname[char_index] = temp_str[char_index];
		}

		GetPrivateProfileString("Firmware","Message", "DeSmuME makes you happy!", temp_str, 27, IniName);
		win_fw_config.message_len = strlen( temp_str);
		for ( char_index = 0; char_index < win_fw_config.message_len; char_index++) {
			win_fw_config.message[char_index] = temp_str[char_index];
		}
	}

	// Create the dummy firmware
	NDS_CreateDummyFirmware( &win_fw_config);


	if (cmdline.nds_file != "")
	{
		if(OpenCore(cmdline.nds_file.c_str()))
		{
			romloaded = TRUE;
			if(!cmdline.start_paused)
				NDS_UnPause();
		}
	}

	cmdline.process_movieCommands();
	
	if(cmdline.load_slot != 0)
	{
		HK_StateLoadSlot(cmdline.load_slot, true);
	}

	MainWindow->Show(SW_NORMAL);


	//DEBUG TEST HACK
	//driver->VIEW3D_Init();
	//driver->view3d->Launch();
	//---------
	
	//------DO EVERYTHING
	run();


	//------SHUTDOWN

	KillDisplay();

	DRV_AviEnd();
	WAV_End();

	NDS_DeInit();

#ifdef DEBUG
	//LogStop();
#endif

	timeKillEvent(hKeyInputTimer);

	GInfo_DeInit();

	SoundView_DlgClose();
	//IORegView_DlgClose();

	SoundView_DeInit();

	//if (input!=NULL) delete input;
	if (ViewLights!=NULL) delete ViewLights;
	if (ViewMatrices!=NULL) delete ViewMatrices;
	if (ViewOAM!=NULL) delete ViewOAM;
	if (ViewMaps!=NULL) delete ViewMaps;
	if (ViewTiles!=NULL) delete ViewTiles;
	if (ViewPalette!=NULL) delete ViewPalette;
	if (ViewDisasm_ARM9!=NULL) delete ViewDisasm_ARM9;
	if (ViewDisasm_ARM7!=NULL) delete ViewDisasm_ARM7;

	CloseAllToolWindows();

	delete MainWindow;

	if (lpDDClipPrimary!=NULL) IDirectDraw7_Release(lpDDClipPrimary);
	if (lpPrimarySurface != NULL) IDirectDraw7_Release(lpPrimarySurface);
	if (lpBackSurface != NULL) IDirectDraw7_Release(lpBackSurface);
	if (lpDDraw != NULL) IDirectDraw7_Release(lpDDraw);

	UnregWndClass("DeSmuME");

	return 0;
}

int WINAPI WinMain (HINSTANCE hThisInstance,
					HINSTANCE hPrevInstance,
					LPSTR lpszArgument,
					int nFunsterStil)

{
	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS))== TIMERR_NOERROR)
	{
		wmTimerRes = std::min(std::max(tc.wPeriodMin, (UINT)1), tc.wPeriodMax);
		timeBeginPeriod (wmTimerRes);
	}
	else
	{
		wmTimerRes = 5;
		timeBeginPeriod (wmTimerRes);
	}

	g_thread_init (NULL);
	hAppInst=hThisInstance;
	OpenConsole();			// Init debug console

	int ret = _main();

	timeEndPeriod (wmTimerRes);

	CloseConsole();

	return ret;
}


void UpdateScreenRects()
{
	if (video.layout == 1)
	{
		// Main screen
		MainScreenSrcRect.left = 0;
		MainScreenSrcRect.top = 0;
		MainScreenSrcRect.right = video.width;
		MainScreenSrcRect.bottom = video.height/2;

		// Sub screen
		SubScreenSrcRect.left = 0;
		SubScreenSrcRect.top = video.height/2;
		SubScreenSrcRect.right = video.width;
		SubScreenSrcRect.bottom = video.height;
	}
	else
	if (video.layout == 2)
	{
		// Main screen
		MainScreenSrcRect.left = 0;
		MainScreenSrcRect.top = 0;
		MainScreenSrcRect.right = video.width;
		MainScreenSrcRect.bottom = video.height/2;

		// Sub screen
		SubScreenSrcRect.left = 0;
		SubScreenSrcRect.top = video.height/2;
		SubScreenSrcRect.right = video.width;
		SubScreenSrcRect.bottom = video.height;
	}
	else
	{
		if((video.rotation == 90) || (video.rotation == 270))
		{
			// Main screen
			MainScreenSrcRect.left = 0;
			MainScreenSrcRect.top = 0;
			MainScreenSrcRect.right = video.height/2;
			MainScreenSrcRect.bottom = video.width;

			// Sub screen
			SubScreenSrcRect.left = video.height/2;
			SubScreenSrcRect.top = 0;
			SubScreenSrcRect.right = video.height;
			SubScreenSrcRect.bottom = video.width;
		}
		else
		{
			// Main screen
			MainScreenSrcRect.left = 0;
			MainScreenSrcRect.top = 0;
			MainScreenSrcRect.right = video.width;
			MainScreenSrcRect.bottom = video.height/2;

			// Sub screen
			SubScreenSrcRect.left = 0;
			SubScreenSrcRect.top = video.height/2;
			SubScreenSrcRect.right = video.width;
			SubScreenSrcRect.bottom = video.height;
		}
	}
}

// re-run the aspect ratio calculations if enabled
void FixAspectRatio()
{
	if(windowSize != 0)
	{
		ScaleScreen(windowSize);
	}
	else if(ForceRatio)
	{
		RECT rc;
		GetWindowRect(MainWindow->getHWnd(), &rc);
		SendMessage(MainWindow->getHWnd(), WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&rc);
		MoveWindow(MainWindow->getHWnd(), rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
	}
}

void SetScreenGap(int gap)
{
	video.screengap = gap;
	SetMinWindowSize();
	FixAspectRatio();
	UpdateWndRects(MainWindow->getHWnd());
}

//========================================================================================
void SetRotate(HWND hwnd, int rot, bool user)
{
	Lock lock (win_backbuffer_sync);

	RECT rc;
	int oldrot = video.rotation;
	int oldheight, oldwidth;
	int newheight, newwidth;

	video.rotation = rot;

	GetClientRect(hwnd, &rc);
	oldwidth = (rc.right - rc.left);
	oldheight = (rc.bottom - rc.top);
	newwidth = oldwidth;
	newheight = oldheight;

	switch(oldrot)
	{
	case 0:
	case 180:
		{
			if((rot == 90) || (rot == 270))
			{
				newwidth = oldheight;
				newheight = oldwidth;
			}
		}
		break;

	case 90:
	case 270:
		{
			if((rot == 0) || (rot == 180))
			{
				newwidth = oldheight;
				newheight = oldwidth;
			}
		}
		break;
	}

	osd->setRotate(rot);

	SetMinWindowSize();

	MainWindow->setClientSize(newwidth, newheight);

	/* Recreate the DirectDraw back buffer */
	if (lpBackSurface!=NULL)
	{
		IDirectDrawSurface7_Release(lpBackSurface);

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize          = sizeof(ddsd);
		ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN;
		ddsd.dwWidth         = video.rotatedwidth();
		ddsd.dwHeight        = video.rotatedheight();

		IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpBackSurface, NULL);
	}

	WritePrivateProfileInt("Video","Window Rotate",video.rotation,IniName);
	if(user)
	{
		video.rotation_userset = video.rotation;
		WritePrivateProfileInt("Video","Window Rotate Set",video.rotation_userset,IniName);
	}

	gpu_SetRotateScreen(video.rotation);

	UpdateScreenRects();
}

void AviEnd()
{
	NDS_Pause();
	DRV_AviEnd();
	NDS_UnPause();
}

//Shows an Open File menu and starts recording an AVI
void AviRecordTo()
{
	NDS_Pause();

	OPENFILENAME ofn;

	////if we are playing a movie, construct the filename from the current movie.
	////else construct it from the filename.
	//if(FCEUMOV_Mode(MOVIEMODE_PLAY|MOVIEMODE_RECORD))
	//{
	//	extern char curMovieFilename[];
	//	strcpy(szChoice, curMovieFilename);
	//	char* dot = strrchr(szChoice,'.');

	//	if (dot)
	//	{
	//		*dot='\0';
	//	}

	//	strcat(szChoice, ".avi");
	//}
	//else
	//{
	//	extern char FileBase[];
	//	sprintf(szChoice, "%s.avi", FileBase);
	//}

	// avi record file browser
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = MainWindow->getHWnd();
	ofn.lpstrFilter = "AVI Files (*.avi)\0*.avi\0\0";
	ofn.lpstrDefExt = "avi";
	ofn.lpstrTitle = "Save AVI as";

	char folder[MAX_PATH];
	ZeroMemory(folder, sizeof(folder));
	path.getpath(path.AVI_FILES, folder);

	char file[MAX_PATH];
	ZeroMemory(file, sizeof(file));
	path.formatname(file);

	strcat(folder, file);
	int len = strlen(folder);
	if(len > MAX_PATH - 4)
		folder[MAX_PATH - 4] = '\0';
	
	strcat(folder, ".avi");
	ofn.lpstrFile = folder;


	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

	if(GetSaveFileName(&ofn))
	{
		DRV_AviBegin(folder);
	}

	NDS_UnPause();
}

void WavEnd()
{
	NDS_Pause();
	WAV_End();
	NDS_UnPause();
}

//Shows an Open File menu and starts recording an WAV
void WavRecordTo(int wavmode)
{
	NDS_Pause();

	OPENFILENAME ofn;
	char szChoice[MAX_PATH] = {0};

	////if we are playing a movie, construct the filename from the current movie.
	////else construct it from the filename.
	//if(FCEUMOV_Mode(MOVIEMODE_PLAY|MOVIEMODE_RECORD))
	//{
	//	extern char curMovieFilename[];
	//	strcpy(szChoice, curMovieFilename);
	//	char* dot = strrchr(szChoice,'.');

	//	if (dot)
	//	{
	//		*dot='\0';
	//	}

	//	strcat(szChoice, ".wav");
	//}
	//else
	//{
	//	extern char FileBase[];
	//	sprintf(szChoice, "%s.wav", FileBase);
	//}

	// wav record file browser
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = MainWindow->getHWnd();
	ofn.lpstrFilter = "WAV Files (*.wav)\0*.wav\0\0";
	ofn.lpstrFile = szChoice;
	ofn.lpstrDefExt = "wav";
	ofn.lpstrTitle = "Save WAV as";

	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

	if(GetSaveFileName(&ofn))
	{
		WAV_Begin(szChoice, (WAVMode)wavmode);
	}

	NDS_UnPause();
}

static BOOL OpenCore(const char* filename)
{
	char LogicalName[1024], PhysicalName[1024];

	const char* s_nonRomExtensions [] = {"txt", "nfo", "htm", "html", "jpg", "jpeg", "png", "bmp", "gif", "mp3", "wav", "lnk", "exe", "bat", "gmv", "gm2", "lua", "luasav", "sav", "srm", "brm", "cfg", "wch", "gs*", "dst"};

	if(!ObtainFile(filename, LogicalName, PhysicalName, "rom", s_nonRomExtensions, ARRAY_SIZE(s_nonRomExtensions)))
		return FALSE;

	StopAllLuaScripts();

	if(LoadROM(filename, LogicalName))
	{
		romloaded = TRUE;
		Unpause();
		return TRUE;
	}
	else return FALSE;
}

LRESULT OpenFile()
{
	HWND hwnd = MainWindow->getHWnd();

	int filterSize = 0, i = 0;
	OPENFILENAME ofn;
	char filename[MAX_PATH] = "",
		fileFilter[512]="";
	NDS_Pause(); //Stop emulation while opening new rom

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;

	ofn.lpstrFilter = 
		"All Usable Files (*.nds, *.ds.gba, *.zip, *.7z, *.rar, *.bz2)\0*.nds;*.ds.gba;*.zip;*.7z;*.rar;*.bz2\0"
		"NDS ROM file (*.nds)\0*.nds\0"
		"NDS/GBA ROM File (*.ds.gba)\0*.ds.gba\0"
		"Zipped NDS ROM file (*.zip)\0*.zip\0"
		"7Zipped NDS ROM file (*.7z)\0*.7z\0"
		"RARed NDS ROM file (*.rar)\0*.rar\0"
		"BZipped NDS ROM file (*.bz2)\0*.bz2\0"
		"Any file (*.*)\0*.*\0"
		"\0"
		; //gzip doesnt actually work right now

	ofn.nFilterIndex = 1;
	ofn.lpstrFile =  filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "nds";
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

	char buffer[MAX_PATH];
	ZeroMemory(buffer, sizeof(buffer));
	path.getpath(path.ROMS, buffer);
	ofn.lpstrInitialDir = buffer;

	if (GetOpenFileName(&ofn) == NULL)
	{
		NDS_UnPause();
		return 0;
	}
	else
	{
		if(path.savelastromvisit)
		{
			char *lchr, buffer[MAX_PATH];
			ZeroMemory(buffer, sizeof(buffer));

			lchr = strrchr(filename, '\\');
			strncpy(buffer, filename, strlen(filename) - strlen(lchr));
			
			path.setpath(path.ROMS, buffer);
			WritePathSettings();
		}
	}

	if(!OpenCore(filename))
		return 0;

//	if(!GetOpenFileName(&ofn))
//	{
//		if (romloaded)
//		{
//			CheatsSearchReset();
//			NDS_UnPause(); //Restart emulation if no new rom chosen
//		}
//		return 0;
//	}



	return 0;
}

void CloseRom()
{
	StopAllLuaScripts();
//	cheatsSearchClose();
	NDS_FreeROM();
	romloaded = false;
	execute = false;
	Hud.resetTransient();
	NDS_Reset();
}

int GetInitialModifiers(int key) // async version for input thread
{
	if (key == VK_MENU || key == VK_CONTROL || key == VK_SHIFT)
		return CUSTKEY_NONE_MASK;

	int modifiers = 0;
	if(GetAsyncKeyState(VK_MENU   )&0x8000) modifiers |= CUSTKEY_ALT_MASK;
	if(GetAsyncKeyState(VK_CONTROL)&0x8000) modifiers |= CUSTKEY_CTRL_MASK;
	if(GetAsyncKeyState(VK_SHIFT  )&0x8000) modifiers |= CUSTKEY_SHIFT_MASK;
	if(!modifiers)                          modifiers |= CUSTKEY_NONE_MASK;
	return modifiers;
}
int GetModifiers(int key)
{
	if (key == VK_MENU || key == VK_CONTROL || key == VK_SHIFT)
		return 0;

	int bakedModifiers = (key >> 8) & (CUSTKEY_ALT_MASK | CUSTKEY_CTRL_MASK | CUSTKEY_SHIFT_MASK | CUSTKEY_NONE_MASK);
	if(bakedModifiers)
		return bakedModifiers & ~CUSTKEY_NONE_MASK;

	int modifiers = 0;
	if(GetKeyState(VK_MENU   )&0x80) modifiers |= CUSTKEY_ALT_MASK;
	if(GetKeyState(VK_CONTROL)&0x80) modifiers |= CUSTKEY_CTRL_MASK;
	if(GetKeyState(VK_SHIFT  )&0x80) modifiers |= CUSTKEY_SHIFT_MASK;
	return modifiers;
}
int PurgeModifiers(int key)
{
	return key & ~((CUSTKEY_ALT_MASK | CUSTKEY_CTRL_MASK | CUSTKEY_SHIFT_MASK | CUSTKEY_NONE_MASK) << 8);
}

int HandleKeyUp(WPARAM wParam, LPARAM lParam, int modifiers)
{
	SCustomKey *key = &CustomKeys.key(0);

	while (!IsLastCustomKey(key)) {
		// the modifiers check here was disabled to fix the following problem:
		// hold tab, hold alt, release tab, release alt -> fast forward gets stuck on
		if (key->handleKeyUp && wParam == key->key /*&& modifiers == key->modifiers*/) {
			key->handleKeyUp(key->param);
		}
		key++;
	}

	return 1;
}

int HandleKeyMessage(WPARAM wParam, LPARAM lParam, int modifiers)
{
	//some crap from snes9x I dont understand with toggles and macros...

	bool hitHotKey = false;

	if(!(wParam == 0 || wParam == VK_ESCAPE)) // if it's the 'disabled' key, it's never pressed as a hotkey
	{
		SCustomKey *key = &CustomKeys.key(0);
		while (!IsLastCustomKey(key)) {
			if (wParam == key->key && modifiers == key->modifiers && key->handleKeyDown) {
				key->handleKeyDown(key->param, lParam & 0x40000000 ? false : true);
				hitHotKey = true;
			}
			key++;
		}

		// don't pull down menu with Alt or F10 if it is a hotkey, unless no game is running
		if(romloaded && ((wParam == VK_MENU || wParam == VK_F10) && (hitHotKey) && !GetAsyncKeyState(VK_F4)))
			return 0;

		return 1;
	}

	return 1;
}

void Unpause()
{
	lastPauseFromLostFocus = FALSE;
	if (emu_paused && autoframeskipenab && frameskiprate) AutoFrameSkip_IgnorePreviousDelay();
	if (!execute && !emu_paused) NDS_Pause(false), emu_paused=true;
	if (emu_paused) NDS_UnPause();
	emu_paused = 0;
}

void Pause()
{
	lastPauseFromLostFocus = FALSE;
	if (execute && emu_paused) NDS_UnPause(false), emu_paused=false;
	if (!emu_paused) NDS_Pause();
	emu_paused = 1;
}

void TogglePause()
{
	lastPauseFromLostFocus = FALSE;
	if (emu_paused && autoframeskipenab && frameskiprate) AutoFrameSkip_IgnorePreviousDelay();
	if (emu_paused) NDS_UnPause();
	else NDS_Pause();
	emu_paused ^= 1;
}


bool first = true;

void FrameAdvance(bool state)
{
	continuousframeAdvancing = false;
	if(!romloaded)
		return;
	if(state) {
		if(first) {
			// frame advance button newly pressed
			first = false;
			if(!emu_paused) {
				// if not paused, pause immediately and don't advance yet
				Pause();
			} else {
				// if already paused, execute for 1 frame
				execute = TRUE;
				frameAdvance = true;
			}
			// this seems to reduce the average recovery time (by about 1 frame)
			// when switching from frameskipping to frameadvance.
			// we could pass 1 in to force it to happen even earlier
			// but that can result in 2d and 3d looking out of sync for 1 frame.
			NDS_OmitFrameSkip();
		} else {
			// frame advance button still held down,
			// start or continue executing at normal speed
			Unpause();
			frameAdvance = false;
			continuousframeAdvancing = true;
		}
	} else {
		// frame advance button released
		first = true;
		frameAdvance = false;

		// pause immediately
		Pause();
	}
}

//-----------------------------------------------------------------------------
//   Put screenshot in clipboard
//-----------------------------------------------------------------------------

void ScreenshotToClipboard()
{
	const char* nameandver = EMU_DESMUME_NAME_AND_VERSION();
	bool twolinever = strlen(nameandver) > 32;

	HFONT hFont = CreateFont(14, 8, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "Lucida Console");

	HDC hScreenDC = GetDC(NULL);
	HDC hMemDC = CreateCompatibleDC(hScreenDC);
	HBITMAP hMemBitmap = CreateCompatibleBitmap(hScreenDC, 256, 384 + (14 * (twolinever ? 6:5)));
	HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hMemBitmap);
	HFONT hOldFont = (HFONT)SelectObject(hMemDC, hFont);

	RECT rc; SetRect(&rc, 0, 0, 256, 384 + (14 * (twolinever ? 6:5)));

	BITMAPV4HEADER bmi;
	memset(&bmi, 0, sizeof(bmi));
    bmi.bV4Size = sizeof(bmi);
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = 16;
    bmi.bV4V4Compression = BI_RGB | BI_BITFIELDS;
    bmi.bV4RedMask = 0x001F;
    bmi.bV4GreenMask = 0x03E0;
    bmi.bV4BlueMask = 0x7C00;
    bmi.bV4Width = 256;
    bmi.bV4Height = -384;

	FillRect(hMemDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	SetDIBitsToDevice(hMemDC, 0, 0, 256, 384, 0, 0, 0, 384, &GPU_screen[0], (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
	
	SetBkColor(hMemDC, RGB(255, 255, 255));
	SetTextColor(hMemDC, RGB(64, 64, 130));

	if (twolinever)
	{
		int i;
		for (i = 31; i > 0; i--)
			if (nameandver[i] == ' ')
				break;

		TextOut(hMemDC, 0, 384 + 14, &nameandver[0], i+1);
		TextOut(hMemDC, 8, 384 + 14*2, &nameandver[i+1], strlen(nameandver) - (i+1));
	}
	else
		TextOut(hMemDC, 0, 384 + 14, nameandver, strlen(nameandver));

	char str[32];
	memcpy(&str[0], &MMU.CART_ROM[0], 12); str[12] = '\0';
	int titlelen = strlen(str); 
	str[titlelen] = ' ';
	memcpy(&str[titlelen+1], &MMU.CART_ROM[12], 6); str[titlelen+1+6] = '\0';
	TextOut(hMemDC, 8, 384 + 14 * (twolinever ? 3:2), str, strlen(str));

	sprintf(str, "FPS: %i/%i | %s", mainLoopData.fps, mainLoopData.fps3d, paused ? "Paused":"Running");
	TextOut(hMemDC, 8, 384 + 14 * (twolinever ? 4:3), str, strlen(str));

	sprintf(str, "3D Render: %s", core3DList[cur3DCore]->name);
	TextOut(hMemDC, 8, 384 + 14 * (twolinever ? 5:4), str, strlen(str));

	OpenClipboard(NULL);
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, hMemBitmap);
	CloseClipboard();

	SelectObject(hMemDC, hOldBitmap);
	SelectObject(hMemDC, hOldFont);
	ReleaseDC(NULL, hScreenDC);
	DeleteDC(hMemDC);

	DeleteObject(hMemBitmap);
	DeleteObject(hFont);
}

//-----------------------------------------------------------------------------
//   Config dialogs
//-----------------------------------------------------------------------------

enum CONFIGSCREEN
{
	CONFIGSCREEN_INPUT,
	CONFIGSCREEN_HOTKEY,
	CONFIGSCREEN_FIRMWARE,
	CONFIGSCREEN_WIFI,
	CONFIGSCREEN_SOUND,
	CONFIGSCREEN_EMULATION,
	CONFIGSCREEN_MICROPHONE,
	CONFIGSCREEN_PATHSETTINGS
};

void RunConfig(CONFIGSCREEN which) 
{
	HWND hwnd = MainWindow->getHWnd();
	bool tpaused=false;
	if (execute) 
	{
		tpaused=true;
		NDS_Pause();
	}

	switch(which)
	{
	case CONFIGSCREEN_INPUT:
		RunInputConfig();
		break;
	case CONFIGSCREEN_HOTKEY:
		RunHotkeyConfig();
		break;
	case CONFIGSCREEN_FIRMWARE:
		DialogBoxW(hAppInst,MAKEINTRESOURCEW(IDD_FIRMSETTINGS), hwnd, (DLGPROC) FirmConfig_Proc);
		break;
	case CONFIGSCREEN_SOUND:
		DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_SOUNDSETTINGS), hwnd, (DLGPROC)SoundSettingsDlgProc);
		break;
	case CONFIGSCREEN_EMULATION:
		DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_EMULATIONSETTINGS), hwnd, (DLGPROC)EmulationSettingsDlgProc);
		break; 
	case CONFIGSCREEN_MICROPHONE:
		DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_MICROPHONE), hwnd, (DLGPROC)MicrophoneSettingsDlgProc);
		break;
	case CONFIGSCREEN_PATHSETTINGS:
		DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_PATHSETTINGS), hwnd, (DLGPROC)PathSettingsDlgProc);
		break;
	case CONFIGSCREEN_WIFI:
#ifdef EXPERIMENTAL_WIFI_COMM
		if(wifi_netEnabled)
		{
			DialogBoxW(hAppInst,MAKEINTRESOURCEW(IDD_WIFISETTINGS), hwnd, (DLGPROC) WifiSettingsDlgProc);
		}
		else
		{
			MessageBox(MainWindow->getHWnd(),"winpcap failed to initialize, and so wifi cannot be configured.","wifi system failure",0);
		}
#endif
		break;
	}

	if (tpaused)
		NDS_UnPause();
}

void FilterUpdate(HWND hwnd, bool user)
{
	UpdateScreenRects();
	UpdateWndRects(hwnd);
	SetScreenGap(video.screengap);
	SetRotate(hwnd, video.rotation, false);
	if(user && windowSize==0) {}
	else ScaleScreen(windowSize);
	WritePrivateProfileInt("Video", "Filter", video.currentfilter, IniName);
	WritePrivateProfileInt("Video", "Width", video.width, IniName);
	WritePrivateProfileInt("Video", "Height", video.height, IniName);
}

void SaveWindowSize(HWND hwnd)
{
	//dont save if window was maximized
	if(IsZoomed(hwnd)) return;
	RECT rc;
	GetClientRect(hwnd, &rc);
	WritePrivateProfileInt("Video", "Window width", (rc.right - rc.left), IniName);
	WritePrivateProfileInt("Video", "Window height", (rc.bottom - rc.top), IniName);
}

void SaveWindowPos(HWND hwnd)
{
	//dont save if window was maximized
	if(IsZoomed(hwnd)) return;
	WritePrivateProfileInt("Video", "WindowPosX", WndX/*MainWindowRect.left*/, IniName);
	WritePrivateProfileInt("Video", "WindowPosY", WndY/*MainWindowRect.top*/, IniName);
}


//========================================================================================
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
	static int tmp_execute;
	switch (message)                  // handle the messages
	{
		case WM_EXITMENULOOP:
			SPU_Pause(0);
			if (autoframeskipenab && frameskiprate) AutoFrameSkip_IgnorePreviousDelay();
			break;
		case WM_ENTERMENULOOP:		  //Update menu items that needs to be updated dynamically
		{
			SPU_Pause(1);

			MENUITEMINFOW mii;
			WCHAR menuItemString[256];
			ZeroMemory(&mii, sizeof(MENUITEMINFOW));
			//Check if AVI is recording
			mii.cbSize = sizeof(MENUITEMINFOW);
			mii.fMask = MIIM_STRING;
			LoadStringW(hAppInst, !AVI_IsRecording() ? IDM_FILE_RECORDAVI : IDM_FILE_STOPAVI, menuItemString, 256);
			mii.dwTypeData = menuItemString;
			SetMenuItemInfoW(mainMenu, IDM_FILE_RECORDAVI, FALSE, &mii);
			//Check if WAV is recording
			LoadStringW(hAppInst, !WAV_IsRecording() ? IDM_FILE_RECORDWAV : IDM_FILE_STOPWAV, menuItemString, 256);
			SetMenuItemInfoW(mainMenu, IDM_FILE_RECORDWAV, FALSE, &mii);

			//Menu items dependent on a ROM loaded
			DesEnableMenuItem(mainMenu, IDM_GAME_INFO,         romloaded);
			DesEnableMenuItem(mainMenu, IDM_IMPORTBACKUPMEMORY,romloaded);
			DesEnableMenuItem(mainMenu, IDM_EXPORTBACKUPMEMORY,romloaded);
			DesEnableMenuItem(mainMenu, IDM_STATE_SAVE,        romloaded);
			DesEnableMenuItem(mainMenu, IDM_STATE_LOAD,        romloaded);
			DesEnableMenuItem(mainMenu, IDM_PRINTSCREEN,       romloaded);
			DesEnableMenuItem(mainMenu, IDM_QUICK_PRINTSCREEN, romloaded);
			DesEnableMenuItem(mainMenu, IDM_FILE_RECORDAVI,    romloaded);
			DesEnableMenuItem(mainMenu, IDM_FILE_RECORDWAV,    romloaded);
			DesEnableMenuItem(mainMenu, IDM_RESET,             romloaded && movieMode != MOVIEMODE_PLAY);
			DesEnableMenuItem(mainMenu, IDM_CLOSEROM,          romloaded);
			DesEnableMenuItem(mainMenu, IDM_SHUT_UP,           romloaded);
			DesEnableMenuItem(mainMenu, IDM_CHEATS_LIST,       romloaded);
			DesEnableMenuItem(mainMenu, IDM_CHEATS_SEARCH,     romloaded);
			//DesEnableMenuItem(mainMenu, IDM_WIFISETTINGS,      romloaded);
#ifdef DEVELOPER_MENU_ITEMS
			DesEnableMenuItem(mainMenu, IDM_FILE_RECORDUSERSPUWAV,    romloaded && !WAV_IsRecording());
#endif

			DesEnableMenuItem(mainMenu, IDM_RECORD_MOVIE,      (romloaded /*&& movieMode == MOVIEMODE_INACTIVE*/));
			DesEnableMenuItem(mainMenu, IDM_PLAY_MOVIE,        (romloaded /*&& movieMode == MOVIEMODE_INACTIVE*/));
			DesEnableMenuItem(mainMenu, IDM_STOPMOVIE,         (romloaded && movieMode != MOVIEMODE_INACTIVE));

			DesEnableMenuItem(mainMenu, ID_RAM_WATCH,          romloaded);
			DesEnableMenuItem(mainMenu, ID_RAM_SEARCH,         romloaded);
			//Update savestate slot items based on ROM loaded
			for (int x = 0; x < 10; x++)
			{
				DesEnableMenuItem(mainMenu, IDM_STATE_SAVE_F1+x,   romloaded);
				DesEnableMenuItem(mainMenu, IDM_STATE_LOAD_F1+x,   romloaded);
			}
			
			//Gray the recent ROM menu item if there are no recent ROMs
			DesEnableMenuItem(mainMenu, ID_FILE_RECENTROM, RecentRoms.size()>0);

			//Updated Checked menu items
			
			//Pause
			MainWindow->checkMenu(IDM_PAUSE, ((paused)));
			// LCDs layout
			MainWindow->checkMenu(ID_LCDS_VERTICAL, ((video.layout==0)));
			MainWindow->checkMenu(ID_LCDS_HORIZONTAL, ((video.layout==1)));
			MainWindow->checkMenu(ID_LCDS_ONE, ((video.layout==2)));
			// LCDs swap
			MainWindow->checkMenu(ID_LCDS_NOSWAP,  video.swap == 0);
			MainWindow->checkMenu(ID_LCDS_SWAP,    video.swap == 1);
			MainWindow->checkMenu(ID_LCDS_MAINGPU, video.swap == 2);
			MainWindow->checkMenu(ID_LCDS_SUBGPU,  video.swap == 3);
			//Force Maintain Ratio
			MainWindow->checkMenu(IDC_FORCERATIO, ((ForceRatio)));
			//Screen rotation
			MainWindow->checkMenu(IDC_ROTATE0, ((video.rotation_userset==0)));
			MainWindow->checkMenu(IDC_ROTATE90, ((video.rotation_userset==90)));
			MainWindow->checkMenu(IDC_ROTATE180, ((video.rotation_userset==180)));
			MainWindow->checkMenu(IDC_ROTATE270, ((video.rotation_userset==270)));

			//Window Size
			MainWindow->checkMenu(IDC_WINDOW1X, ((windowSize==1)));
			MainWindow->checkMenu(IDC_WINDOW1_5X, ((windowSize==kScale1point5x)));
			MainWindow->checkMenu(IDC_WINDOW2X, ((windowSize==2)));
			MainWindow->checkMenu(IDC_WINDOW2_5X, ((windowSize==kScale2point5x)));
			MainWindow->checkMenu(IDC_WINDOW3X, ((windowSize==3)));
			MainWindow->checkMenu(IDC_WINDOW4X, ((windowSize==4)));
			MainWindow->checkMenu(IDM_ALWAYS_ON_TOP, (GetStyle()&DWS_ALWAYSONTOP)!=0);
			MainWindow->checkMenu(IDM_LOCKDOWN, (GetStyle()&DWS_LOCKDOWN)!=0);

			//Screen Separation
			MainWindow->checkMenu(IDM_SCREENSEP_NONE,   ((video.screengap==kGapNone)));
			MainWindow->checkMenu(IDM_SCREENSEP_BORDER, ((video.screengap==kGapBorder)));
			MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP, ((video.screengap==kGapNDS)));
			MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP2, ((video.screengap==kGapNDS2)));
			MainWindow->checkMenu(IDM_SCREENSEP_DRAGEDIT, (SeparationBorderDrag));
			MainWindow->checkMenu(IDM_SCREENSEP_COLORWHITE, ((ScreenGapColor&0xFFFFFF) == 0xFFFFFF));
			MainWindow->checkMenu(IDM_SCREENSEP_COLORGRAY, ((ScreenGapColor&0xFFFFFF) == 0x7F7F7F));
			MainWindow->checkMenu(IDM_SCREENSEP_COLORBLACK, ((ScreenGapColor&0xFFFFFF) == 0x000000));
	
			//Counters / Etc.
			MainWindow->checkMenu(ID_VIEW_FRAMECOUNTER,CommonSettings.hud.FrameCounterDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYFPS,CommonSettings.hud.FpsDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYINPUT,CommonSettings.hud.ShowInputDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYGRAPHICALINPUT,CommonSettings.hud.ShowGraphicalInputDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYLAG,CommonSettings.hud.ShowLagFrameCounter);
			MainWindow->checkMenu(ID_VIEW_DISPLAYMICROPHONE,CommonSettings.hud.ShowMicrophone);
			MainWindow->checkMenu(ID_VIEW_HUDEDITOR, HudEditorMode);
			MainWindow->checkMenu(IDC_FRAMELIMIT, FrameLimit);
			
			//Frame Skip
			MainWindow->checkMenu(IDC_FRAMESKIPAUTO, autoframeskipenab);
			DesEnableMenuItem(mainMenu, IDC_FRAMESKIPAUTO, frameskiprate!=0);
			MainWindow->checkMenu(IDC_FRAMESKIP0, frameskiprate==0);
			MainWindow->checkMenu(IDC_FRAMESKIP1, frameskiprate==1);
			MainWindow->checkMenu(IDC_FRAMESKIP2, frameskiprate==2);
			MainWindow->checkMenu(IDC_FRAMESKIP3, frameskiprate==3);
			MainWindow->checkMenu(IDC_FRAMESKIP4, frameskiprate==4);
			MainWindow->checkMenu(IDC_FRAMESKIP5, frameskiprate==5);
			MainWindow->checkMenu(IDC_FRAMESKIP6, frameskiprate==6);
			MainWindow->checkMenu(IDC_FRAMESKIP7, frameskiprate==7);
			MainWindow->checkMenu(IDC_FRAMESKIP8, frameskiprate==8);
			MainWindow->checkMenu(IDC_FRAMESKIP9, frameskiprate==9);

			//gpu visibility toggles
			MainWindow->checkMenu(IDM_MGPU, CommonSettings.showGpu.main );
			MainWindow->checkMenu(IDM_SGPU, CommonSettings.showGpu.sub );
			//TODO - change how the gpu visibility flags work

			//Filters
			MainWindow->checkMenu(IDM_RENDER_NORMAL, video.currentfilter == video.NONE );
			MainWindow->checkMenu(IDM_RENDER_LQ2X, video.currentfilter == video.LQ2X );
			MainWindow->checkMenu(IDM_RENDER_LQ2XS, video.currentfilter == video.LQ2XS );
			MainWindow->checkMenu(IDM_RENDER_HQ2X, video.currentfilter == video.HQ2X );
			MainWindow->checkMenu(IDM_RENDER_HQ2XS, video.currentfilter == video.HQ2XS );
			MainWindow->checkMenu(IDM_RENDER_2XSAI, video.currentfilter == video._2XSAI );
			MainWindow->checkMenu(IDM_RENDER_SUPER2XSAI, video.currentfilter == video.SUPER2XSAI );
			MainWindow->checkMenu(IDM_RENDER_SUPEREAGLE, video.currentfilter == video.SUPEREAGLE );
			MainWindow->checkMenu(IDM_RENDER_SCANLINE, video.currentfilter == video.SCANLINE );
			MainWindow->checkMenu(IDM_RENDER_BILINEAR, video.currentfilter == video.BILINEAR );
			MainWindow->checkMenu(IDM_RENDER_NEAREST2X, video.currentfilter == video.NEAREST2X );
			MainWindow->checkMenu(IDM_RENDER_EPX, video.currentfilter == video.EPX );
			MainWindow->checkMenu(IDM_RENDER_EPXPLUS, video.currentfilter == video.EPXPLUS );
			MainWindow->checkMenu(IDM_RENDER_EPX1POINT5, video.currentfilter == video.EPX1POINT5 );
			MainWindow->checkMenu(IDM_RENDER_EPXPLUS1POINT5, video.currentfilter == video.EPXPLUS1POINT5 );
			MainWindow->checkMenu(IDM_RENDER_NEAREST1POINT5, video.currentfilter == video.NEAREST1POINT5 );
			MainWindow->checkMenu(IDM_RENDER_NEARESTPLUS1POINT5, video.currentfilter == video.NEARESTPLUS1POINT5 );

			MainWindow->checkMenu(IDC_STATEREWINDING, staterewindingenabled == 1 );

			MainWindow->checkMenu(IDC_BACKGROUNDPAUSE, lostFocusPause);

			//Save type
			const int savelist[] = {IDC_SAVETYPE1,IDC_SAVETYPE2,IDC_SAVETYPE3,IDC_SAVETYPE4,IDC_SAVETYPE5,IDC_SAVETYPE6,IDC_SAVETYPE7};
			for(int i=0;i<7;i++) 
				MainWindow->checkMenu(savelist[i], false);
			MainWindow->checkMenu(savelist[CommonSettings.manualBackupType], true);

			// recent/active scripts menu
			PopulateLuaSubmenu();

			if (video.layout != 0)
			{
				DesEnableMenuItem(mainMenu, IDC_ROTATE90, false);
				DesEnableMenuItem(mainMenu, IDC_ROTATE180, false);
				DesEnableMenuItem(mainMenu, IDC_ROTATE270, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NONE, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_BORDER, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NDSGAP, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_NDSGAP2, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_DRAGEDIT, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORWHITE, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORGRAY, false);
				DesEnableMenuItem(mainMenu, IDM_SCREENSEP_COLORBLACK, false);
			}

			UpdateHotkeyAssignments();	//Add current hotkey mappings to menu item names

			return 0;
		}

	case WM_CREATE:
		{
			path.ReadPathSettings();
			pausedByMinimize = FALSE;
			UpdateScreenRects();
			return 0;
		}
	case WM_DESTROY:
	case WM_CLOSE:
		{
			NDS_Pause();
			if (true/*AskSave()*/)	//Ask Save comes from the Ram Watch dialog.  The dialog uses .wch files and this allows asks the user if he wants to save changes first, should he cancel, closing will not happen
			{
				//Save window size
				WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
				if (windowSize==0)
				{
					SaveWindowSize(hwnd);
				}

				//Save window position
				SaveWindowPos(hwnd);

				//Save frame counter status
				WritePrivateProfileInt("Display", "FrameCounter", CommonSettings.hud.FrameCounterDisplay, IniName);

				WritePrivateProfileInt("Display", "ScreenGap", video.screengap, IniName);

				//Save Ram Watch information
				WritePrivateProfileInt("RamWatch", "SaveWindowPos", RWSaveWindowPos, IniName);
				WritePrivateProfileInt("RamWatch", "RWWindowPosX", ramw_x, IniName);
				WritePrivateProfileInt("RamWatch", "RWWindowPosY", ramw_y, IniName);
				WritePrivateProfileInt("RamWatch", "Auto-load", AutoRWLoad, IniName);

				for(int i = 0; i < MAX_RECENT_WATCHES; i++)
				{
					char str[256];
					sprintf(str, "Recent Watch %d", i+1);
					WritePrivateProfileString("Watches", str, &rw_recent_files[i][0], IniName);	
				}

				for(int i = 0; i < MAX_RECENT_SCRIPTS; i++)
				{
					char str[256];
					sprintf(str, "Recent Lua Script %d", i+1);
					WritePrivateProfileString("Scripting", str, &Recent_Scripts[i][0], IniName);
				}

				WritePrivateProfileInt("Sound", "Volume", sndvolume, IniName);

				ExitRunLoop();
			}
			else
				NDS_UnPause();
			return 0;
		}
	case WM_MOVING:
		InvalidateRect(hwnd, NULL, FALSE); UpdateWindow(hwnd);
		return 0;
	case WM_MOVE: {
		RECT rect;
		GetWindowRect(MainWindow->getHWnd(),&rect);
		WndX = rect.left;
		WndY = rect.top;
		UpdateWndRects(hwnd);
		return 0;
	}

	case WM_KILLFOCUS:
		if(lostFocusPause) {
			if(!emu_paused) {
				Pause();
				lastPauseFromLostFocus = TRUE;
			}
		}
		return 0;
	case WM_SETFOCUS:
		if(lostFocusPause) {
			if(lastPauseFromLostFocus) {
				Unpause();
			}
		}
		return 0;

	case WM_SIZING:
		{
			{
				Lock lock(win_backbuffer_sync);
				backbuffer_invalidate = true;
			}
			bool fullscreen = false;
			if(wParam==999) { fullscreen = true; wParam = WMSZ_BOTTOMRIGHT; }
			InvalidateRect(hwnd, NULL, FALSE); UpdateWindow(hwnd);

			if(windowSize)
			{
				windowSize = 0;
				MainWindow->checkMenu(IDC_WINDOW1X, false);
				MainWindow->checkMenu(IDC_WINDOW1_5X, false);
				MainWindow->checkMenu(IDC_WINDOW2X, false);
				MainWindow->checkMenu(IDC_WINDOW2_5X, false);
				MainWindow->checkMenu(IDC_WINDOW3X, false);
				MainWindow->checkMenu(IDC_WINDOW4X, false);
			}

			RECT cRect, ncRect;
			GetClientRect(MainWindow->getHWnd(), &cRect);
			GetWindowRect(MainWindow->getHWnd(), &ncRect);

			LONG forceRatioFlags = WINCLASS::NOKEEP;
			bool setGap = false;
			bool sideways = (video.rotation == 90) || (video.rotation == 270);
			{
				bool horizontalDrag = (wParam == WMSZ_LEFT) || (wParam == WMSZ_RIGHT);
				bool verticalDrag = (wParam == WMSZ_TOP) || (wParam == WMSZ_BOTTOM);
				int minX = 0;
				int minY = 0;

				if (video.layout == 0)
				{
					minX = video.rotatedwidthgap();
					minY = video.rotatedheightgap();
				}
				else
					if (video.layout == 1)
					{
						minX = video.rotatedwidthgap() * 2;
						minY = video.rotatedheightgap() / 2;
					}
					else
						if (video.layout == 2)
						{
							minX = video.rotatedwidthgap();
							minY = video.rotatedheightgap() / 2;
						}

				if(verticalDrag && !sideways && SeparationBorderDrag && video.layout == 0)
				{
					forceRatioFlags |= WINCLASS::KEEPX;
					minY = (MainScreenRect.bottom - MainScreenRect.top) + (SubScreenRect.bottom - SubScreenRect.top);
					setGap = true;
				}
				else if(horizontalDrag && sideways && SeparationBorderDrag && video.layout == 0)
				{
					forceRatioFlags |= WINCLASS::KEEPY;
					minX = (MainScreenRect.right - MainScreenRect.left) + (SubScreenRect.right - SubScreenRect.left);
					setGap = true;
				}
				else if(ForceRatio)
				{
					forceRatioFlags |= WINCLASS::KEEPX;
					forceRatioFlags |= WINCLASS::KEEPY;
				}
				MainWindow->setMinSize(minX, minY);
			}


			MainWindow->sizingMsg(wParam, lParam, forceRatioFlags | (fullscreen?WINCLASS::FULLSCREEN:0));

			if (video.layout == 1) return 1;
			if (video.layout == 2) return 1;

			if(setGap)
			{
				RECT rc = *(RECT*)lParam;
				rc.right += (cRect.right - cRect.left) - (ncRect.right - ncRect.left);
				rc.bottom += (cRect.bottom - cRect.top) - (ncRect.bottom - ncRect.top);
				int wndWidth, wndHeight, wndHeightGapless;
				if(sideways)
				{
					wndWidth = (rc.bottom - rc.top);
					wndHeight = (rc.right - rc.left);
					wndHeightGapless = (MainScreenRect.right - MainScreenRect.left) + (SubScreenRect.right - SubScreenRect.left);
				}
				else
				{
					wndWidth = (rc.right - rc.left);
					wndHeight = (rc.bottom - rc.top);
					wndHeightGapless = (MainScreenRect.bottom - MainScreenRect.top) + (SubScreenRect.bottom - SubScreenRect.top);
				}
				if(ForceRatio)
					video.screengap = wndHeight * video.width / wndWidth - video.height;
				else
					video.screengap = wndHeight * video.height / wndHeightGapless - video.height;
				UpdateWndRects(MainWindow->getHWnd());
			}
		}
		return 1;
		//break;

	case WM_GETMINMAXINFO:
		if(ForceRatio)
		{
			// extend the window size limits, otherwise they can make our window get squashed
			PMINMAXINFO pmmi = (PMINMAXINFO)lParam;
			pmmi->ptMaxTrackSize.x *= 4;
			pmmi->ptMaxTrackSize.y *= 4;
			return 1;
		}
		break;

	case WM_KEYDOWN:
		input_acquire();
		if(wParam != VK_PAUSE)
			break;
	
	case WM_CUSTKEYDOWN:
			//since the user has used a gamepad,
			//send some fake input to keep the screensaver from running
			PreventScreensaver();
			goto DOKEYDOWN;
	case WM_SYSKEYDOWN:
DOKEYDOWN:
		{
			if(message == WM_SYSKEYDOWN && wParam==VK_RETURN && !(lParam&0x40000000))
			{
				if(IsZoomed(hwnd))
					ShowWindow(hwnd,SW_NORMAL); //maximize and fullscreen get mixed up so make sure no maximize now
				ToggleFullscreen();
			}
			else
			{
				int modifiers = GetModifiers(wParam);
				wParam = PurgeModifiers(wParam);
				if(!HandleKeyMessage(wParam,lParam, modifiers))
					return 0;
			}
			break;
		}
	case WM_KEYUP:
		//handle ctr+printscreen: our own custom printscreener
		if (wParam == VK_SNAPSHOT) { 
			if(GetKeyState(VK_CONTROL)&0x8000)
			{
				ScreenshotToClipboard();
				return 0;
			}
		}
		input_acquire();
		if(wParam != VK_PAUSE)
			break;
	case WM_SYSKEYUP:
	case WM_CUSTKEYUP:
		{
			int modifiers = GetModifiers(wParam);
			wParam = PurgeModifiers(wParam);
			HandleKeyUp(wParam, lParam, modifiers);
		}
		break;

	case WM_SIZE:
		{
			Lock lock(win_backbuffer_sync);
			backbuffer_invalidate = true;
		}
		switch(wParam)
		{
		case SIZE_MINIMIZED:
			{
				if(!paused)
				{
					pausedByMinimize = TRUE;
					NDS_Pause();
				}
			}
			break;
		default:
			{
				if(pausedByMinimize)
					NDS_UnPause();

				if(wParam==SIZE_MAXIMIZED)
				{
					RECT fullscreen;
					GetClientRect(hwnd,&fullscreen);
					int fswidth = fullscreen.right-fullscreen.left;
					int fsheight = fullscreen.bottom-fullscreen.top;

				
					//feed a fake window size to the aspect ratio protection logic.
					FullScreenRect = fullscreen;
					SendMessage(MainWindow->getHWnd(), WM_SIZING, 999, (LPARAM)&FullScreenRect);

					int fakewidth = FullScreenRect.right-FullScreenRect.left;
					int fakeheight = FullScreenRect.bottom-FullScreenRect.top;
					
					//now use it to create a new virtual area in the center of the client rect
					int xfudge = (fswidth-fakewidth)/2;
					int yfudge = (fsheight-fakeheight)/2;
					OffsetRect(&FullScreenRect,xfudge,yfudge);
				}
				
				UpdateWndRects(hwnd);

			}
			break;
		}
		return 0;
	case WM_PAINT:
		{
			HDC				hdc;
			PAINTSTRUCT		ps;

			hdc = BeginPaint(hwnd, &ps);

			if(!romloaded)
			{
				Display();
			}
			else
			{
				if(CommonSettings.single_core())
				{
					video.srcBuffer = (u8*)GPU_screen;
					DoDisplay(true);
				}
			}

			EndPaint(hwnd, &ps);
		}
		return 0;

	case WM_CUSTINVOKE:
		ServiceDisplayThreadInvocations();
		return 0;

	case WM_DROPFILES:
		{
			char filename[MAX_PATH] = "";
			DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
			DragFinish((HDROP)wParam);
			
			//adelikat: dropping these in from FCEUX, I hope this is the best place for that
			std::string fileDropped = filename;
			//-------------------------------------------------------
			//Check if Movie file
			//-------------------------------------------------------
			if (!(fileDropped.find(".dsm") == string::npos) && (fileDropped.find(".dsm") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if (romloaded && !(fileDropped.find(".dsm") == string::npos))	//.dsm is at the end of the filename so that must be the extension		
					FCEUI_LoadMovie(fileDropped.c_str(), 1, false, false);		 //We are convinced it is a movie file, attempt to load it
			}
			
			//-------------------------------------------------------
			//Check if Savestate file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".ds") == string::npos))
			{
				size_t extIndex = fileDropped.find(".ds");
				if (extIndex <= fileDropped.length()-4)	//Check to see it is both at the end (file extension) and there is on more character
				{
					if ((fileDropped[extIndex+3] >= '0' && fileDropped[extIndex+3] <= '9') || fileDropped[extIndex+3] == '-' || fileDropped[extIndex+3] == 't')	//If last character is 0-9 (making .ds0 - .ds9) or .dst
					{
						savestate_load(filename);
													//adelikat: TODO this should be a single function call in main, that way we can expand as future dialogs need updating
						Update_RAM_Search();		//Update_RAM_Watch() is also called; hotkey.cpp - HK_StateLoadSlot & State_Load also call these functions
					}
				}
			}
			
			//-------------------------------------------------------
			//Check if Lua script file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".lua") == string::npos) && (fileDropped.find(".lua") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(LuaScriptHWnds.size() < 16)
				{
					char temp [1024];
					strcpy(temp, fileDropped.c_str());
					HWND IsScriptFileOpen(const char* Path);
					if(!IsScriptFileOpen(temp))
					{
						HWND hDlg = CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
					}
				}
			}
			
			//-------------------------------------------------------
			//Check if watchlist file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".wch") == string::npos) && (fileDropped.find(".wch") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(!RamWatchHWnd)
				{
					RamWatchHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_RAMWATCH), hwnd, (DLGPROC) RamWatchProc);
					//				DialogsOpen++;
				}
				else
					SetForegroundWindow(RamWatchHWnd);
				Load_Watches(true, fileDropped.c_str());
			}
			
			//-------------------------------------------------------
			//Else load it as a ROM
			//-------------------------------------------------------

			else if(OpenCore(filename))
			{
				romloaded = TRUE;
			}
			NDS_UnPause();
		}
		return 0;
	case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
		return TABLET_DISABLE_PRESSANDHOLD |
			TABLET_DISABLE_PENTAPFEEDBACK |
			TABLET_DISABLE_PENBARRELFEEDBACK |
			TABLET_DISABLE_TOUCHUIFORCEON |
			TABLET_DISABLE_TOUCHUIFORCEOFF |
			TABLET_DISABLE_TOUCHSWITCH |
			TABLET_DISABLE_FLICKS |
			TABLET_DISABLE_SMOOTHSCROLLING |
			TABLET_DISABLE_FLICKFALLBACKKEYS;

	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if (wParam & MK_LBUTTON)
		{
			SetCapture(hwnd);

			s32 x = (s32)((s16)LOWORD(lParam));
			s32 y = (s32)((s16)HIWORD(lParam));

			UnscaleScreenCoords(x,y);

			if(HudEditorMode)
			{
				ToDSScreenRelativeCoords(x,y,0);
				EditHud(x,y, &Hud);
			}
			else
			{
				if ((video.layout == 2) && ((video.swap == 0) || (video.swap == 2 && !MainScreen.offset) || (video.swap == 3 && MainScreen.offset))) return 0;
				ToDSScreenRelativeCoords(x,y,1);
				if(x<0) x = 0; else if(x>255) x = 255;
				if(y<0) y = 0; else if(y>192) y = 192;
				NDS_setTouchPos(x, y);
				winLastTouch.x = x;
				winLastTouch.y = y;
				userTouchesScreen = true;
				return 0;
			}
		}
		if (!StylusAutoHoldPressed)
			NDS_releaseTouch();
		userTouchesScreen = false;
		return 0;

	case WM_LBUTTONUP:

		ReleaseCapture();
		HudClickRelease(&Hud);
		if (!StylusAutoHoldPressed)
			NDS_releaseTouch();
		userTouchesScreen = false;
		return 0;

#if 0
	case WM_INITMENU: {
		HMENU menu = (HMENU)wParam;
		//last minute modification of menu before display
		#ifndef DEVELOPER
			RemoveMenu(menu,IDM_DISASSEMBLER,MF_BYCOMMAND);
		#endif
		break;
	}
#endif

	case WM_COMMAND:
		if(HIWORD(wParam) == 0 || HIWORD(wParam) == 1)
		{
			//wParam &= 0xFFFF;

			// A menu item from the recent files menu was clicked.
			if(wParam >= recentRoms_baseid && wParam <= recentRoms_baseid + MAX_RECENT_ROMS - 1)
			{
				int x = wParam - recentRoms_baseid;
				OpenRecentROM(x);					
			}
			else if(wParam == recentRoms_clearid)
			{
				/* Clear all the recent ROMs */
				if(IDOK == MessageBox(hwnd, "OK to clear recent ROMs list?","DeSmuME",MB_OKCANCEL))
					ClearRecentRoms();
			}

			if(wParam >= IDD_LUARECENT_RESERVE_START &&
			   wParam <= IDD_LUARECENT_RESERVE_END &&
			   wParam - IDD_LUARECENT_RESERVE_START < MAX_RECENT_SCRIPTS)
			{
				if(LuaScriptHWnds.size() < 16)
				{
					char temp [1024];
					strcpy(temp, Recent_Scripts[wParam - IDD_LUARECENT_RESERVE_START]);
					HWND IsScriptFileOpen(const char* Path);
					if(!IsScriptFileOpen(temp))
					{
						HWND hDlg = CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
					}
				}
			}

			if(wParam >= IDC_LUASCRIPT_RESERVE_START &&
			   wParam <= IDC_LUASCRIPT_RESERVE_END)
			{
				unsigned int index = wParam - IDC_LUASCRIPT_RESERVE_START;
				if(LuaScriptHWnds.size() > index)
					SetForegroundWindow(LuaScriptHWnds[index]);
			}

		}
		switch(LOWORD(wParam))
		{
		case IDM_SHUT_UP:
			if(SPU_user) SPU_user->ShutUp();
			return 0;
		case IDM_QUIT:
			if (AskSave()) DestroyWindow(hwnd);
			return 0;
		case IDM_OPEN:
			return OpenFile();
		case IDM_CLOSEROM:
			return CloseRom(),0;
		case IDM_PRINTSCREEN:
			HK_PrintScreen(0, true);
			return 0;
		case IDM_QUICK_PRINTSCREEN:
			{
				char buffer[MAX_PATH];
				ZeroMemory(buffer, sizeof(buffer));
				path.getpath(path.SCREENSHOTS, buffer);

				char file[MAX_PATH];
				ZeroMemory(file, sizeof(file));
				path.formatname(file);
		
				strcat(buffer, file);
				if( strlen(buffer) > (MAX_PATH - 4))
					buffer[MAX_PATH - 4] = '\0';

				switch(path.imageformat())
				{
					case path.PNG:
						{		
							strcat(buffer, ".png");
							NDS_WritePNG(buffer);
						}
						break;
					case path.BMP:
						{
							strcat(buffer, ".bmp");
							NDS_WriteBMP(buffer);
						}
						break;
				}
			}
			return 0;
		case IDM_FILE_RECORDAVI:
			if (AVI_IsRecording())
				AviEnd();
			else
				AviRecordTo();
			break;
		case IDM_FILE_RECORDWAV:
			if (WAV_IsRecording())
				WAV_End();
			else
				WavRecordTo(WAVMODE_CORE);
			break;
#ifdef DEVELOPER_MENU_ITEMS
		case IDM_FILE_RECORDUSERSPUWAV:
			if (WAV_IsRecording())
				WAV_End();
			else
				WavRecordTo(WAVMODE_USER);
			break;
#endif
		case IDC_STATEREWINDING:
			if(staterewindingenabled) staterewindingenabled = false;
			else staterewindingenabled = true;
			break;
		case IDM_RENDER_NORMAL:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.NONE);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_LQ2X:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.LQ2X);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_LQ2XS:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.LQ2XS);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_HQ2X:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.HQ2X);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_HQ2XS:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.HQ2XS);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_2XSAI:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video._2XSAI);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_SUPER2XSAI:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.SUPER2XSAI);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_SUPEREAGLE:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.SUPEREAGLE);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_SCANLINE:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.SCANLINE);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_BILINEAR:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.BILINEAR);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_NEAREST2X:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.NEAREST2X);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_EPX:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.EPX);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_EPXPLUS:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.EPXPLUS);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_EPX1POINT5:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.EPX1POINT5);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_EPXPLUS1POINT5:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.EPXPLUS1POINT5);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_NEAREST1POINT5:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.NEAREST1POINT5);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_NEARESTPLUS1POINT5:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.NEARESTPLUS1POINT5);
				FilterUpdate(hwnd);
			}
			break;

		case IDM_STATE_LOAD:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "DeSmuME Savestate (*.dst or *.ds#)\0*.dst;*.ds0*;*.ds1*;*.ds2*;*.ds3*;*.ds4*;*.ds5*;*.ds6*;*.ds7*;*.ds8*;*.ds9*;*.ds-*\0DeSmuME Savestate (*.dst only)\0*.dst\0All files (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  SavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dst";
				ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

				char buffer[MAX_PATH];
				ZeroMemory(buffer, sizeof(buffer));
				path.getpath(path.STATES, buffer);
				ofn.lpstrInitialDir = buffer;

				if(!GetOpenFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

				savestate_load(SavName);
											//adelikat: TODO this should be a single function call in main, that way we can expand as future dialogs need updating
				Update_RAM_Search();		//Update_RAM_Watch() is also called; hotkey.cpp - HK_StateLoadSlot also calls these functions
				NDS_UnPause();
			}
			return 0;
		case IDM_STATE_SAVE:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "DeSmuME Savestate (*.dst or *.ds#)\0*.dst;*.ds0*;*.ds1*;*.ds2*;*.ds3*;*.ds4*;*.ds5*;*.ds6*;*.ds7*;*.ds8*;*.ds9*;*.ds-*\0DeSmuME Savestate (*.dst only)\0*.dst\0All files (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  SavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dst";
				ofn.Flags = OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

				char buffer[MAX_PATH];
				ZeroMemory(buffer, sizeof(buffer));
				path.getpath(path.STATES, buffer);
				ofn.lpstrInitialDir = buffer;

				if(!GetSaveFileName(&ofn))
				{
					return 0;
				}

				savestate_save(SavName);
				LoadSaveStateInfo();
				NDS_UnPause();
			}
			return 0;
		case IDM_STATE_SAVE_F1:
		case IDM_STATE_SAVE_F2:
		case IDM_STATE_SAVE_F3:
		case IDM_STATE_SAVE_F4:
		case IDM_STATE_SAVE_F5:
		case IDM_STATE_SAVE_F6:
		case IDM_STATE_SAVE_F7:
		case IDM_STATE_SAVE_F8:
		case IDM_STATE_SAVE_F9:
		case IDM_STATE_SAVE_F10:
				HK_StateSaveSlot( abs(IDM_STATE_SAVE_F1 - LOWORD(wParam)) +1, true);
				return 0;

		case IDM_STATE_LOAD_F1:
			HK_StateLoadSlot(1, true);
			return 0;
		case IDM_STATE_LOAD_F2:
			HK_StateLoadSlot(2, true);
			return 0;
		case IDM_STATE_LOAD_F3:
			HK_StateLoadSlot(3, true);
			return 0;
		case IDM_STATE_LOAD_F4:
			HK_StateLoadSlot(4, true);
			return 0;
		case IDM_STATE_LOAD_F5:
			HK_StateLoadSlot(5, true);
			return 0;
		case IDM_STATE_LOAD_F6:
			HK_StateLoadSlot(6, true);
			return 0;
		case IDM_STATE_LOAD_F7:
			HK_StateLoadSlot(7, true);
			return 0;
		case IDM_STATE_LOAD_F8:
			HK_StateLoadSlot(8, true);
			return 0;
		case IDM_STATE_LOAD_F9:
			HK_StateLoadSlot(9, true);
			return 0;
		case IDM_STATE_LOAD_F10:
			HK_StateLoadSlot(10, true);
			return 0;
		case IDM_IMPORTBACKUPMEMORY:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "All supported types\0*.duc;*.sav\0Action Replay DS Save (*.duc)\0*.duc\0Raw/No$GBA Save format (*.sav)\0*.sav\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  ImportSavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "duc";
				ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

				char buffer[MAX_PATH];
				ZeroMemory(buffer, sizeof(buffer));
				path.getpath(path.BATTERY, buffer);
				ofn.lpstrInitialDir = buffer;

				if(!GetOpenFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

				if (!NDS_ImportSave(ImportSavName))
					MessageBox(hwnd,"Save was not successfully imported","Error",MB_OK);
				NDS_UnPause();
				return 0;
			}
		case IDM_EXPORTBACKUPMEMORY:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "Raw Save format (*.sav)\0*.sav\0No$GBA Save format (*.sav)\0*.sav\0\0";
				ofn.nFilterIndex = 0;
				ofn.lpstrFile =  ImportSavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "sav";
				ofn.Flags = OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

				if(!GetSaveFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

				if (ofn.nFilterIndex == 2) strcat(ImportSavName, "*");

				if (!NDS_ExportSave(ImportSavName))
					MessageBox(hwnd,"Save was not successfully exported","Error",MB_OK);
				NDS_UnPause();
				return 0;
			}
		
		
		case IDM_CONFIG:
			RunConfig(CONFIGSCREEN_INPUT);
			return 0;
		case IDM_HOTKEY_CONFIG:
			RunConfig(CONFIGSCREEN_HOTKEY);
			return 0;
		case IDM_FIRMSETTINGS:
			RunConfig(CONFIGSCREEN_FIRMWARE);
			return 0;
		case IDM_SOUNDSETTINGS:
			RunConfig(CONFIGSCREEN_SOUND);
			return 0;
		case IDM_WIFISETTINGS:
			RunConfig(CONFIGSCREEN_WIFI);
			return 0;
		case IDM_EMULATIONSETTINGS:
			RunConfig(CONFIGSCREEN_EMULATION);
			return 0;
		case IDM_MICROPHONESETTINGS:
			RunConfig(CONFIGSCREEN_MICROPHONE);
			return 0;
		case IDM_PATHSETTINGS:
			RunConfig(CONFIGSCREEN_PATHSETTINGS);
			return 0;

		case IDM_GAME_INFO:
			{
				//CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_GAME_INFO), hwnd, GinfoView_Proc);
				GInfo_DlgOpen(hwnd);
			}
			return 0;

			//========================================================= Tools
		case IDM_PAL:
			ViewPalette->open();
			return 0;
		case IDM_TILE:
			{
				ViewTiles->regClass("TileViewBox", TileViewBoxProc);
				ViewTiles->regClass("MiniTileViewBox", MiniTileViewBoxProc, true);
				if (!ViewTiles->open())
					ViewTiles->unregClass();
			}
			return 0;
		case IDM_IOREG:
			//ViewRegisters->open();
			if (!RegWndClass("DeSmuME_IORegView", IORegView_Proc, CS_DBLCLKS, LoadIcon(hAppInst, MAKEINTRESOURCE(ICONDESMUME)), sizeof(CIORegView*)))
				return 0;

			OpenToolWindow(new CIORegView());
			return 0;
		case IDM_MEMORY:
			//if(!MemView_IsOpened(ARMCPU_ARM9)) MemView_DlgOpen(HWND_DESKTOP, "ARM9 memory", ARMCPU_ARM9);
			//if(!MemView_IsOpened(ARMCPU_ARM7)) MemView_DlgOpen(HWND_DESKTOP, "ARM7 memory", ARMCPU_ARM7);
			if (!RegWndClass("MemView_ViewBox", MemView_ViewBoxProc, 0, sizeof(CMemView*)))
				return 0;

			OpenToolWindow(new CMemView());
			return 0;
		case IDM_VIEW3D:
			driver->VIEW3D_Init();
			driver->view3d->Launch();
			return 0;
		case IDM_SOUND_VIEW:
			if(!SoundView_IsOpened()) SoundView_DlgOpen(HWND_DESKTOP);
			return 0;
		case IDM_DISASSEMBLER:
			ViewDisasm_ARM7->regClass("DesViewBox7",ViewDisasm_ARM7BoxProc);
			if (!ViewDisasm_ARM7->open())
				ViewDisasm_ARM7->unregClass();

			ViewDisasm_ARM9->regClass("DesViewBox9",ViewDisasm_ARM9BoxProc);
			if (!ViewDisasm_ARM9->open())
				ViewDisasm_ARM9->unregClass();
			return 0;
		case IDM_MAP:
			ViewMaps->open();
			return 0;
		case IDM_OAM:
			ViewOAM->regClass("OAMViewBox", ViewOAMBoxProc);
			if (!ViewOAM->open())
				ViewOAM->unregClass();
			return 0;

		case IDM_MATRIX_VIEWER:
			ViewMatrices->open();
			return 0;

		case IDM_LIGHT_VIEWER:
			ViewLights->open();
			return 0;
			//========================================================== Tools end

		case IDM_MGPU:
			CommonSettings.showGpu.main = !CommonSettings.showGpu.main;
			WritePrivateProfileInt("Display","MainGpu",CommonSettings.showGpu.main?1:0,IniName);
			return 0;

		case IDM_SGPU:
			CommonSettings.showGpu.sub = !CommonSettings.showGpu.sub;
			WritePrivateProfileInt("Display","SubGpu",CommonSettings.showGpu.sub?1:0,IniName);
			return 0;

		case IDM_MBG0 : 
			if(MainScreen.gpu->dispBG[0])
			{
				GPU_remove(MainScreen.gpu, 0);
				MainWindow->checkMenu(IDM_MBG0, false);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 0);
				MainWindow->checkMenu(IDM_MBG0, true);
			}
			return 0;
		case IDM_MBG1 : 
			if(MainScreen.gpu->dispBG[1])
			{
				GPU_remove(MainScreen.gpu, 1);
				MainWindow->checkMenu(IDM_MBG1, false);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 1);
				MainWindow->checkMenu(IDM_MBG1, true);
			}
			return 0;
		case IDM_MBG2 : 
			if(MainScreen.gpu->dispBG[2])
			{
				GPU_remove(MainScreen.gpu, 2);
				MainWindow->checkMenu(IDM_MBG2, false);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 2);
				MainWindow->checkMenu(IDM_MBG2, true);
			}
			return 0;
		case IDM_MBG3 : 
			if(MainScreen.gpu->dispBG[3])
			{
				GPU_remove(MainScreen.gpu, 3);
				MainWindow->checkMenu(IDM_MBG3, false);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 3);
				MainWindow->checkMenu(IDM_MBG3, true);
			}
			return 0;
		case IDM_SBG0 : 
			if(SubScreen.gpu->dispBG[0])
			{
				GPU_remove(SubScreen.gpu, 0);
				MainWindow->checkMenu(IDM_SBG0, false);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 0);
				MainWindow->checkMenu(IDM_SBG0, true);
			}
			return 0;
		case IDM_SBG1 : 
			if(SubScreen.gpu->dispBG[1])
			{
				GPU_remove(SubScreen.gpu, 1);
				MainWindow->checkMenu(IDM_SBG1, false);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 1);
				MainWindow->checkMenu(IDM_SBG1, true);
			}
			return 0;
		case IDM_SBG2 : 
			if(SubScreen.gpu->dispBG[2])
			{
				GPU_remove(SubScreen.gpu, 2);
				MainWindow->checkMenu(IDM_SBG2, false);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 2);
				MainWindow->checkMenu(IDM_SBG2, true);
			}
			return 0;
		case IDM_SBG3 : 
			if(SubScreen.gpu->dispBG[3])
			{
				GPU_remove(SubScreen.gpu, 3);
				MainWindow->checkMenu(IDM_SBG3, false);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 3);
				MainWindow->checkMenu(IDM_SBG3, true);
			}
			return 0;

		case IDM_PAUSE:
			TogglePause();
			return 0;

		case IDM_GBASLOT:
			GBAslotDialog(hwnd);
			return 0;

		case IDM_CHEATS_LIST:
			CheatsListDialog(hwnd);
			return 0;

		case IDM_CHEATS_SEARCH:
			CheatsSearchDialog(hwnd);
			return 0;

		case IDM_CHEATS_DISABLE:
			_cheatsDisabled = !_cheatsDisabled;
			cheats->disable(_cheatsDisabled);
			MainWindow->checkMenu(IDM_CHEATS_DISABLE, _cheatsDisabled );
			return 0;

		case IDM_RECORD_MOVIE:
			MovieRecordTo();
			return 0;
		case IDM_PLAY_MOVIE:
			Replay_LoadMovie();
			return 0;
		case IDM_STOPMOVIE:
			FCEUI_StopMovie();
			return 0;

		case ID_LCDS_VERTICAL:
			if (video.layout == 0) return 0;
			video.layout = 0;
			doLCDsLayout();
			return 0;
		case ID_LCDS_HORIZONTAL:
			if (video.layout == 1) return 0;
			video.layout = 1;
			doLCDsLayout();
			return 0;

		case ID_LCDS_ONE:
			if (video.layout == 2) return 0;
			video.layout = 2;
			doLCDsLayout();
			return 0;

		case ID_LCDS_NOSWAP:
			LCDsSwap(0);
			return 0;
		case ID_LCDS_SWAP:
			LCDsSwap(1);
			return 0;
		case ID_LCDS_MAINGPU:
			LCDsSwap(2);
			return 0;
		case ID_LCDS_SUBGPU:
			LCDsSwap(3);
			return 0;

		case ID_VIEW_FRAMECOUNTER:
			CommonSettings.hud.FrameCounterDisplay ^= true;
			WritePrivateProfileBool("Display", "FrameCounter", CommonSettings.hud.FrameCounterDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYFPS:
			CommonSettings.hud.FpsDisplay ^= true;
			WritePrivateProfileBool("Display", "Display Fps", CommonSettings.hud.FpsDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYINPUT:
			CommonSettings.hud.ShowInputDisplay ^= true;
			WritePrivateProfileBool("Display", "Display Input", CommonSettings.hud.ShowInputDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYGRAPHICALINPUT:
			CommonSettings.hud.ShowGraphicalInputDisplay ^= true;
			WritePrivateProfileBool("Display", "Display Graphical Input", CommonSettings.hud.ShowGraphicalInputDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYLAG:
			CommonSettings.hud.ShowLagFrameCounter ^= true;
			WritePrivateProfileBool("Display", "Display Lag Counter", CommonSettings.hud.ShowLagFrameCounter, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_HUDEDITOR:
			HudEditorMode ^= true;
			osd->clear();
			osd->border(HudEditorMode);
			return 0;

		case ID_VIEW_DISPLAYMICROPHONE:
			CommonSettings.hud.ShowMicrophone ^= true;
			WritePrivateProfileBool("Display", "Display Microphone", CommonSettings.hud.ShowMicrophone, IniName);
			osd->clear();
			return 0;

		case ID_RAM_SEARCH:
			if(!RamSearchHWnd)
			{
				InitRamSearch();
				RamSearchHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_RAMSEARCH), hwnd, (DLGPROC) RamSearchProc);
			}
			else
				SetForegroundWindow(RamSearchHWnd);
			break;

		case ID_RAM_WATCH:
			if(!RamWatchHWnd)
			{
				RamWatchHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_RAMWATCH), hwnd, (DLGPROC) RamWatchProc);
				//				DialogsOpen++;
			}
			else
				SetForegroundWindow(RamWatchHWnd);
			return 0;

		case IDC_BACKGROUNDPAUSE:
			lostFocusPause = !lostFocusPause;
			WritePrivateProfileInt("Focus", "BackgroundPause", (int)lostFocusPause, IniName);
			return 0;

		case IDC_SAVETYPE1: backup_setManualBackupType(0); return 0;
		case IDC_SAVETYPE2: backup_setManualBackupType(1); return 0;   
		case IDC_SAVETYPE3: backup_setManualBackupType(2); return 0;   
		case IDC_SAVETYPE4: backup_setManualBackupType(3); return 0;
		case IDC_SAVETYPE5: backup_setManualBackupType(4); return 0; 
		case IDC_SAVETYPE6: backup_setManualBackupType(5); return 0; 
		case IDC_SAVETYPE7: backup_setManualBackupType(6); return 0; 

		case IDM_RESET:
			ResetGame();
			return 0;

		case IDM_3DCONFIG:
			{
				bool tpaused = false;
				if(execute) 
				{
					tpaused = true;
					NDS_Pause();
				}

				DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_3DSETTINGS), hwnd, (DLGPROC)GFX3DSettingsDlgProc);

				if(tpaused) NDS_UnPause();
			}
			return 0;
		case IDC_FRAMESKIPAUTO:
			{
				char text[80];
				autoframeskipenab = !autoframeskipenab;
				sprintf(text, "%s%d", autoframeskipenab ? "AUTO" : "", frameskiprate);
				WritePrivateProfileString("Video", "FrameSkip", text, IniName);
				AutoFrameSkip_IgnorePreviousDelay();
			}
			return 0;
		case IDC_FRAMESKIP0:
		case IDC_FRAMESKIP1:
		case IDC_FRAMESKIP2:
		case IDC_FRAMESKIP3:
		case IDC_FRAMESKIP4:
		case IDC_FRAMESKIP5:
		case IDC_FRAMESKIP6:
		case IDC_FRAMESKIP7:
		case IDC_FRAMESKIP8:
		case IDC_FRAMESKIP9:
			{
				char text[80];
				frameskiprate = LOWORD(wParam) - IDC_FRAMESKIP0;
				sprintf(text, "%s%d", autoframeskipenab ? "AUTO" : "", frameskiprate);
				WritePrivateProfileString("Video", "FrameSkip", text, IniName);
			}
			return 0;
		case IDC_NEW_LUA_SCRIPT:
			if(LuaScriptHWnds.size() < 16)
				CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
			break;
		case IDC_CLOSE_LUA_SCRIPTS:
			for(int i=(int)LuaScriptHWnds.size()-1; i>=0; i--)
				SendMessage(LuaScriptHWnds[i], WM_CLOSE, 0,0);
			break;
		case IDC_LANGENGLISH:
			SaveLanguage(0);
			ChangeLanguage(0);
			CheckLanguage(LOWORD(wParam));
			return 0;
		case IDC_LANGFRENCH:
			SaveLanguage(1);
			ChangeLanguage(1);
			CheckLanguage(LOWORD(wParam));
			return 0;
		case IDC_LANG_CHINESE_SIMPLIFIED:
			SaveLanguage(3);
			ChangeLanguage(3);
			CheckLanguage(LOWORD(wParam));
			return 0;
		case IDC_LANGITALIAN:
			SaveLanguage(4);
			ChangeLanguage(4);
			CheckLanguage(LOWORD(wParam));
			return 0;
		case IDC_LANGJAPANESE:
			SaveLanguage(5);
			ChangeLanguage(5);
			CheckLanguage(LOWORD(wParam));
			return 0;

		case IDC_FRAMELIMIT:
			FrameLimit ^= 1;
			WritePrivateProfileInt("FrameLimit", "FrameLimit", FrameLimit, IniName);
			return 0;

		case IDM_SCREENSEP_NONE:
			SetScreenGap(kGapNone);
			return 0;
		case IDM_SCREENSEP_BORDER:
			SetScreenGap(kGapBorder);
			return 0;
		case IDM_SCREENSEP_NDSGAP:
			SetScreenGap(kGapNDS);
			return 0;
		case IDM_SCREENSEP_NDSGAP2:
			SetScreenGap(kGapNDS2);
			return 0;
		case IDM_SCREENSEP_DRAGEDIT:
			SeparationBorderDrag = !SeparationBorderDrag;
			WritePrivateProfileInt("Display","Window Split Border Drag",(int)SeparationBorderDrag,IniName);
			break;
		case IDM_SCREENSEP_COLORWHITE:
		case IDM_SCREENSEP_COLORGRAY:
		case IDM_SCREENSEP_COLORBLACK:
			switch(LOWORD(wParam))
			{
			case IDM_SCREENSEP_COLORWHITE: ScreenGapColor = 0xFFFFFF; break;
			case IDM_SCREENSEP_COLORGRAY:  ScreenGapColor = 0x7F7F7F; break;
			case IDM_SCREENSEP_COLORBLACK: ScreenGapColor = 0x000000; break;
			}
			WritePrivateProfileInt("Display", "ScreenGapColor", ScreenGapColor, IniName);
			break;
		case IDM_WEBSITE:
			ShellExecute(NULL, "open", "http://desmume.sourceforge.net", NULL, NULL, SW_SHOWNORMAL);
			return 0;

		case IDM_FORUM:
			ShellExecute(NULL, "open", "http://forums.desmume.org/index.php", NULL, NULL, SW_SHOWNORMAL);
			return 0;

		case IDM_ABOUT:
			{
				bool tpaused=false;
				if (execute) 
				{
					tpaused=true;
					NDS_Pause();
				}
				DialogBoxW(hAppInst,MAKEINTRESOURCEW(IDD_ABOUT_BOX), hwnd, (DLGPROC) AboutBox_Proc);
				if (tpaused)
					NDS_UnPause();

				return 0;
			}

#ifndef BETA_VERSION
		case IDM_SUBMITBUGREPORT:
			ShellExecute(NULL, "open", "http://sourceforge.net/tracker/?func=add&group_id=164579&atid=832291", NULL, NULL, SW_SHOWNORMAL);
			return 0;
#endif

		case IDC_ROTATE0:
			SetRotate(hwnd, 0);
			return 0;
		case IDC_ROTATE90:
			SetRotate(hwnd, 90);
			return 0;
		case IDC_ROTATE180:
			SetRotate(hwnd, 180);
			return 0;
		case IDC_ROTATE270:
			SetRotate(hwnd, 270);
			return 0;

		case IDC_WINDOW1_5X:
			windowSize=kScale1point5x;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW2_5X:
			windowSize=kScale2point5x;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW1X:
			windowSize=1;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW2X:
			windowSize=2;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW3X:
			windowSize=3;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW4X:
			windowSize=4;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;

		case IDC_FORCERATIO:
			if (ForceRatio) {
				ForceRatio = FALSE;
				WritePrivateProfileInt("Video","Window Force Ratio",0,IniName);
			}
			else {
				ForceRatio = TRUE;
				FixAspectRatio();
				WritePrivateProfileInt("Video","Window Force Ratio",1,IniName);
			}
			break;


		case IDM_DEFSIZE:
			{
				windowSize = 1;
				ScaleScreen(1);
			}
			break;
		case IDM_LOCKDOWN:
			{
				SetStyle(GetStyle()^DWS_LOCKDOWN);
				WritePrivateProfileBool("Video", "Window Lockdown", (GetStyle()&DWS_LOCKDOWN)!=0, IniName);
			}
			return 0;
		case IDM_ALWAYS_ON_TOP:
			{
				SetStyle(GetStyle()^DWS_ALWAYSONTOP);
				WritePrivateProfileBool("Video", "Window Always On Top", (GetStyle()&DWS_ALWAYSONTOP)!=0, IniName);
			}
			return 0;

		}
	}
  return DefWindowProc (hwnd, message, wParam, lParam);
}

void Change3DCoreWithFallbackAndSave(int newCore, int fallbackCore)
{
	if(!NDS_3D_ChangeCore(newCore) && newCore != fallbackCore)
		NDS_3D_ChangeCore(fallbackCore);
	int gpu3dSaveValue = ((cur3DCore != GPU3D_NULL) ? cur3DCore : GPU3D_NULL_SAVED);
	WritePrivateProfileInt("3D", "Renderer", gpu3dSaveValue, IniName);
}

LRESULT CALLBACK GFX3DSettingsDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			int i;

			CheckDlgButton(hw,IDC_INTERPOLATECOLOR,CommonSettings.GFX3D_HighResolutionInterpolateColor?1:0);
			CheckDlgButton(hw,IDC_3DSETTINGS_EDGEMARK,CommonSettings.GFX3D_EdgeMark?1:0);
			CheckDlgButton(hw,IDC_3DSETTINGS_FOG,CommonSettings.GFX3D_Fog?1:0);
			//CheckDlgButton(hw,IDC_ALTERNATEFLUSH,CommonSettings.gfx3d_flushMode);

			for(i = 0; core3DList[i] != NULL; i++)
			{
				ComboBox_AddString(GetDlgItem(hw, IDC_3DCORE), core3DList[i]->name);
			}
			ComboBox_SetCurSel(GetDlgItem(hw, IDC_3DCORE), cur3DCore);
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wp))
			{
			case IDOK:
				{
					CommonSettings.GFX3D_HighResolutionInterpolateColor = IsDlgCheckboxChecked(hw,IDC_INTERPOLATECOLOR);
					CommonSettings.GFX3D_EdgeMark = IsDlgCheckboxChecked(hw,IDC_3DSETTINGS_EDGEMARK);
					CommonSettings.GFX3D_Fog = IsDlgCheckboxChecked(hw,IDC_3DSETTINGS_FOG);
					Change3DCoreWithFallbackAndSave(ComboBox_GetCurSel(GetDlgItem(hw, IDC_3DCORE)));
					WritePrivateProfileInt("3D", "HighResolutionInterpolateColor", CommonSettings.GFX3D_HighResolutionInterpolateColor?1:0, IniName);
					WritePrivateProfileInt("3D", "EnableEdgeMark", CommonSettings.GFX3D_EdgeMark?1:0, IniName);
					WritePrivateProfileInt("3D", "EnableFog", CommonSettings.GFX3D_Fog?1:0, IniName);
					//CommonSettings.gfx3d_flushMode = (IsDlgButtonChecked(hw,IDC_ALTERNATEFLUSH) == BST_CHECKED)?1:0;
					//WritePrivateProfileInt("3D", "AlternateFlush", CommonSettings.gfx3d_flushMode, IniName);
				}
			case IDCANCEL:
				{
					EndDialog(hw, TRUE);
				}
				return TRUE;

			case IDC_DEFAULT:
				{
					Change3DCoreWithFallbackAndSave(GPU3D_DEFAULT);
					ComboBox_SetCurSel(GetDlgItem(hw, IDC_3DCORE), cur3DCore);
					//CommonSettings.gfx3d_flushMode = 0;
					//WritePrivateProfileInt("3D", "AlternateFlush", CommonSettings.gfx3d_flushMode, IniName);
				}
				return TRUE;
			}

			return TRUE;
		}
	}

	return FALSE;
}

LRESULT CALLBACK EmulationSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND cur;

			CheckDlgItem(hDlg,IDC_CHECKBOX_DEBUGGERMODE,CommonSettings.DebugConsole);
			CheckDlgItem(hDlg,IDC_CHECKBOX_ENSATAEMULATION,CommonSettings.EnsataEmulation);
			CheckDlgItem(hDlg,IDC_USEEXTBIOS,CommonSettings.UseExtBIOS);
			CheckDlgItem(hDlg, IDC_BIOSSWIS, CommonSettings.SWIFromBIOS);
			SetDlgItemText(hDlg, IDC_ARM9BIOS, CommonSettings.ARM9BIOS);
			SetDlgItemText(hDlg, IDC_ARM7BIOS, CommonSettings.ARM7BIOS);

			if(CommonSettings.UseExtBIOS == false)
			{
				cur = GetDlgItem(hDlg, IDC_ARM9BIOS);
				EnableWindow(cur, FALSE);
				cur = GetDlgItem(hDlg, IDC_ARM9BIOSBROWSE);
				EnableWindow(cur, FALSE);
				cur = GetDlgItem(hDlg, IDC_ARM7BIOS);
				EnableWindow(cur, FALSE);
				cur = GetDlgItem(hDlg, IDC_ARM7BIOSBROWSE);
				EnableWindow(cur, FALSE);
				cur = GetDlgItem(hDlg, IDC_BIOSSWIS);
				EnableWindow(cur, FALSE);
			}

			CheckDlgButton(hDlg, IDC_USEEXTFIRMWARE, ((CommonSettings.UseExtFirmware == true) ? BST_CHECKED : BST_UNCHECKED));
			SetDlgItemText(hDlg, IDC_FIRMWARE, CommonSettings.Firmware);
			CheckDlgButton(hDlg, IDC_FIRMWAREBOOT, ((CommonSettings.BootFromFirmware == true) ? BST_CHECKED : BST_UNCHECKED));

			if(CommonSettings.UseExtFirmware == false)
			{
				cur = GetDlgItem(hDlg, IDC_FIRMWARE);
				EnableWindow(cur, FALSE);
				cur = GetDlgItem(hDlg, IDC_FIRMWAREBROWSE);
				EnableWindow(cur, FALSE);
			}

			if((CommonSettings.UseExtBIOS == false) || (CommonSettings.UseExtFirmware == false))
			{
				cur = GetDlgItem(hDlg, IDC_FIRMWAREBOOT);
				EnableWindow(cur, FALSE);
			}
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDOK:
				{
					int val = 0;

					if(romloaded)
						val = MessageBox(hDlg, "The current ROM needs to be reset to apply changes.\nReset now ?", "DeSmuME", (MB_YESNO | MB_ICONQUESTION));

					HWND cur;

					CommonSettings.UseExtBIOS = IsDlgCheckboxChecked(hDlg, IDC_USEEXTBIOS);
					cur = GetDlgItem(hDlg, IDC_ARM9BIOS);
					GetWindowText(cur, CommonSettings.ARM9BIOS, 256);
					cur = GetDlgItem(hDlg, IDC_ARM7BIOS);
					GetWindowText(cur, CommonSettings.ARM7BIOS, 256);
					CommonSettings.SWIFromBIOS = IsDlgCheckboxChecked(hDlg, IDC_BIOSSWIS);

					CommonSettings.UseExtFirmware = IsDlgCheckboxChecked(hDlg, IDC_USEEXTFIRMWARE);
					cur = GetDlgItem(hDlg, IDC_FIRMWARE);
					GetWindowText(cur, CommonSettings.Firmware, 256);
					CommonSettings.BootFromFirmware = IsDlgCheckboxChecked(hDlg, IDC_FIRMWAREBOOT);

					CommonSettings.DebugConsole = IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_DEBUGGERMODE);
					CommonSettings.EnsataEmulation = IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_ENSATAEMULATION);

					WritePrivateProfileInt("Emulation", "DebugConsole", ((CommonSettings.DebugConsole == true) ? 1 : 0), IniName);
					WritePrivateProfileInt("Emulation", "EnsataEmulation", ((CommonSettings.EnsataEmulation == true) ? 1 : 0), IniName);
					WritePrivateProfileInt("BIOS", "UseExtBIOS", ((CommonSettings.UseExtBIOS == true) ? 1 : 0), IniName);
					WritePrivateProfileString("BIOS", "ARM9BIOSFile", CommonSettings.ARM9BIOS, IniName);
					WritePrivateProfileString("BIOS", "ARM7BIOSFile", CommonSettings.ARM7BIOS, IniName);
					WritePrivateProfileInt("BIOS", "SWIFromBIOS", ((CommonSettings.SWIFromBIOS == true) ? 1 : 0), IniName);

					WritePrivateProfileInt("Firmware", "UseExtFirmware", ((CommonSettings.UseExtFirmware == true) ? 1 : 0), IniName);
					WritePrivateProfileString("Firmware", "FirmwareFile", CommonSettings.Firmware, IniName);
					WritePrivateProfileInt("Firmware", "BootFromFirmware", ((CommonSettings.BootFromFirmware == true) ? 1 : 0), IniName);

					if(val == IDYES)
					{
						NDS_Reset();
					}
				}
			case IDCANCEL:
				{
					EndDialog(hDlg, TRUE);
				}
				return TRUE;

			case IDC_USEEXTBIOS:
				{
					HWND cur;
					BOOL enable = IsDlgButtonChecked(hDlg, IDC_USEEXTBIOS);

					cur = GetDlgItem(hDlg, IDC_ARM9BIOS);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_ARM9BIOSBROWSE);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_ARM7BIOS);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_ARM7BIOSBROWSE);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_BIOSSWIS);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_FIRMWAREBOOT);
					EnableWindow(cur, (enable && IsDlgButtonChecked(hDlg, IDC_USEEXTFIRMWARE)));
				}
				return TRUE;

			case IDC_USEEXTFIRMWARE:
				{
					HWND cur;
					BOOL enable = IsDlgButtonChecked(hDlg, IDC_USEEXTFIRMWARE);

					cur = GetDlgItem(hDlg, IDC_FIRMWARE);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_FIRMWAREBROWSE);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_FIRMWAREBOOT);
					EnableWindow(cur, (enable && IsDlgButtonChecked(hDlg, IDC_USEEXTBIOS)));
				}
				return TRUE;

			case IDC_ARM9BIOSBROWSE:
			case IDC_ARM7BIOSBROWSE:
			case IDC_FIRMWAREBROWSE:
				{
					char fileName[256] = "";
					OPENFILENAME ofn;

					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hDlg;
					ofn.lpstrFilter = "Binary file (*.bin)\0*.bin\0ROM file (*.rom)\0*.rom\0Any file(*.*)\0*.*\0\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFile = fileName;
					ofn.nMaxFile = 256;
					ofn.lpstrDefExt = "bin";
					ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

					char buffer[MAX_PATH];
					ZeroMemory(buffer, sizeof(buffer));
					path.getpath(path.FIRMWARE, buffer);
					ofn.lpstrInitialDir = buffer;

					if(GetOpenFileName(&ofn))
					{
						HWND cur;

						switch(LOWORD(wParam))
						{
						case IDC_ARM9BIOSBROWSE: cur = GetDlgItem(hDlg, IDC_ARM9BIOS); break;
						case IDC_ARM7BIOSBROWSE: cur = GetDlgItem(hDlg, IDC_ARM7BIOS); break;
						case IDC_FIRMWAREBROWSE: cur = GetDlgItem(hDlg, IDC_FIRMWARE); break;
						}

						SetWindowText(cur, fileName);
					}
				}
				return TRUE;
			}
		}
		return TRUE;
	}
	
	return FALSE;
}

LRESULT CALLBACK MicrophoneSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND cur;

			CommonSettings.micMode = (TCommonSettings::MicMode)GetPrivateProfileInt("MicSettings", "MicMode", (int)TCommonSettings::InternalNoise, IniName);
			CheckDlgButton(hDlg, IDC_USEMICSAMPLE, ((CommonSettings.micMode == TCommonSettings::Sample) ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hDlg, IDC_USEMICRAND, ((CommonSettings.micMode == TCommonSettings::Random) ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hDlg, IDC_USENOISE, ((CommonSettings.micMode == TCommonSettings::InternalNoise) ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hDlg, IDC_USEPHYSICAL, ((CommonSettings.micMode == TCommonSettings::Physical) ? BST_CHECKED : BST_UNCHECKED));
			GetPrivateProfileString("MicSettings", "MicSampleFile", "micsample.raw", MicSampleName, MAX_PATH, IniName);
			SetDlgItemText(hDlg, IDC_MICSAMPLE, MicSampleName);

			if(CommonSettings.micMode != TCommonSettings::Sample)
			{
				cur = GetDlgItem(hDlg, IDC_MICSAMPLE);
				EnableWindow(cur, FALSE);
				cur = GetDlgItem(hDlg, IDC_MICSAMPLEBROWSE);
				EnableWindow(cur, FALSE);
			}
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDOK:
				{
					int val = 0;

					HWND cur;

					if(IsDlgCheckboxChecked(hDlg, IDC_USEMICSAMPLE))
						CommonSettings.micMode = TCommonSettings::Sample;
					else if(IsDlgCheckboxChecked(hDlg, IDC_USEMICRAND))
						CommonSettings.micMode = TCommonSettings::Random;
					else if(IsDlgCheckboxChecked(hDlg, IDC_USENOISE))
						CommonSettings.micMode = TCommonSettings::InternalNoise;
					else if(IsDlgCheckboxChecked(hDlg, IDC_USEPHYSICAL))
						CommonSettings.micMode = TCommonSettings::Physical;

					cur = GetDlgItem(hDlg, IDC_MICSAMPLE);
					GetWindowText(cur, MicSampleName, 256);
	
					WritePrivateProfileInt("MicSettings", "MicMode", (int)CommonSettings.micMode, IniName);
					WritePrivateProfileString("MicSettings", "MicSampleFile", MicSampleName, IniName);
					RefreshMicSettings();
				}
			case IDCANCEL:
				{
					EndDialog(hDlg, TRUE);
				}
				return TRUE;

			case IDC_USENOISE:
			case IDC_USEMICRAND:
			case IDC_USEMICSAMPLE:
				{
					HWND cur;
					BOOL enable = IsDlgButtonChecked(hDlg, IDC_USEMICSAMPLE);

					cur = GetDlgItem(hDlg, IDC_MICSAMPLE);
					EnableWindow(cur, enable);
					cur = GetDlgItem(hDlg, IDC_MICSAMPLEBROWSE);
					EnableWindow(cur, enable);
				}
				return TRUE;

			case IDC_MICSAMPLEBROWSE:
				{
					char fileName[256] = "";
					OPENFILENAME ofn;

					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hDlg;
					ofn.lpstrFilter = "8bit PCM mono WAV file(*.wav)\0*.wav\0\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFile = fileName;
					ofn.nMaxFile = 256;
					ofn.lpstrDefExt = "wav";
					ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

					char buffer[MAX_PATH];
					ZeroMemory(buffer, sizeof(buffer));
					path.getpath(path.SOUNDS, buffer);
					ofn.lpstrInitialDir = buffer;

					if(GetOpenFileName(&ofn))
					{
						HWND cur;

						switch(LOWORD(wParam))
						{
						case IDC_MICSAMPLEBROWSE: cur = GetDlgItem(hDlg, IDC_MICSAMPLE); break;
						}

						SetWindowText(cur, fileName);
					}
				}
				return TRUE;
			}
		}
		return TRUE;
	}
	
	return FALSE;
}

LRESULT CALLBACK WifiSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	#ifdef EXPERIMENTAL_WIFI_COMM
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			char errbuf[PCAP_ERRBUF_SIZE];
			pcap_if_t *alldevs;
			pcap_if_t *d;
			int i;
			HWND cur;

			CheckRadioButton(hDlg, IDC_WIFIMODE0, IDC_WIFIMODE1, IDC_WIFIMODE0 + CommonSettings.wifi.mode);

			if(PCAP::pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1)
			{
				// TODO: fail more gracefully!
				EndDialog(hDlg, TRUE);
				return TRUE;
			}

			cur = GetDlgItem(hDlg, IDC_BRIDGEADAPTER);
			for(i = 0, d = alldevs; d != NULL; i++, d = d->next)
			{
				ComboBox_AddString(cur, d->description);
			}
			ComboBox_SetCurSel(cur, CommonSettings.wifi.infraBridgeAdapter);
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDOK:
				{
					int val = IDNO;
					HWND cur;

					if(romloaded)
						val = MessageBox(hDlg, "The current ROM needs to be reset to apply changes.\nReset now ?", "DeSmuME", (MB_YESNO | MB_ICONQUESTION));

					if (IsDlgButtonChecked(hDlg, IDC_WIFIMODE0))
						CommonSettings.wifi.mode = 0;
					else
						CommonSettings.wifi.mode = 1;
					WritePrivateProfileInt("Wifi", "Mode", CommonSettings.wifi.mode, IniName);

					cur = GetDlgItem(hDlg, IDC_BRIDGEADAPTER);
					CommonSettings.wifi.infraBridgeAdapter = ComboBox_GetCurSel(cur);
					WritePrivateProfileInt("Wifi", "BridgeAdapter", CommonSettings.wifi.infraBridgeAdapter, IniName);

					if(val == IDYES)
					{
						NDS_Reset();
					}
				}
			case IDCANCEL:
				{
					EndDialog(hDlg, TRUE);
				}
				return TRUE;
			}
		}
		return TRUE;
	}
#endif
	return FALSE;
}

static void SoundSettings_updateVolumeReadout(HWND hDlg)
{
	SetDlgItemInt(hDlg,IDC_VOLUME,SendMessage(GetDlgItem(hDlg,IDC_SLVOLUME),TBM_GETPOS,0,0),FALSE);
}

static void SoundSettings_updateSynchMode(HWND hDlg)
{
	BOOL en = IsDlgCheckboxChecked(hDlg,IDC_SYNCHMODE_SYNCH)?TRUE:FALSE;
	EnableWindow(GetDlgItem(hDlg,IDC_GROUP_SYNCHMETHOD),en);
	EnableWindow(GetDlgItem(hDlg,IDC_SYNCHMETHOD_N),en);
	EnableWindow(GetDlgItem(hDlg,IDC_SYNCHMETHOD_Z),en);
	EnableWindow(GetDlgItem(hDlg,IDC_SYNCHMETHOD_P),en);
}

static LRESULT CALLBACK SoundSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static UINT_PTR timerid=0;
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			int i;
			char tempstr[MAX_PATH];
			
			// Setup Sound Core Combo box
			SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_ADDSTRING, 0, (LPARAM)"None");

			for (i = 1; SNDCoreList[i] != NULL; i++)
				SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_ADDSTRING, 0, (LPARAM)SNDCoreList[i]->Name);

			// Set Selected Sound Core
			for (i = 0; SNDCoreList[i] != NULL; i++)
			{
				if (sndcoretype == SNDCoreList[i]->id)
					SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_SETCURSEL, i, 0);
			}

			//update the synch mode
			CheckDlgItem(hDlg,IDC_SYNCHMODE_DUAL,snd_synchmode==0);
			CheckDlgItem(hDlg,IDC_SYNCHMODE_SYNCH,snd_synchmode==1);
			SoundSettings_updateSynchMode(hDlg);

			//update the synch method
			CheckDlgItem(hDlg,IDC_SYNCHMETHOD_N,snd_synchmethod==0);
			CheckDlgItem(hDlg,IDC_SYNCHMETHOD_Z,snd_synchmethod==1);
			CheckDlgItem(hDlg,IDC_SYNCHMETHOD_P,snd_synchmethod==2);

			//setup interpolation combobox
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"None (fastest, sounds bad)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"Linear (typical, sounds good)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"Cosine (slowest, sounds best)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_SETCURSEL, (int)CommonSettings.spuInterpolationMode, 0);

			// Setup Sound Buffer Size Edit Text
			sprintf(tempstr, "%d", sndbuffersize);
			SetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr);

			// Setup Volume Slider
			SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_SETRANGE, 0, MAKELONG(0, 100));

			// Set Selected Volume
			SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_SETPOS, TRUE, sndvolume);
			SoundSettings_updateVolumeReadout(hDlg);

			timerid = SetTimer(hDlg, 1, 500, NULL);
			return TRUE;
		}
	case WM_HSCROLL:
		SoundSettings_updateVolumeReadout(hDlg);
		break;

	case WM_TIMER:
		{
			if (timerid == wParam)
			{
				int setting;
				setting = SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_GETPOS, 0, 0);
				SPU_SetVolume(setting);
				break;
			}
			break;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDC_SYNCHMODE_DUAL:
			case IDC_SYNCHMODE_SYNCH:
				SoundSettings_updateSynchMode(hDlg);
				break;

			case IDOK:
				{
					char tempstr[MAX_PATH];

					EndDialog(hDlg, TRUE);

					// Write Sound core type
					sndcoretype = SNDCoreList[SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_GETCURSEL, 0, 0)]->id;
					sprintf(tempstr, "%d", sndcoretype);
					WritePrivateProfileString("Sound", "SoundCore2", tempstr, IniName);

					// Write Sound Buffer size
					int tmp_size_buf = sndbuffersize;
					GetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr, 6);
					sscanf(tempstr, "%d", &sndbuffersize);
					WritePrivateProfileString("Sound", "SoundBufferSize2", tempstr, IniName);

					if( (sndcoretype != SPU_currentCoreNum) || (sndbuffersize != tmp_size_buf) )
					{
						Lock lock;
						SPU_ChangeSoundCore(sndcoretype, sndbuffersize);
					}

					// Write Volume
					sndvolume = SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_GETPOS, 0, 0);
					sprintf(tempstr, "%d", sndvolume);
					WritePrivateProfileString("Sound", "Volume", tempstr, IniName);
					SPU_SetVolume(sndvolume);

					//save the synch mode
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMODE_DUAL)) snd_synchmode = 0;
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMODE_SYNCH)) snd_synchmode = 1;
					WritePrivateProfileInt("Sound", "SynchMode", snd_synchmode, IniName);

					//save the synch method
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMETHOD_N)) snd_synchmethod = 0;
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMETHOD_Z)) snd_synchmethod = 1;
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMETHOD_P)) snd_synchmethod = 2;
					WritePrivateProfileInt("Sound", "SynchMethod", snd_synchmethod, IniName);

					{
						Lock lock;
						SPU_SetSynchMode(snd_synchmode, snd_synchmethod);
					}

					//write interpolation type
					CommonSettings.spuInterpolationMode = (SPUInterpolationMode)SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_GETCURSEL, 0, 0);
					//SPU_SetInterpolationMode(CommonSettings.spuInterpolationMode);
					WritePrivateProfileInt("Sound","SPUInterpolation",(int)CommonSettings.spuInterpolationMode, IniName);

					return TRUE;
				}
			case IDCANCEL:
				{
					EndDialog(hDlg, FALSE);
					return TRUE;
				}
			default: break;
			}

			break;
		}
	case WM_DESTROY:
		{
			if (timerid != 0)
				KillTimer(hDlg, timerid);
			break;
		}
	}

	return FALSE;
}

void ResetGame()
{
	if(movieMode != MOVIEMODE_PLAY)
	{
		NDS_Reset();
		Unpause();
	}
}

//adelikat: This function changes a menu item's text
void ChangeMenuItemText(int menuitem, wstring text)
{
	MENUITEMINFOW moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_TYPE;
	moo.cch = NULL;
	if(GetMenuItemInfoW(mainMenu, menuitem, FALSE, &moo))
	{
		moo.dwTypeData = (LPWSTR)text.c_str();
		SetMenuItemInfoW(mainMenu, menuitem, FALSE, &moo);
	}
}

const char* GetModifierString(int num)
{
	switch (num)
	{
	case 0:
		return "";
	case 1:
		return "Alt+";
	case 2:
		return "Ctrl+";
	case 3:
		return "Ctrl+Alt+";
	case 4:
		return "Shift+";
	case 5:
		return "Alt+Shift+";
	case 6:
		return "Ctrl+Shift+";
	case 7:
		return "Ctrl+Alt+Shift+";
	default:
		return "";
	}
}

static void TranslateKeyForMenu(WORD keyz,char *out)
{
	if(keyz == 0 || keyz == VK_ESCAPE)
	{
		// because it doesn't make sense to list a menu item as "disabled"
		// when actually only its hotkey is disabled
		*out = 0;
	}
	else
	{
		void TranslateKey(WORD keyz,char *out);	//adelikat: Yeah hackey
		TranslateKey(keyz,out);
	}
}

void UpdateHotkeyAssignment(const SCustomKey& hotkey, DWORD menuItemId)
{
	//Update all menu items that can be called from a hotkey to include the current hotkey assignment
	char str[256];		//Temp string 
	wchar_t wstr[256];	//Temp string 
	wstring text;		//Used to manipulate menu item text
	wstring keyname;	//Used to hold the name of the hotkey
	int truncate;		//Used to truncate the hotkey config from the menu item
	
	HMENU menu = GetMenuItemParent(menuItemId);
	if(!GetMenuStringW(menu,menuItemId, wstr, 255, MF_BYCOMMAND))	//Get menu item text
		return;
	text = wstr;											//Store in string object
	truncate = text.find(L"\t");							//Find the tab
	if (truncate >= 1)										//Truncate if it exists
		text = text.substr(0,truncate);
	TranslateKeyForMenu(hotkey.key, str);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, -1, wstr, 255);
	keyname = wstr;
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, GetModifierString(hotkey.modifiers), -1, wstr, 255);
	keyname.insert(0,wstr);
	text.append(L"\t" + keyname);
	ChangeMenuItemText(menuItemId, text);
}

//adelikat:  This function find the current hotkey assignments for corresponding menu items
//			 and appends it to the menu item.  This function works correctly for all language menus
//		     However, a Menu item in the Resource file must NOT have a /t (tab) in its caption.
void UpdateHotkeyAssignments()
{
	UpdateHotkeyAssignment(CustomKeys.OpenROM, IDM_OPEN);
	UpdateHotkeyAssignment(CustomKeys.PrintScreen, IDM_PRINTSCREEN);
	UpdateHotkeyAssignment(CustomKeys.RecordAVI, IDM_FILE_RECORDAVI);
	UpdateHotkeyAssignment(CustomKeys.Pause, IDM_PAUSE);
	UpdateHotkeyAssignment(CustomKeys.Reset, IDM_RESET);
	UpdateHotkeyAssignment(CustomKeys.ToggleFrameCounter, ID_VIEW_FRAMECOUNTER);
	UpdateHotkeyAssignment(CustomKeys.ToggleFPS, ID_VIEW_DISPLAYFPS);
	UpdateHotkeyAssignment(CustomKeys.ToggleInput, ID_VIEW_DISPLAYINPUT);
	UpdateHotkeyAssignment(CustomKeys.ToggleLag, ID_VIEW_DISPLAYLAG);
	UpdateHotkeyAssignment(CustomKeys.PlayMovie, IDM_PLAY_MOVIE);
	UpdateHotkeyAssignment(CustomKeys.RecordMovie, IDM_RECORD_MOVIE);
	UpdateHotkeyAssignment(CustomKeys.StopMovie, IDM_STOPMOVIE);
	UpdateHotkeyAssignment(CustomKeys.RecordWAV, IDM_FILE_RECORDWAV);
	UpdateHotkeyAssignment(CustomKeys.NewLuaScript, IDC_NEW_LUA_SCRIPT);
	UpdateHotkeyAssignment(CustomKeys.MostRecentLuaScript, IDD_LUARECENT_RESERVE_START);
	UpdateHotkeyAssignment(CustomKeys.CloseLuaScripts, IDC_CLOSE_LUA_SCRIPTS);
	for(int i = 0; i < 10; i++)
		UpdateHotkeyAssignment(CustomKeys.Save[(i+1)%10], IDM_STATE_SAVE_F1 + i);
	for(int i = 0; i < 10; i++)
		UpdateHotkeyAssignment(CustomKeys.Load[(i+1)%10], IDM_STATE_LOAD_F1 + i);
}


static char Lua_Dir [1024];
char Desmume_Path [1024];

static const char* PathWithoutPrefixDotOrSlash(const char* path)
{
	while(*path &&
	  ((*path == '.' && (path[1] == '\\' || path[1] == '/')) ||
	  *path == '\\' || *path == '/' || *path == ' '))
		path++;
	return path;
}

const char* MakeScriptPathAbsolute(const char* filename, const char* extraDirToCheck)
{
	static char filename2 [1024];
	if(filename[0] && filename[1] != ':')
	{
		char tempFile [1024], curDir [1024];
		strncpy(tempFile, filename, 1024);
		tempFile[1023] = 0;
		const char* tempFilePtr = PathWithoutPrefixDotOrSlash(tempFile);
		for(int i=0; i<=4; i++)
		{
			if((!*tempFilePtr || tempFilePtr[1] != ':') && i != 2)
				strcpy(curDir, i!=1 ? ((i!=3||!extraDirToCheck) ? Lua_Dir : extraDirToCheck) : Desmume_Path);
			else
				curDir[0] = 0;
			_snprintf(filename2, 1024, "%s%s", curDir, tempFilePtr);
			char* bar = strchr(filename2, '|');
			if(bar) *bar = 0;
			FILE* file = fopen(filename2, "rb");
			if(bar) *bar = '|';
			if(file || i==4)
				filename = filename2;
			if(file)
			{
				fclose(file);
				break;
			}
		}
	}
	return filename;
}

extern void RequestAbortLuaScript(int uid, const char* message);

const char* OpenLuaScript(const char* filename, const char* extraDirToCheck, bool makeSubservient)
{
	if(LuaScriptHWnds.size() < 16)
	{
		// make the filename absolute before loading
		filename = MakeScriptPathAbsolute(filename, extraDirToCheck);

		// now check if it's already open and load it if it isn't
		HWND IsScriptFileOpen(const char* Path);
		HWND scriptHWnd = IsScriptFileOpen(filename);
		if(!scriptHWnd)
		{
			HWND prevWindow = GetActiveWindow();

			HWND hDlg = CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
			SendMessage(hDlg,WM_COMMAND,IDC_NOTIFY_SUBSERVIENT,TRUE);
			SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)filename);

			SetActiveWindow(prevWindow);
		}
		else
		{
			RequestAbortLuaScript((int)scriptHWnd, "terminated to restart because of a call to emu.openscript");
			SendMessage(scriptHWnd, WM_COMMAND, IDC_BUTTON_LUARUN, 0);
		}
	}
	else return "Too many script windows are already open.";

	return NULL;
}

//TODO maybe should be repurposed into an entire WIN_InstallAddon
void WIN_InstallCFlash()
{
	//install cflash values into emulator
	if(win32_CFlash_cfgMode==ADDON_CFLASH_MODE_Path) 
	{
		CFlash_Path = win32_CFlash_cfgDirectory;
		CFlash_Mode = ADDON_CFLASH_MODE_Path;
	}
	else 
		if(win32_CFlash_cfgMode==ADDON_CFLASH_MODE_File)
		{
			CFlash_Path = win32_CFlash_cfgFileName;
			CFlash_Mode = ADDON_CFLASH_MODE_File;
		}
		else 
			{
				CFlash_Path = "";
				CFlash_Mode = ADDON_CFLASH_MODE_RomPath;
			}

}