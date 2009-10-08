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

#include "windriver.h"
#include <algorithm>
#include <shellapi.h>
#include <shlwapi.h>
#include <Winuser.h>
#include <Winnls.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>
#include <tchar.h>
#include "gthread.h"
#include "version.h"
#include "CWindow.h"
#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../debug.h"
#include "../saves.h"
#include "../addons.h"
#include "resource.h"
#include "GPU_osd.h"
#include "memView.h"
#include "disView.h"
#include "ginfo.h"
#include "IORegView.h"
#include "palView.h"
#include "tileView.h"
#include "oamView.h"
#include "mapview.h"
#include "matrixview.h"
#include "lightview.h"
#include "inputdx.h"
#include "FirmConfig.h"
#include "AboutBox.h"
#include "OGLRender.h"
#include "rasterize.h"
#include "../gfx3d.h"
#include "../render3D.h"
#include "../gdbstub.h"
#include "colorctrl.h"
#include "console.h"
#include "throttle.h"
#include "gbaslot_config.h"
#include "cheatsWin.h"
#include "Mmsystem.h"
#include "../mic.h"
#include "../common.h"
#include "main.h"
#include "hotkey.h"
#include "../movie.h"
#include "../replay.h"
#include "snddx.h"
#include "ramwatch.h"
#include "ram_search.h"
#include "aviout.h"
#include "soundView.h"
#include "commandline.h"
#include "../lua-engine.h"
#include "7zip.h"
#include "pathsettings.h"
#include "utils/xstring.h"
#include "directx/ddraw.h"
#include "video.h"
#include "path.h"

#include "aggdraw.h"
#include "agg2d.h"

using namespace std;

#define HAVE_REMOTE
#define WPCAP
#define PACKET_SIZE 65535

#include <pcap.h>
#include <remote-ext.h> //uh?

VideoInfo video;

//#define WX_STUB

#ifdef WX_STUB

// for compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

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

class wxTestModeless : public wxFrame
{
public:
	wxTestModeless(const wxChar *title, int x, int y)
       : wxFrame(NULL, wxID_ANY, title, wxPoint(x, y), wxSize(700, 450))    
	{}
};

void wxTest() {
    wxTestModeless *frame = new wxTestModeless(_T("Controls wxWidgets App"), 50, 50);
    frame->Show(true);
}

#endif

const int kGapNone = 0;
const int kGapBorder = 5;
const int kGapNDS = 64; // extremely tilted (but some games seem to use this value)
const int kGapNDS2 = 90; // more normal viewing angle

static BOOL OpenCore(const char* filename);

//----Recent ROMs menu globals----------
vector<string> RecentRoms;					//The list of recent ROM filenames
const unsigned int MAX_RECENT_ROMS = 10;	//To change the recent rom max, simply change this number
const unsigned int clearid = IDM_RECENT_RESERVED0;			// ID for the Clear recent ROMs item
const unsigned int baseid = IDM_RECENT_RESERVED1;			//Base identifier for the recent ROMs items
static HMENU recentromsmenu;				//Handle to the recent ROMs submenu
//--------------------------------------

void UpdateHotkeyAssignments();				//Appends hotkey mappings to corresponding menu items

static HMENU mainMenu; //Holds handle to the main DeSmuME menu

DWORD hKeyInputTimer;

extern LRESULT CALLBACK RamSearchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitRamSearch();
void FilterUpdate(HWND hwnd, bool user=true);

CRITICAL_SECTION win_execute_sync;
volatile int win_sound_samplecounter = 0;

CRITICAL_SECTION win_backbuffer_sync;

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


inline bool IsDlgCheckboxChecked(HWND hDlg, int id)
{
	return IsDlgButtonChecked(hDlg,id) == BST_CHECKED;
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
RECT MainScreenRect, SubScreenRect, GapRect;
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

void SetRotate(HWND hwnd, int rot);

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

//static char IniName[MAX_PATH];
int sndcoretype=SNDCORE_DIRECTX;
int sndbuffersize=735*4;
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

bool autoframeskipenab=0;
int frameskiprate=0;
int lastskiprate=0;
int emu_paused = 0;
bool frameAdvance = false;
bool staterewindingenabled = false;

bool UseMicSample = false;
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
static int KeyInRepeatMSec = 16;

VOID CALLBACK KeyInputTimer( UINT idEvent, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
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

	for (int i = 0; i < 256; i++) {
		bool active = !S9xGetState(i);

		if (active) {
			bool keyRepeat = (currentTime - joyState[i].firstPressedTime) >= (DWORD)KeyInDelayMSec;
			if (!joyState[i].wasPressed || keyRepeat) {
				if (!joyState[i].wasPressed)
					joyState[i].firstPressedTime = currentTime;
				joyState[i].lastPressedTime = currentTime;
				if (keyRepeat && joyState[i].repeatCount < 0xffff)
					joyState[i].repeatCount++;
				PostMessage(MainWindow->getHWnd(), WM_CUSTKEYDOWN, (WPARAM)(i),(LPARAM)(joyState[i].repeatCount | (joyState[i].wasPressed ? 0x40000000 : 0)));
			}
		}
		else {
			joyState[i].repeatCount = 1;
			if (joyState[i].wasPressed)
				PostMessage(MainWindow->getHWnd(), WM_CUSTKEYUP, (WPARAM)(i),(LPARAM)(joyState[i].repeatCount | (joyState[i].wasPressed ? 0x40000000 : 0)));
		}
		joyState[i].wasPressed = active;
	}
	lastTime = currentTime;
}

void ScaleScreen(float factor)
{
	if(windowSize == 0)
	{
		int defw = GetPrivateProfileInt("Video", "Window width", 256, IniName);
		int defh = GetPrivateProfileInt("Video", "Window height", 384, IniName);

		// fix for wrong rotation
		int w1x = video.rotatedwidthgap();
		int h1x = video.rotatedheightgap();
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
		if(factor==65535)
			factor = 1.5f;
		else if(factor==65534)
			factor = 2.5f;
		MainWindow->setClientSize((int)(video.rotatedwidthgap() * factor), (int)(video.rotatedheightgap() * factor));
	}
}

void SetMinWindowSize()
{
	if(ForceRatio)
		MainWindow->setMinSize(video.rotatedwidth(), video.rotatedheight());
	else
		MainWindow->setMinSize(video.rotatedwidthgap(), video.rotatedheightgap());
}

void UnscaleScreenCoords(s32& x, s32& y)
{
	RECT r;
	HWND hwnd = MainWindow->getHWnd();
	GetClientRect(hwnd,&r);
	int defwidth = video.width, defheight = (video.height+video.screengap);
	int winwidth = (r.right-r.left), winheight = (r.bottom-r.top);

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

	x = x/video.ratio();
	y = y/video.ratio();
}

// input x,y should be windows client-space coords already at 1x scaling.
// output is in pixels relative to the top-left of the chosen screen.
// the gap between screens (if any) is subtracted away from the output y.
void ToDSScreenRelativeCoords(s32& x, s32& y, bool bottomScreen)
{
	s32 tx=x, ty=y;
	int gapSize = video.screengap / video.ratio();

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

	// finally, make it relative to the correct screen
	if(bottomScreen)
		y -= 192;
}

// END Rotation definitions

void UpdateRecentRomsMenu()
{
	//This function will be called to populate the Recent Menu
	//The array must be in the proper order ahead of time

	//UpdateRecentRoms will always call this
	//This will be always called by GetRecentRoms on DesMume startup


	//----------------------------------------------------------------------
	//Get Menu item info

	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(mainMenu, 0), ID_FILE_RECENTROM, FALSE, &moo);
	moo.hSubMenu = GetSubMenu(recentromsmenu, 0);
	//moo.fState = RecentRoms[0].c_str() ? MFS_ENABLED : MFS_GRAYED;
	moo.fState = MFS_ENABLED;
	SetMenuItemInfo(GetSubMenu(mainMenu, 0), ID_FILE_RECENTROM, FALSE, &moo);

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
	//Clear the current menu items
	for(int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		DeleteMenu(GetSubMenu(recentromsmenu, 0), baseid + x, MF_BYCOMMAND);
	}

	if(RecentRoms.size() == 0)
	{
		EnableMenuItem(GetSubMenu(recentromsmenu, 0), clearid, MF_GRAYED);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;

		moo.cch = 5;
		moo.fType = 0;
		moo.wID = baseid;
		moo.dwTypeData = "None";
		moo.fState = MF_GRAYED;

		InsertMenuItem(GetSubMenu(recentromsmenu, 0), 0, TRUE, &moo);

		return;
	}

	EnableMenuItem(GetSubMenu(recentromsmenu, 0), clearid, MF_ENABLED);
	DeleteMenu(GetSubMenu(recentromsmenu, 0), baseid, MF_BYCOMMAND);

	HDC dc = GetDC(MainWindow->getHWnd());

	//-----------------------------------------------------------------------
	//Update the list using RecentRoms vector
	for(int x = RecentRoms.size()-1; x >= 0; x--)	//Must loop in reverse order since InsertMenuItem will insert as the first on the list
	{
		string tmp = RecentRoms[x];
		LPSTR tmp2 = (LPSTR)tmp.c_str();

		PathCompactPath(dc, tmp2, 500);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = tmp2;
		//LOG("Inserting: %s\n",tmp.c_str());  //Debug
		InsertMenuItem(GetSubMenu(recentromsmenu, 0), 0, 1, &moo);
	}

	ReleaseDC(MainWindow->getHWnd(), dc);
	//-----------------------------------------------------------------------

	HWND temp = MainWindow->getHWnd();
	DrawMenuBar(temp);
}

void UpdateRecentRoms(const char* filename)
{
	//This function assumes filename is a ROM filename that was successfully loaded

	string newROM = filename; //Convert to std::string

	//--------------------------------------------------------------
	//Check to see if filename is in list
	vector<string>::iterator x;
	vector<string>::iterator theMatch;
	bool match = false;
	for (x = RecentRoms.begin(); x < RecentRoms.end(); ++x)
	{
		if (newROM == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentRoms.erase(theMatch);

	RecentRoms.insert(RecentRoms.begin(), newROM);	//Add to the array

	//If over the max, we have too many, so remove the last entry
	if (RecentRoms.size() > MAX_RECENT_ROMS)	
		RecentRoms.pop_back();

	//Debug
	//for (int x = 0; x < RecentRoms.size(); x++)
	//	LOG("Recent ROM: %s\n",RecentRoms[x].c_str());

	UpdateRecentRomsMenu();
}

void RemoveRecentRom(std::string filename)
{

	vector<string>::iterator x;
	vector<string>::iterator theMatch;
	bool match = false;
	for (x = RecentRoms.begin(); x < RecentRoms.end(); ++x)
	{
		if (filename == *x)
		{
			//We have a match
			match = true;	//Flag that we have a match
			theMatch = x;	//Hold on to the iterator	(Note: the loop continues, so if for some reason we had a duplicate (which wouldn't happen under normal circumstances, it would pick the last one in the list)
		}
	}
	//----------------------------------------------------------------
	//If there was a match, remove it
	if (match)
		RecentRoms.erase(theMatch);

	UpdateRecentRomsMenu();
}

void GetRecentRoms()
{
	//This function retrieves the recent ROMs stored in the .ini file
	//Then is populates the RecentRomsMenu array
	//Then it calls Update RecentRomsMenu() to populate the menu

	stringstream temp;
	char tempstr[256];

	// Avoids duplicating when changing the language.
	RecentRoms.clear();

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		temp.str("");
		temp << "Recent Rom " << (x+1);

		GetPrivateProfileString("General",temp.str().c_str(),"", tempstr, 256, IniName);
		if (tempstr[0])
			RecentRoms.push_back(tempstr);
	}
	UpdateRecentRomsMenu();
}

void SaveRecentRoms()
{
	//This function stores the RecentRomsMenu array to the .ini file

	stringstream temp;

	//Loops through all available recent slots
	for (int x = 0; x < MAX_RECENT_ROMS; x++)
	{
		temp.str("");
		temp << "Recent Rom " << (x+1);
		if (x < (int)RecentRoms.size())	//If it exists in the array, save it
			WritePrivateProfileString("General",temp.str().c_str(),RecentRoms[x].c_str(),IniName);
		else						//Else, make it empty
			WritePrivateProfileString("General",temp.str().c_str(), "",IniName);
	}
}

void ClearRecentRoms()
{
	RecentRoms.clear();
	SaveRecentRoms();
	UpdateRecentRomsMenu();
}

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

	return 1;
}


template<typename T, int bpp> static T realConvert(u16 val)
{
	switch(bpp)
	{
		case 24: case 32:
			return RGB15TO24_REVERSE(val);
		case 16: return RGB15TO16_REVERSE(val);
		default:
			return 0;
	}
}


template<typename T, int bpp> static FORCEINLINE T ddconvert(u32 val)
{
	//not supported yet, needs to drop from 32 to 16
	return val;
	//switch(bpp)
	//{
	//	case 24: case 32:
	//		return RGB15TO24_REVERSE(val);
	//	case 16: return RGB15TO16_REVERSE(val);
	//	default:
	//		return 0;
	//}
}

template<typename T, int bpp> static void doRotate(void* dst)
{
	u8* buffer = (u8*)dst;
	int size = video.size();
	u32* src = (u32*)video.finalBuffer();
	switch(video.rotation)
	{
	case 0:
	case 180:
		{
			if(ddsd.lPitch == 1024)
			{
				if(video.rotation==180)
					for(int i = 0, j=size-1; j>=0; i++,j--)
						((T*)buffer)[i] = ddconvert<T,bpp>((src)[j]);
				else
					for(int i = 0; i < size; i++)
						((T*)buffer)[i] = ddconvert<T,bpp>(src[i]);
			}
			else
			{
				if(video.rotation==180)
					for(int y = 0; y < video.height; y++)
					{
						for(int x = 0; x < video.width; x++)
							((T*)buffer)[x] = ddconvert<T,bpp>(src[video.height*video.width - (y * video.width) - x - 1]);

						buffer += ddsd.lPitch;
					}
				else
					for(int y = 0; y < video.height; y++)
					{
						for(int x = 0; x < video.width; x++)
							((T*)buffer)[x] = ddconvert<T,bpp>(src[(y * video.width) + x]);

						buffer += ddsd.lPitch;
					}
			}
		}
		break;
	case 90:
	case 270:
		{
			if(video.rotation == 90)
				for(int y = 0; y < video.width; y++)
				{
					for(int x = 0; x < video.height; x++)
						((T*)buffer)[x] = ddconvert<T,bpp>(src[(((video.height-1)-x) * video.width) + y]);

					buffer += ddsd.lPitch;
				}
			else
				for(int y = 0; y < video.width; y++)
				{
					for(int x = 0; x < video.height; x++)
						((T*)buffer)[x] = ddconvert<T,bpp>(src[((x) * video.width) + (video.width-1) - y]);

					buffer += ddsd.lPitch;
				}
		}
		break;
	}
}

//the directdraw final presentation portion of display, including rotating
static void DD_DoDisplay()
{
	int res;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;
	res = lpBackSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);

	if (res==DDERR_SURFACELOST)
	{
		LOG("DirectDraw buffers is lost\n");
		if (IDirectDrawSurface7_Restore(lpPrimarySurface)==DD_OK)
			IDirectDrawSurface7_Restore(lpBackSurface);
	} else if(res != DD_OK)
		return;

	char* buffer = (char*)ddsd.lpSurface;

	switch(ddsd.ddpfPixelFormat.dwRGBBitCount)
	{
	case 32: doRotate<u32,32>(ddsd.lpSurface); break;
	case 24: doRotate<u32,24>(ddsd.lpSurface); break;
	case 16: doRotate<u16,16>(ddsd.lpSurface); break;
	default:
		{
			INFO("Unsupported color depth: %i bpp\n", ddsd.ddpfPixelFormat.dwRGBBitCount);
			emu_halt();
		}
		break;
	}


	lpBackSurface->Unlock((LPRECT)ddsd.lpSurface);

	// Main screen
	if(lpPrimarySurface->Blt(&MainScreenRect, lpBackSurface, &MainScreenSrcRect, DDBLT_WAIT, 0) == DDERR_SURFACELOST)
	{
		LOG("DirectDraw buffers is lost\n");
		if(IDirectDrawSurface7_Restore(lpPrimarySurface) == DD_OK)
			IDirectDrawSurface7_Restore(lpBackSurface);
	}

	// Sub screen
	if(lpPrimarySurface->Blt(&SubScreenRect, lpBackSurface, &SubScreenSrcRect, DDBLT_WAIT, 0) == DDERR_SURFACELOST)
	{
		LOG("DirectDraw buffers is lost\n");
		if(IDirectDrawSurface7_Restore(lpPrimarySurface) == DD_OK)
			IDirectDrawSurface7_Restore(lpBackSurface);
	}

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

//tripple buffering logic
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
		if(CommonSettings.single_core)
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

	if(!CommonSettings.single_core)
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
	if(CommonSettings.single_core)
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
	u32 framecount;
	u64 onesecondticks;
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
		win_sound_samplecounter = 735;
		SPU_Emulate_user();
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
	if(CommonSettings.single_core && GetActiveWindow() == mainLoopData.hwnd)
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

	if(!mainLoopData.skipnextframe || forceFrameSkip == 0)
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
	else if((autoframeskipenab || FrameLimit) && allowSleep)
	{
		while(SpeedThrottle())
		{
		}
	}

	if (autoframeskipenab)
	{
		mainLoopData.framecount++;

		if (mainLoopData.framecount > 60)
		{
			mainLoopData.framecount = 1;
			mainLoopData.onesecondticks = 0;
		}

		QueryPerformanceCounter((LARGE_INTEGER *)&mainLoopData.curticks);
		mainLoopData.diffticks = mainLoopData.curticks - mainLoopData.lastticks;

		if(ThrottleIsBehind() && (mainLoopData.framesskipped < ffSkipRate))
		{
			mainLoopData.skipnextframe = 1;
			mainLoopData.framestoskip = 1;
		}

		mainLoopData.onesecondticks += mainLoopData.diffticks;
		mainLoopData.lastticks = mainLoopData.curticks;
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

		return TRUE;		
	}
	INFO("Loading %s FAILED.\n",logicalName);
	return FALSE;
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
	mainMenu = LoadMenu(hAppInst, "MENU_PRINCIPAL"); //Load Menu, and store handle
	if (!MainWindow->setMenu(mainMenu)) return 0;

	recentromsmenu = LoadMenu(hAppInst, "RECENTROMS");
	GetRecentRoms();

	ResetSaveStateTimes();

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
	case 2:
		// Danish
		setLanguage(MAKELCID(MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT), SORT_DEFAULT));
		break;
	case 3:
		// Chinese
		setLanguage(MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_DEFAULT));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), SORT_DEFAULT));
		break;

	default: break;
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
	for (i = IDC_LANGENGLISH; i < IDC_LANG_CHINESE_SIMPLIFIED+1; i++)
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
		#ifdef EXPERIMENTAL_WIFI
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
			return false ;
		#endif
	}
	virtual void WIFI_Host_ShutdownSystem() 
	{
		#ifdef EXPERIMENTAL_WIFI 	 
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
		if(!CommonSettings.single_core)
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
		{
			void Pause(); Pause();
		}
		else
		{
			void Unpause(); Unpause();
		}
	}

	virtual bool EMU_IsEmulationPaused()
	{
		return emu_paused;
	}

	virtual bool EMU_IsFastForwarding()
	{
		return FastForward;
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

int _main()
{
	InitDecoder();

#ifdef WX_STUB
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
	if(systemInfo.dwNumberOfProcessors==1)
		CommonSettings.single_core = true;
	else
		CommonSettings.single_core = false;


	char text[80];

	GetINIPath();

	addon_type = GetPrivateProfileInt("GBAslot", "type", NDS_ADDON_NONE, IniName);
	UINT CFlashFileMode = GetPrivateProfileInt("GBAslot.CFlash", "fileMode", 2, IniName);
	
	CFlashPath = GetPrivateProfileStdString("GBAslot.CFlash", "path", "");
	CFlashName = GetPrivateProfileStdString("GBAslot.CFlash", "filename", "");
	GetPrivateProfileString("GBAslot.GBAgame", "filename", "", GBAgameName, MAX_PATH, IniName);

	if(CFlashFileMode==ADDON_CFLASH_MODE_Path) 
	{
		CFlash_Mode = ADDON_CFLASH_MODE_Path;
	}
	else 
		if(CFlashFileMode==ADDON_CFLASH_MODE_File)
		{
			CFlash_Path = CFlashName;
			CFlash_Mode = ADDON_CFLASH_MODE_File;
		}
		else 
			{
				CFlash_Path = "";
				CFlash_Mode = ADDON_CFLASH_MODE_RomPath;
			}

	//init_configured_features( &my_config);
	/*if ( !fill_configured_features( &my_config, lpszArgument)) {
		MessageBox(NULL,"Unable to parse command line arguments","Error",MB_OK);
		return 0;
	}*/
	ColorCtrl_Register();

	if (!RegWndClass("DeSmuME", WindowProcedure, CS_DBLCLKS, LoadIcon(hAppInst, "ICONDESMUME")))
	{
		MessageBox(NULL, "Error registering windows class", "DeSmuME", MB_OK);
		exit(-1);
	}

	windowSize = GetPrivateProfileInt("Video","Window Size", 0, IniName);
	video.rotation =  GetPrivateProfileInt("Video","Window Rotate", 0, IniName);
	ForceRatio = GetPrivateProfileBool("Video","Window Force Ratio", 1, IniName);
	WndX = GetPrivateProfileInt("Video","WindowPosX", CW_USEDEFAULT, IniName);
	WndY = GetPrivateProfileInt("Video","WindowPosY", CW_USEDEFAULT, IniName);
	if(WndX < -10000) WndX = CW_USEDEFAULT; // fix for missing window problem
	if(WndY < -10000) WndY = CW_USEDEFAULT; // (happens if you close desmume while it's minimized)
	video.width = GetPrivateProfileInt("Video", "Width", 256, IniName);
	video.height = GetPrivateProfileInt("Video", "Height", 384, IniName);
	
	CommonSettings.hud.FpsDisplay = GetPrivateProfileBool("Display","Display Fps", false, IniName);
	CommonSettings.hud.FrameCounterDisplay = GetPrivateProfileBool("Display","FrameCounter", false, IniName);
	CommonSettings.hud.ShowInputDisplay = GetPrivateProfileBool("Display","Display Input", false, IniName);
	CommonSettings.hud.ShowGraphicalInputDisplay = GetPrivateProfileBool("Display","Display Graphical Input", false, IniName);
	CommonSettings.hud.ShowLagFrameCounter = GetPrivateProfileBool("Display","Display Lag Counter", false, IniName);
	CommonSettings.hud.ShowMicrophone = GetPrivateProfileBool("Display","Display Microphone", false, IniName);
	
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
	if(CommonSettings.single_core)
		SetProcessAffinityMask(GetCurrentProcess(),1);

	MainWindow = new WINCLASS(CLASSNAME, hAppInst);
	if (!MainWindow->create(DESMUME_NAME_AND_VERSION, WndX, WndY, video.width,video.height+video.screengap,
		WS_CAPTION| WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
		NULL))
	{
		MessageBox(NULL, "Error creating main window", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	gpu_SetRotateScreen(video.rotation);

	/* default the firmware settings, they may get changed later */
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

	SetMinWindowSize();

	ScaleScreen(windowSize);

	DragAcceptFiles(MainWindow->getHWnd(), TRUE);

#ifdef EXPERIMENTAL_WIFI
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
			CFlashPath = CFlash_Path;
		else 
			CFlashName = CFlash_Path;
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

	GetPrivateProfileString("Video", "FrameSkip", "0", text, 80, IniName);

	if (strcmp(text, "AUTO") == 0)
	{
		autoframeskipenab=1;
		frameskiprate=0;
		MainWindow->checkMenu(IDC_FRAMESKIPAUTO, true);
	}
	else
	{
		autoframeskipenab=0;
		frameskiprate=atoi(text);
		MainWindow->checkMenu(frameskiprate + IDC_FRAMESKIP0, true);
	}

	DWORD wmTimerRes;
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

	cur3DCore = GetPrivateProfileInt("3D", "Renderer", GPU3D_OPENGL, IniName);
	CommonSettings.HighResolutionInterpolateColor = GetPrivateProfileBool("3D", "HighResolutionInterpolateColor", 1, IniName);
	CommonSettings.gfx3d_flushMode = GetPrivateProfileInt("3D", "AlternateFlush", 0, IniName);
	NDS_3D_ChangeCore(cur3DCore);

#ifdef BETA_VERSION
	EnableMenuItem (mainMenu, IDM_SUBMITBUGREPORT, MF_GRAYED);
#endif
	LOG("Init sound core\n");
	sndcoretype = GetPrivateProfileInt("Sound","SoundCore2", SNDCORE_DIRECTX, IniName);
	sndbuffersize = GetPrivateProfileInt("Sound","SoundBufferSize", 735 * 4, IniName);
	CommonSettings.spuInterpolationMode = (SPUInterpolationMode)GetPrivateProfileInt("Sound","SPUInterpolation", 1, IniName);
	CommonSettings.spuAdpcmCache = GetPrivateProfileInt("Sound","SPUAdpcmCache",0,IniName)!=0;

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

	CommonSettings.DebugConsole = GetPrivateProfileBool("Emulation", "DebugConsole", FALSE, IniName);
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
	run();

	KillDisplay();

	SaveRecentRoms();
	DRV_AviEnd();
	WAV_End();

	NDS_DeInit();

	//------SHUTDOWN

#ifdef DEBUG
	//LogStop();
#endif

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
	timeBeginPeriod(1);
	g_thread_init (NULL);
	hAppInst=hThisInstance;
	OpenConsole();			// Init debug console

	int ret = _main();

	CloseConsole();

	return ret;
}

void UpdateWndRects(HWND hwnd)
{
	POINT ptClient;
	RECT rc;

	int wndWidth, wndHeight;
	int defHeight = (video.height + video.screengap);
	float ratio;
	int oneScreenHeight, gapHeight;

	GetClientRect(hwnd, &rc);

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

void UpdateScreenRects()
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
void SetRotate(HWND hwnd, int rot)
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
void WavRecordTo()
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
		WAV_Begin(szChoice);
	}

	NDS_UnPause();
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

#include "OpenArchive.h"
#include "utils/xstring.h"

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
		NDS_UnPause();
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

	//  To avoid #ifdef hell, we'll do a little trick, as lpstrFilter
	// needs 0 terminated string, and standard string library, of course,
	// can't help us with string creation: just put a '|' were a string end
	// should be, and later transform prior assigning to the OPENFILENAME structure
	strncpy (fileFilter, "NDS ROM file (*.nds)|*.nds|NDS/GBA ROM File (*.ds.gba)|*.ds.gba|",512);
#ifdef HAVE_LIBZZIP
	strncpy (fileFilter, "All Usable Files (*.nds, *.ds.gba, *.zip, *.gz, *.7z, *.rar, *.bz2)|*.nds;*.ds.gba;*.zip;*.gz;*.7z;*.rar;*.bz2|",512);
#endif			

#ifdef HAVE_LIBZZIP
	strncat (fileFilter, "Zipped NDS ROM file (*.zip)|*.zip|",512 - strlen(fileFilter));
#endif
#ifdef HAVE_LIBZ
	strncat (fileFilter, "GZipped NDS ROM file (*.gz)|*.gz|",512 - strlen(fileFilter));
#endif
	strncat (fileFilter, "7Zipped NDS ROM file (*.7z)|*.7z|",512 - strlen(fileFilter));
	strncat (fileFilter, "RARed NDS ROM file (*.rar)|*.rar|",512 - strlen(fileFilter));
	strncat (fileFilter, "BZipped NDS ROM file (*.bz2)|*.bz2|",512 - strlen(fileFilter));
	
	strncat (fileFilter, "Any file (*.*)|*.*||",512 - strlen(fileFilter));

	filterSize = strlen(fileFilter);
	for (i = 0; i < filterSize; i++)
	{
		if (fileFilter[i] == '|')	fileFilter[i] = '\0';
	}
	ofn.lpstrFilter = fileFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile =  filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "nds";
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

	char buffer[MAX_PATH];
	ZeroMemory(buffer, sizeof(buffer));
	path.getpath(path.ROMS, buffer);
	ofn.lpstrInitialDir = buffer;


	if (GetOpenFileName(&ofn) == NULL) {
		NDS_UnPause();
		return 0;
	}
	else {
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

//TODO - async key state? for real?
int GetModifiers(int key)
{
	int modifiers = 0;

	if (key == VK_MENU || key == VK_CONTROL || key == VK_SHIFT)
		return 0;

	if(GetAsyncKeyState(VK_MENU   )&0x8000) modifiers |= CUSTKEY_ALT_MASK;
	if(GetAsyncKeyState(VK_CONTROL)&0x8000) modifiers |= CUSTKEY_CTRL_MASK;
	if(GetAsyncKeyState(VK_SHIFT  )&0x8000) modifiers |= CUSTKEY_SHIFT_MASK;
	return modifiers;
}

int HandleKeyUp(WPARAM wParam, LPARAM lParam, int modifiers)
{
	SCustomKey *key = &CustomKeys.key(0);

	while (!IsLastCustomKey(key)) {
		if (wParam == key->key && modifiers == key->modifiers && key->handleKeyUp) {
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

		// don't pull down menu if alt is a hotkey or the menu isn't there, unless no game is running
		//if(!Settings.StopEmulation && ((wParam == VK_MENU || wParam == VK_F10) && (hitHotKey || GetMenu (GUI.hWnd) == NULL) && !GetAsyncKeyState(VK_F4)))
		/*if(((wParam == VK_MENU || wParam == VK_F10) && (hitHotKey || GetMenu (MainWindow->getHWnd()) == NULL) && !GetAsyncKeyState(VK_F4)))
			return 0;*/
		return 1;
	}

	return 1;
}

void Unpause()
{
	lastPauseFromLostFocus = FALSE;
	if (emu_paused) NDS_UnPause();
	emu_paused = 0;
}

void Pause()
{
	lastPauseFromLostFocus = FALSE;
	if (emu_paused) NDS_UnPause();
	else NDS_Pause();
	emu_paused ^= 1;
}

bool first;

void FrameAdvance(bool state)
{
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
		} else {
			// frame advance button still held down,
			// start or continue executing at normal speed
			execute = TRUE;
			frameAdvance = false;
		}
	} else {
		// frame advance button released
		first = true;
		frameAdvance = false;

		// pause immediately
		emu_halt();
		SPU_Pause(1);
		emu_paused = 1;
	}
}

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
		DialogBox(hAppInst,MAKEINTRESOURCE(IDD_FIRMSETTINGS), hwnd, (DLGPROC) FirmConfig_Proc);
		break;
	case CONFIGSCREEN_SOUND:
		DialogBox(hAppInst, MAKEINTRESOURCE(IDD_SOUNDSETTINGS), hwnd, (DLGPROC)SoundSettingsDlgProc);
		break;
	case CONFIGSCREEN_EMULATION:
		DialogBox(hAppInst, MAKEINTRESOURCE(IDD_EMULATIONSETTINGS), hwnd, (DLGPROC)EmulationSettingsDlgProc);
		break; 
	case CONFIGSCREEN_MICROPHONE:
		DialogBox(hAppInst, MAKEINTRESOURCE(IDD_MICROPHONE), hwnd, (DLGPROC)MicrophoneSettingsDlgProc);
		break;
	case CONFIGSCREEN_PATHSETTINGS:
		DialogBox(hAppInst, MAKEINTRESOURCE(IDD_PATHSETTINGS), hwnd, (DLGPROC)PathSettingsDlgProc);
		break;
	case CONFIGSCREEN_WIFI:
#ifdef EXPERIMENTAL_WIFI
		if(wifi_netEnabled)
		{
			DialogBox(hAppInst,MAKEINTRESOURCE(IDD_WIFISETTINGS), hwnd, (DLGPROC) WifiSettingsDlgProc);
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

void FilterUpdate (HWND hwnd, bool user)
{
	UpdateScreenRects();
	UpdateWndRects(hwnd);
	SetScreenGap(video.screengap);
	SetRotate(hwnd, video.rotation);
	if(user && windowSize==0) {}
	else ScaleScreen(windowSize);
	WritePrivateProfileInt("Video", "Filter", video.currentfilter, IniName);
	WritePrivateProfileInt("Video", "Width", video.width, IniName);
	WritePrivateProfileInt("Video", "Height", video.height, IniName);
}

void DesEnableMenuItem(HMENU hMenu, UINT uIDEnableItem, bool enable)
{
	EnableMenuItem(hMenu, uIDEnableItem, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
}

//========================================================================================
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
	static int tmp_execute;
	switch (message)                  // handle the messages
	{
		case WM_ENTERMENULOOP:		  //Update menu items that needs to be updated dynamically
		{
			SPU_Pause(1);

			UpdateHotkeyAssignments();	//Add current hotkey mappings to menu item names

			MENUITEMINFO mii;
			TCHAR menuItemString[256];
			ZeroMemory(&mii, sizeof(MENUITEMINFO));
			//Check if AVI is recording
			mii.cbSize = sizeof(MENUITEMINFO);
			mii.fMask = MIIM_STRING;
			LoadString(hAppInst, !AVI_IsRecording() ? IDM_FILE_RECORDAVI : IDM_FILE_STOPAVI, menuItemString, 256);
			mii.dwTypeData = menuItemString;
			SetMenuItemInfo(mainMenu, IDM_FILE_RECORDAVI, FALSE, &mii);
			//Check if WAV is recording
			LoadString(hAppInst, !WAV_IsRecording() ? IDM_FILE_RECORDWAV : IDM_FILE_STOPWAV, menuItemString, 256);
			SetMenuItemInfo(mainMenu, IDM_FILE_RECORDWAV, FALSE, &mii);

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
			DesEnableMenuItem(mainMenu, IDM_RESET,             romloaded);
			DesEnableMenuItem(mainMenu, IDM_CLOSEROM,          romloaded);
			DesEnableMenuItem(mainMenu, IDM_SHUT_UP,           romloaded);
			DesEnableMenuItem(mainMenu, IDM_CHEATS_LIST,       romloaded);
			DesEnableMenuItem(mainMenu, IDM_CHEATS_SEARCH,     romloaded);
			//DesEnableMenuItem(mainMenu, IDM_WIFISETTINGS,      romloaded);

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
			DesEnableMenuItem(mainMenu, ID_FILE_RECENTROM,      RecentRoms.size()>0);

			//Updated Checked menu items
			
			//Pause
			MainWindow->checkMenu(IDM_PAUSE, ((paused)));
			//Force Maintain Ratio
			MainWindow->checkMenu(IDC_FORCERATIO, ((ForceRatio)));
			//Screen rotation
			MainWindow->checkMenu(IDC_ROTATE0, ((video.rotation==0)));
			MainWindow->checkMenu(IDC_ROTATE90, ((video.rotation==90)));
			MainWindow->checkMenu(IDC_ROTATE180, ((video.rotation==180)));
			MainWindow->checkMenu(IDC_ROTATE270, ((video.rotation==270)));

			//Window Size
			MainWindow->checkMenu(IDC_WINDOW1X, ((windowSize==1)));
			MainWindow->checkMenu(IDC_WINDOW1_5X, ((windowSize==65535)));
			MainWindow->checkMenu(IDC_WINDOW2X, ((windowSize==2)));
			MainWindow->checkMenu(IDC_WINDOW2_5X, ((windowSize==65534)));
			MainWindow->checkMenu(IDC_WINDOW3X, ((windowSize==3)));
			MainWindow->checkMenu(IDC_WINDOW4X, ((windowSize==4)));

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
			MainWindow->checkMenu(IDC_FRAMESKIPAUTO, ((autoframeskipenab)));

			MainWindow->checkMenu(IDC_FRAMESKIP0, (!autoframeskipenab && frameskiprate==0) );
			MainWindow->checkMenu(IDC_FRAMESKIP1, (!autoframeskipenab && frameskiprate==1) );
			MainWindow->checkMenu(IDC_FRAMESKIP2, (!autoframeskipenab && frameskiprate==2) );
			MainWindow->checkMenu(IDC_FRAMESKIP3, (!autoframeskipenab && frameskiprate==3) );
			MainWindow->checkMenu(IDC_FRAMESKIP4, (!autoframeskipenab && frameskiprate==4) );
			MainWindow->checkMenu(IDC_FRAMESKIP5, (!autoframeskipenab && frameskiprate==5) );
			MainWindow->checkMenu(IDC_FRAMESKIP6, (!autoframeskipenab && frameskiprate==6) );
			MainWindow->checkMenu(IDC_FRAMESKIP7, (!autoframeskipenab && frameskiprate==7) );
			MainWindow->checkMenu(IDC_FRAMESKIP8, (!autoframeskipenab && frameskiprate==8) );
			MainWindow->checkMenu(IDC_FRAMESKIP9, (!autoframeskipenab && frameskiprate==9) );

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

			MainWindow->checkMenu(IDC_STATEREWINDING, staterewindingenabled == 1 );

			MainWindow->checkMenu(IDC_BACKGROUNDPAUSE, lostFocusPause);

			//Save type
			const int savelist[] = {IDC_SAVETYPE1,IDC_SAVETYPE2,IDC_SAVETYPE3,IDC_SAVETYPE4,IDC_SAVETYPE5,IDC_SAVETYPE6,IDC_SAVETYPE7};
			for(int i=0;i<7;i++) 
				MainWindow->checkMenu(savelist[i], false);
			MainWindow->checkMenu(savelist[CommonSettings.manualBackupType], true);

			// recent/active scripts menu
			PopulateLuaSubmenu();

			return 0;
		}
	case WM_EXITMENULOOP:
		{
			SPU_Pause(0);
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
					//	 WritePrivateProfileInt("Video","Window width",MainWindowRect.right-MainWindowRect.left+widthTradeOff,IniName);
					//	 WritePrivateProfileInt("Video","Window height",MainWindowRect.bottom-MainWindowRect.top+heightTradeOff,IniName);
					RECT rc;
					GetClientRect(hwnd, &rc);
					WritePrivateProfileInt("Video", "Window width", (rc.right - rc.left), IniName);
					WritePrivateProfileInt("Video", "Window height", (rc.bottom - rc.top), IniName);
				}

				//Save window position
				WritePrivateProfileInt("Video", "WindowPosX", WndX/*MainWindowRect.left*/, IniName);
				WritePrivateProfileInt("Video", "WindowPosY", WndY/*MainWindowRect.top*/, IniName);

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
				int minX = video.rotatedwidthgap();
				int minY = video.rotatedheightgap();
				if(verticalDrag && !sideways && SeparationBorderDrag)
				{
					forceRatioFlags |= WINCLASS::KEEPX;
					minY = (MainScreenRect.bottom - MainScreenRect.top) + (SubScreenRect.bottom - SubScreenRect.top);
					setGap = true;
				}
				else if(horizontalDrag && sideways && SeparationBorderDrag)
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


			MainWindow->sizingMsg(wParam, lParam, forceRatioFlags);


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
	case WM_SYSKEYDOWN:
	case WM_CUSTKEYDOWN:
		{
			int modifiers = GetModifiers(wParam);
			if(!HandleKeyMessage(wParam,lParam, modifiers))
				return 0;
			break;
		}
	case WM_KEYUP:
		input_acquire();
		if(wParam != VK_PAUSE)
			break;
	case WM_SYSKEYUP:
	case WM_CUSTKEYUP:
		{
			int modifiers = GetModifiers(wParam);
			HandleKeyUp(wParam, lParam, modifiers);
		}
		break;

	case WM_SIZE:
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
				if(CommonSettings.single_core)
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
			if (!(fileDropped.find(".lua") == string::npos) && (fileDropped.find(".lua") == fileDropped.length()-4))	 //ROM is already loaded and .dsm in filename
			{
				if(LuaScriptHWnds.size() < 16)
				{
					char temp [1024];
					strcpy(temp, fileDropped.c_str());
					HWND IsScriptFileOpen(const char* Path);
					if(!IsScriptFileOpen(temp))
					{
						HWND hDlg = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
						SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_SETTEXT,0,(LPARAM)temp);
					}
				}
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
				ToDSScreenRelativeCoords(x,y,false);
				EditHud(x,y, &Hud);
			}
			else
			{
				ToDSScreenRelativeCoords(x,y,true);
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
			if(wParam >= baseid && wParam <= baseid + MAX_RECENT_ROMS - 1)
			{
				int x = wParam - baseid;
				OpenRecentROM(x);					
			}
			else if(wParam == clearid)
			{
				/* Clear all the recent ROMs */
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
						HWND hDlg = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
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
				WavRecordTo();
			break;
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
				//CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_GAME_INFO), hwnd, GinfoView_Proc);
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
			if (!RegWndClass("DeSmuME_IORegView", IORegView_Proc, CS_DBLCLKS, LoadIcon(hAppInst, "ICONDESMUME"), sizeof(CIORegView*)))
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
			Pause();
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
		case IDM_RECORD_MOVIE:
			MovieRecordTo();
			return 0;
		case IDM_PLAY_MOVIE:
			Replay_LoadMovie();
			return 0;
		case IDM_STOPMOVIE:
			FCEUI_StopMovie();
			return 0;
		case ID_VIEW_FRAMECOUNTER:
			CommonSettings.hud.FrameCounterDisplay ^= true;
			WritePrivateProfileBool("Display", "Display Fps", CommonSettings.hud.FpsDisplay, IniName);
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

				DialogBox(hAppInst, MAKEINTRESOURCE(IDD_3DSETTINGS), hwnd, (DLGPROC)GFX3DSettingsDlgProc);

				if(tpaused) NDS_UnPause();
			}
			return 0;
		case IDC_FRAMESKIPAUTO:
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
				if(LOWORD(wParam) == IDC_FRAMESKIPAUTO)
				{
					autoframeskipenab = 1;
					WritePrivateProfileString("Video", "FrameSkip", "AUTO", IniName);
				}
				else
				{
					char text[80];
					autoframeskipenab = 0;
					frameskiprate = LOWORD(wParam) - IDC_FRAMESKIP0;
					sprintf(text, "%d", frameskiprate);
					WritePrivateProfileString("Video", "FrameSkip", text, IniName);
				}
			}
			return 0;
		case IDC_NEW_LUA_SCRIPT:
			if(LuaScriptHWnds.size() < 16)
				CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
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
		
		case IDC_LANGDANISH:
			SaveLanguage(2);
			ChangeLanguage(2);
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
			#ifdef WX_STUB
				wxTest();
				return 0;
			#endif
				bool tpaused=false;
				if (execute) 
				{
					tpaused=true;
					NDS_Pause();
				}
				DialogBox(hAppInst,MAKEINTRESOURCE(IDD_ABOUT_BOX), hwnd, (DLGPROC) AboutBox_Proc);
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
			windowSize=65535;
			ScaleScreen(windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW2_5X:
			windowSize=65534;
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
				RECT rc;
				GetClientRect(hwnd, &rc);
				WritePrivateProfileInt("Video","Window Force Ratio",1,IniName);
				WritePrivateProfileInt("Video", "Window width", (rc.right - rc.left), IniName);
				WritePrivateProfileInt("Video", "Window height", (rc.bottom - rc.top), IniName);
			}
			break;


		case IDM_DEFSIZE:
			{
				if(windowSize != 1)
					windowSize = 0;

				ScaleScreen(1);
			}
			break;
		case IDM_ALWAYS_ON_TOP:
			{
				LONG exStyle = GetWindowLong(MainWindow->getHWnd(), GWL_EXSTYLE);
				UINT menuCheck = MF_BYCOMMAND;
				HWND insertAfter = HWND_TOPMOST;

	
				if(exStyle & WS_EX_TOPMOST)
				{
					menuCheck |= MF_UNCHECKED;
					insertAfter = HWND_NOTOPMOST;
				}
				else
					menuCheck |= MF_CHECKED;
				
				CheckMenuItem(mainMenu, IDM_ALWAYS_ON_TOP, menuCheck);
				SetWindowPos(MainWindow->getHWnd(), insertAfter, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
			}
			return 0;

		}
	}
  return DefWindowProc (hwnd, message, wParam, lParam);
}

LRESULT CALLBACK GFX3DSettingsDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			int i;

			CheckDlgButton(hw,IDC_INTERPOLATECOLOR,CommonSettings.HighResolutionInterpolateColor?1:0);
			CheckDlgButton(hw,IDC_ALTERNATEFLUSH,CommonSettings.gfx3d_flushMode);

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
					CommonSettings.HighResolutionInterpolateColor = IsDlgCheckboxChecked(hw,IDC_INTERPOLATECOLOR);
					NDS_3D_ChangeCore(ComboBox_GetCurSel(GetDlgItem(hw, IDC_3DCORE)));
					WritePrivateProfileInt("3D", "Renderer", cur3DCore, IniName);
					WritePrivateProfileInt("3D", "HighResolutionInterpolateColor", CommonSettings.HighResolutionInterpolateColor?1:0, IniName);
					CommonSettings.gfx3d_flushMode = (IsDlgButtonChecked(hw,IDC_ALTERNATEFLUSH) == BST_CHECKED)?1:0;
					WritePrivateProfileInt("3D", "AlternateFlush", CommonSettings.gfx3d_flushMode, IniName);
				}
			case IDCANCEL:
				{
					EndDialog(hw, TRUE);
				}
				return TRUE;

			case IDC_DEFAULT:
				{
					NDS_3D_ChangeCore(GPU3D_OPENGL);
					ComboBox_SetCurSel(GetDlgItem(hw, IDC_3DCORE), GPU3D_OPENGL);
					WritePrivateProfileInt("3D", "Renderer", GPU3D_OPENGL, IniName);
					CommonSettings.gfx3d_flushMode = 0;
					WritePrivateProfileInt("3D", "AlternateFlush", CommonSettings.gfx3d_flushMode, IniName);
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

			CheckDlgButton(hDlg, IDC_CHECKBOX_DEBUGGERMODE, ((CommonSettings.DebugConsole == true) ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hDlg, IDC_USEEXTBIOS, ((CommonSettings.UseExtBIOS == true) ? BST_CHECKED : BST_UNCHECKED));
			SetDlgItemText(hDlg, IDC_ARM9BIOS, CommonSettings.ARM9BIOS);
			SetDlgItemText(hDlg, IDC_ARM7BIOS, CommonSettings.ARM7BIOS);
			CheckDlgButton(hDlg, IDC_BIOSSWIS, ((CommonSettings.SWIFromBIOS == true) ? BST_CHECKED : BST_UNCHECKED));

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

					WritePrivateProfileInt("Emulation", "DebugConsole", ((CommonSettings.DebugConsole == true) ? 1 : 0), IniName);
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

			UseMicSample = GetPrivateProfileBool("Use Mic Sample", "UseMicSample", false, IniName);
			CheckDlgButton(hDlg, IDC_USEMICSAMPLE, ((UseMicSample == true) ? BST_CHECKED : BST_UNCHECKED));
			GetPrivateProfileString("Use Mic Sample", "MicSampleFile", "micsample.raw", MicSampleName, MAX_PATH, IniName);
			SetDlgItemText(hDlg, IDC_MICSAMPLE, MicSampleName);

			if(UseMicSample == false)
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

					if((romloaded)) //|| (val == IDYES))
					{
						HWND cur;

						UseMicSample = IsDlgCheckboxChecked(hDlg, IDC_USEMICSAMPLE);
						cur = GetDlgItem(hDlg, IDC_MICSAMPLE);
						GetWindowText(cur, MicSampleName, 256);
		
						WritePrivateProfileInt("Use Mic Sample", "UseMicSample", ((UseMicSample == true) ? 1 : 0), IniName);
						WritePrivateProfileString("Use Mic Sample", "MicSampleFile", MicSampleName, IniName);

						if (UseMicSample)
						{
							if (!LoadSample(MicSampleName))
							{
								MessageBox(hDlg, "Unable to read the sample", "DeSmuME", (MB_OK | MB_ICONEXCLAMATION));
							}
						}
					}
				}
			case IDCANCEL:
				{
					EndDialog(hDlg, TRUE);
				}
				return TRUE;

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
	#ifdef EXPERIMENTAL_WIFI
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

LRESULT CALLBACK SoundSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

			//setup interpolation combobox
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"None (fastest, sounds bad)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"Linear (typical, sounds good)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"Cosine (slowest, sounds best)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_SETCURSEL, (int)CommonSettings.spuInterpolationMode, 0);

			//setup cache setting
			CheckDlgButton(hDlg, IDC_SPU_CACHE,  CommonSettings.spuAdpcmCache?BST_CHECKED:BST_UNCHECKED   );

			// Setup Sound Buffer Size Edit Text
			sprintf(tempstr, "%d", sndbuffersize);
			SetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr);

			// Setup Volume Slider
			SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_SETRANGE, 0, MAKELONG(0, 100));

			// Set Selected Volume
			SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_SETPOS, TRUE, sndvolume);

			timerid = SetTimer(hDlg, 1, 500, NULL);
			return TRUE;
		}
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
					WritePrivateProfileString("Sound", "SoundBufferSize", tempstr, IniName);

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

					//write interpolation type
					CommonSettings.spuInterpolationMode = (SPUInterpolationMode)SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_GETCURSEL, 0, 0);
					WritePrivateProfileInt("Sound","SPUInterpolation",(int)CommonSettings.spuInterpolationMode, IniName);

					//write cache setting
					CommonSettings.spuAdpcmCache = IsDlgButtonChecked(hDlg, IDC_SPU_CACHE) != 0;
					WritePrivateProfileInt("Sound","SPUAdpcmCache",CommonSettings.spuAdpcmCache?1:0, IniName);

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
	NDS_Reset();
}

//adelikat: This function changes a menu item's text
void ChangeMenuItemText(int menuitem, string text)
{
	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_TYPE;
	moo.cch = NULL;
	GetMenuItemInfo(mainMenu, menuitem, FALSE, &moo);
	moo.dwTypeData = (LPSTR)text.c_str();
	SetMenuItemInfo(mainMenu, menuitem, FALSE, &moo);
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

//adelikat:  This function find the current hotkey assignments for corresponding menu items
//			 and appends it to the menu item.  This function works correctly for all language menus
//		     However, a Menu item in the Resource file must NOT have a /t (tab) in its caption.
void UpdateHotkeyAssignments()
{
	extern void TranslateKey(WORD keyz,char *out);	//adelikat: Yeah hackey
	//Update all menu items that can be called from a hotkey to include the current hotkey assignment
	char str[255];		//Temp string 
	string text;		//Used to manipulate menu item text
	string keyname;		//Used to hold the name of the hotkey
	int truncate;		//Used to truncate the hotkey config from the menu item
	//-------------------------------FILE---------------------------------------
	
	
	//Open ROM
	GetMenuString(mainMenu,IDM_OPEN, str, 255, IDM_OPEN);	//Get menu item text
	text = str;												//Store in string object
	truncate = text.find("\t");								//Find the tab
	if (truncate >= 1)										//Truncate if it exists
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.OpenROM.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.OpenROM.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(IDM_OPEN, text);

	//Save Screenshot As...
	GetMenuString(mainMenu,IDM_PRINTSCREEN, str, 255, IDM_PRINTSCREEN);	
	text = str;
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.PrintScreen.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.PrintScreen.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(IDM_PRINTSCREEN, text);

	//adelikat: Why don't these work?  GetMenuString returns null for these IDs yet those are the valid ID numbers
	/*
	//Record AVI
	GetMenuString(mainMenu,IDM_FILE_RECORDAVI, str, 255, IDM_FILE_RECORDAVI);	
	text = str;
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.RecordAVI.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.RecordAVI.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(IDM_FILE_RECORDAVI, text);

	//Stop AVI
	GetMenuString(mainMenu,IDM_FILE_STOPAVI, str, 255, IDM_FILE_STOPAVI);	
	text = str;
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.StopAVI.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.StopAVI.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(IDM_FILE_STOPAVI, text);
*/
	
	//-------------------------------EMULATION----------------------------------

	//Pause
	GetMenuString(mainMenu,IDM_PAUSE, str, 255, IDM_PAUSE);	
	text = str;								
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.Pause.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.Pause.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(IDM_PAUSE, text);

	//Reset
	GetMenuString(mainMenu,IDM_RESET, str, 255, IDM_RESET);
	text = str;	
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.Reset.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.Reset.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(IDM_RESET, text);

	//-------------------------------EMULATION----------------------------------
/*
	//Display Frame Counter
	GetMenuString(mainMenu,ID_VIEW_FRAMECOUNTER, str, 255, ID_VIEW_FRAMECOUNTER);
	text = str;	
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.ToggleFrameCounter.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.ToggleFrameCounter.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(ID_VIEW_FRAMECOUNTER, text);

	//Display FPS
	GetMenuString(mainMenu,ID_VIEW_DISPLAYFPS, str, 255, ID_VIEW_DISPLAYFPS);
	text = str;	
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.ToggleFPS.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.ToggleFPS.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(ID_VIEW_DISPLAYFPS, text);

	//Display Input
	GetMenuString(mainMenu,ID_VIEW_DISPLAYINPUT, str, 255, ID_VIEW_DISPLAYINPUT);
	text = str;	
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.ToggleInput.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.ToggleInput.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(ID_VIEW_DISPLAYINPUT, text);

	//Display Lag Counter
	GetMenuString(mainMenu,ID_VIEW_DISPLAYLAG, str, 255, ID_VIEW_DISPLAYLAG);
	text = str;	
	truncate = text.find("\t");
	if (truncate >= 1)
		text = text.substr(0,truncate);
	TranslateKey(CustomKeys.ToggleLag.key, str);
	keyname = str;
	keyname.insert(0,GetModifierString(CustomKeys.ToggleLag.modifiers));
	text.append("\t" + keyname);
	ChangeMenuItemText(ID_VIEW_DISPLAYLAG, text);
*/
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

			HWND hDlg = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
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
