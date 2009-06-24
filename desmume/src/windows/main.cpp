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
#include "version.h"
#include "CWindow.h"
#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../debug.h"
#include "../saves.h"
#ifndef EXPERIMENTAL_GBASLOT
#include "../cflash.h"
#else
#include "../addons.h"
#endif
#include "resource.h"
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

#include "directx/ddraw.h"

using namespace std;

#define HAVE_REMOTE
#define WPCAP
#define PACKET_SIZE 65535

#include <pcap.h>
#include <remote-ext.h> //uh?

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

unsigned int lastSaveState = 0;		//Keeps track of last savestate used for quick save/load functions
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


CRITICAL_SECTION win_sync;
volatile int win_sound_samplecounter = 0;

Lock::Lock() { EnterCriticalSection(&win_sync); }
Lock::~Lock() { LeaveCriticalSection(&win_sync); }

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

#ifndef EXPERIMENTAL_GBASLOT
/* The compact flash disk image file */
static const char *bad_glob_cflash_disk_image_file;
static char cflash_filename_buffer[512];
#endif

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

/*  Make the class name into a global variable  */
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

int ScreenGap = 0;
extern HWND RamSearchHWnd;
static BOOL lostFocusPause = TRUE;
static BOOL lastPauseFromLostFocus = FALSE;
static int FrameLimit = 1;

//=========================== view tools
TOOLSCLASS	*ViewDisasm_ARM7 = NULL;
TOOLSCLASS	*ViewDisasm_ARM9 = NULL;
//TOOLSCLASS	*ViewMem_ARM7 = NULL;
//TOOLSCLASS	*ViewMem_ARM9 = NULL;
TOOLSCLASS	*ViewRegisters = NULL;
TOOLSCLASS	*ViewPalette = NULL;
TOOLSCLASS	*ViewTiles = NULL;
TOOLSCLASS	*ViewMaps = NULL;
TOOLSCLASS	*ViewOAM = NULL;
TOOLSCLASS	*ViewMatrices = NULL;
TOOLSCLASS	*ViewLights = NULL;

volatile BOOL execute = FALSE;
volatile BOOL paused = TRUE;
volatile BOOL pausedByMinimize = FALSE;
u32 glock = 0;

BOOL click = FALSE;

BOOL finished = FALSE;
BOOL romloaded = FALSE;

void SetScreenGap(int gap);

void SetRotate(HWND hwnd, int rot);

BOOL ForceRatio = TRUE;
float DefaultWidth;
float DefaultHeight;
float widthTradeOff;
float heightTradeOff;

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

int autoframeskipenab=0;
int frameskiprate=0;
int emu_paused = 0;
bool frameAdvance = false;
bool frameCounterDisplay = false;
bool FpsDisplay = false;
bool ShowLagFrameCounter = false;
bool ShowMicrophone = false;
bool HudEditorMode = false;
bool UseMicSample = false;
unsigned short windowSize = 0;

struct HudCoordinates{
	int x;
	int y;
	int xsize;
	int ysize;
	int storedx;
	int storedy;
	int clicked;
};

struct HudStruct {

	HudCoordinates FpsDisplay;
	HudCoordinates FrameCounter;
	HudCoordinates InputDisplay;
	HudCoordinates LagFrameCounter;
	HudCoordinates Microphone;
	HudCoordinates Dummy;

	HudCoordinates &hud(int i) { return ((HudCoordinates*)this)[i]; }
};

void SetHudDummy (HudCoordinates *hud)
{
	hud->x=666;
	hud->y=666;
}

bool IsHudDummy (HudCoordinates *hud)
{
	return (hud->x == 666 && hud->y == 666);
}

HudStruct Hud;

void EditHud(s32 x, s32 y, HudStruct *hudstruct) {

	UINT i = 0;

	while (!IsHudDummy(&hudstruct->hud(i))) {
		HudCoordinates &hud = hudstruct->hud(i);

		//reset
		if(!hud.clicked) {
			hud.storedx=0;
			hud.storedy=0;
		}

		if((x >= hud.x && x <= hud.x + hud.xsize) && 
			(y >= hud.y && y <= hud.y + hud.ysize) && !hud.clicked ) {

				hud.clicked=1;
				hud.storedx = x - hud.x;
				hud.storedy = y - hud.y;
		}

		if(hud.clicked) {
			hud.x = x - hud.storedx;
			hud.y = y - hud.storedy;
		}

		//sanity checks
		if(hud.x < 0)  hud.x = 0;
		if(hud.y < 0)  hud.y = 0;
		if(hud.x > 245)hud.x = 245; //margins
		if(hud.y > 180)hud.y = 180;

		if(hud.clicked)
			break;//prevent items from grouping together

		i++;
	}
}

void HudClickRelease(HudStruct *hudstruct) {

	UINT i = 0;

	while (!IsHudDummy(&hudstruct->hud(i))) {
		HudCoordinates &hud = hudstruct->hud(i);
		hud.clicked=0;
		i++;
	}
}

void ResetHud(HudStruct *hudstruct) {

	hudstruct->FpsDisplay.x=0;
	hudstruct->FpsDisplay.y=5;
	hudstruct->FpsDisplay.xsize=120;
	hudstruct->FpsDisplay.ysize=10;

	hudstruct->FrameCounter.x=0;
	hudstruct->FrameCounter.y=25;
	hudstruct->FrameCounter.xsize=60;
	hudstruct->FrameCounter.ysize=10;

	hudstruct->InputDisplay.x=0;
	hudstruct->InputDisplay.y=45;
	hudstruct->InputDisplay.xsize=120;
	hudstruct->InputDisplay.ysize=10;

	hudstruct->LagFrameCounter.x=0;
	hudstruct->LagFrameCounter.y=65;
	hudstruct->LagFrameCounter.xsize=30;
	hudstruct->LagFrameCounter.ysize=10;
	
	hudstruct->Microphone.x=0;
	hudstruct->Microphone.y=85;
	hudstruct->Microphone.xsize=20;
	hudstruct->Microphone.ysize=10;

	SetHudDummy(&hudstruct->Dummy);
}

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

int KeyInDelayInCount=10;

static int lastTime = timeGetTime();
int repeattime;

VOID CALLBACK KeyInputTimer( UINT idEvent, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	//
//	if(timeGetTime() - lastTime > 5)
//	{
		bool S9xGetState (WORD KeyIdent);

		/*		if(GUI.JoystickHotkeys)
		{
		static uint32 joyState [256];

		for(int i = 0 ; i < 255 ; i++)
		{
		bool active = !S9xGetState(0x8000|i);

		if(active)
		{
		if(joyState[i] < ULONG_MAX) // 0xffffffffUL
		joyState[i]++;
		if(joyState[i] == 1 || joyState[i] >= (unsigned) KeyInDelayInCount)
		PostMessage(GUI.hWnd, WM_CUSTKEYDOWN, (WPARAM)(0x8000|i),(LPARAM)(NULL));
		}
		else
		{
		if(joyState[i])
		{
		joyState[i] = 0;
		PostMessage(GUI.hWnd, WM_CUSTKEYUP, (WPARAM)(0x8000|i),(LPARAM)(NULL));
		}
		}
		}
		}*/
		//		if((!GUI.InactivePause || !Settings.ForcedPause)
		//				|| (GUI.BackgroundInput || !(Settings.ForcedPause & (PAUSE_INACTIVE_WINDOW | PAUSE_WINDOW_ICONISED))))
		//		{
		static uint32 joyState [256];
		for(int i = 0 ; i < 255 ; i++)
		{
			bool active = !S9xGetState(i);
			if(active)
			{
				if(joyState[i] < ULONG_MAX) // 0xffffffffUL
					joyState[i]++;
				if(joyState[i] == 1 || joyState[i] >= (unsigned) KeyInDelayInCount) {
					//sort of fix the auto repeating
					//TODO find something better
				//	repeattime++;
				//	if(repeattime % 10 == 0) {

						PostMessage(MainWindow->getHWnd(), WM_CUSTKEYDOWN, (WPARAM)(i),(LPARAM)(NULL));
						repeattime=0;
				//	}
				}
			}
			else
			{
				if(joyState[i])
				{
					joyState[i] = 0;
					PostMessage(MainWindow->getHWnd(), WM_CUSTKEYUP, (WPARAM)(i),(LPARAM)(NULL));
				}
			}
		}
		//	}
	//	lastTime = timeGetTime();
//	}
}

//static void
//init_configured_features( struct configured_features *config) {
//	config->arm9_gdb_port = 0;
//	config->arm7_gdb_port = 0;
//
//	config->cflash_disk_image_file = NULL;
//}
//
//
//static int
//fill_configured_features( struct configured_features *config, LPSTR lpszArgument) {
//	int good_args = 0;
//	LPTSTR cmd_line;
//	LPWSTR *argv;
//	int argc;
//
//	argv = CommandLineToArgvW( GetCommandLineW(), &argc);
//
//	if ( argv != NULL) {
//		int i;
//		good_args = 1;
//		for ( i = 1; i < argc && good_args; i++) {
//			if ( wcsncmp( argv[i], L"--arm9gdb=", 10) == 0) {
//				wchar_t *end_char;
//				unsigned long port_num = wcstoul( &argv[i][10], &end_char, 10);
//
//				if ( port_num > 0 && port_num < 65536) {
//					config->arm9_gdb_port = port_num;
//				}
//				else {
//					MessageBox(NULL,"ARM9 GDB stub port must be in the range 1 to 65535","Error",MB_OK);
//					good_args = 0;
//				}
//			}
//			else if ( wcsncmp( argv[i], L"--arm7gdb=", 10) == 0) {
//				wchar_t *end_char;
//				unsigned long port_num = wcstoul( &argv[i][10], &end_char, 10);
//
//				if ( port_num > 0 && port_num < 65536) {
//					config->arm7_gdb_port = port_num;
//				}
//				else {
//					MessageBox(NULL,"ARM9 GDB stub port must be in the range 1 to 65535","Error",MB_OK);
//					good_args = 0;
//				}
//			}
//
//#ifdef EXPERIMENTAL_GBASLOT
//			else if ( wcsncmp( argv[i], L"--cflash=", 9) == 0) 
//			{
//				char buf[512];
//				size_t convert_count = wcstombs(&buf[0], &argv[i][9], 512);
//				if (convert_count > 0)
//				{
//					addon_type = NDS_ADDON_CFLASH;
//					CFlashUsePath = FALSE;
//					strcpy(CFlashName, buf);
//				}
//			}
//			else if ( wcsncmp( argv[i], L"--gbagame=", 10) == 0) 
//			{
//				char buf[512];
//				size_t convert_count = wcstombs(&buf[0], &argv[i][9], 512);
//				if (convert_count > 0)
//				{
//					addon_type = NDS_ADDON_GBAGAME;
//					strcpy(GBAgameName, buf);
//				}
//			}
//			else if ( wcsncmp( argv[i], L"--rumble", 8) == 0) 
//			{
//				addon_type = NDS_ADDON_RUMBLEPAK;
//			}
//#else
//			else if ( wcsncmp( argv[i], L"--cflash=", 9) == 0) {
//				if ( config->cflash_disk_image_file == NULL) {
//					size_t convert_count = wcstombs( &cflash_filename_buffer[0], &argv[i][9], 512);
//					if ( convert_count > 0) {
//						config->cflash_disk_image_file = cflash_filename_buffer;
//					}
//				}
//				else {
//					MessageBox(NULL,"CFlash disk image file already set","Error",MB_OK);
//					good_args = 0;
//				}
//			}
//#endif
//		}
//		LocalFree( argv);
//	}
//
//	return good_args;
//}

// Rotation definitions
short GPU_rotation      = 0;
DWORD GPU_width         = 256;
DWORD GPU_height        = 192*2;
DWORD rotationstartscan = 192;
DWORD rotationscanlines = 192*2;

void ScaleScreen(float factor)
{
	if(factor==65535)
		factor = 1.5f;
	else if(factor==65534)
		factor = 2.5f;
	if((GPU_rotation == 90) || (GPU_rotation == 270))
	{
		MainWindow->setClientSize(((384 + ScreenGap) * factor), (256 * factor));
	}
	else
	{
		MainWindow->setClientSize((256 * factor), ((384 + ScreenGap) * factor));
	}
}

void translateXY(s32& x, s32& y)
{
	s32 tx=x, ty=y;
	switch(GPU_rotation)
	{
	case 90:
		x = ty;
		y = 191-tx;
		break;
	case 180:
		x = 255-tx;
		y = 383-ty;
		y -= 192;
		break;
	case 270:
		x = 255-ty;
		y = (tx-192-ScreenGap);
		break;
	}
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
	if (RecentRoms.size() == MAX_RECENT_ROMS)	
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
		if (x < RecentRoms.size())	//If it exists in the array, save it
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

	if ( (GPU_rotation == 0) || (GPU_rotation == 180) )
	{
		ddsd.dwWidth         = 256;
		ddsd.dwHeight        = 384;
	}
	else
		if ( (GPU_rotation == 90) || (GPU_rotation == 270) )
		{
			ddsd.dwWidth         = 384;
			ddsd.dwHeight        = 256;
		}

		if (IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpBackSurface, NULL) != DD_OK) return -2;

		if (IDirectDraw7_CreateClipper(lpDDraw, 0, &lpDDClipPrimary, NULL) != DD_OK) return -3;

		if (IDirectDrawClipper_SetHWnd(lpDDClipPrimary, 0, MainWindow->getHWnd()) != DD_OK) return -4;
		if (IDirectDrawSurface7_SetClipper(lpPrimarySurface, lpDDClipPrimary) != DD_OK) return -5;

		return 1;
}

void Display()
{
	int res;
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags=DDSD_ALL;
	res = lpBackSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);

	if (res == DD_OK)
	{
		char* buffer = (char*)ddsd.lpSurface;

		int i, j, sz=256*sizeof(u32);
		switch(ddsd.ddpfPixelFormat.dwRGBBitCount)
		{
		case 24:
		case 32:
			{
				switch(GPU_rotation)
				{
				case 0:
				case 180:
					{
						if(ddsd.lPitch == 1024)
						{
							for(int i = 0; i < 98304; i++)
								((u32*)buffer)[i] = RGB15TO24_REVERSE(((u16*)GPU_screen)[i]);
						}
						else
						{
							for(int y = 0; y < 384; y++)
							{
								for(int x = 0; x < 256; x++)
									((u32*)buffer)[x] = RGB15TO24_REVERSE(((u16*)GPU_screen)[(y * 384) + x]);

								buffer += ddsd.lPitch;
							}
						}
					}
					break;
				case 90:
				case 270:
					{
						if(ddsd.lPitch == 1536)
						{
							for(int i = 0; i < 98304; i++)
								((u32*)buffer)[i] = RGB15TO24_REVERSE(((u16*)GPU_screen)[i]);
						}
						else
						{
							for(int y = 0; y < 256; y++)
							{
								for(int x = 0; x < 384; x++)
									((u32*)buffer)[x] = RGB15TO24_REVERSE(((u16*)GPU_screen)[(y * 256) + x]);

								buffer += ddsd.lPitch;
							}
						}
					}
					break;
				}
			}
			break;

		case 16:
			{
				switch(GPU_rotation)
				{
				case 0:
				case 180:
					{
						if(ddsd.lPitch == 512)
						{
							for(int i = 0; i < 98304; i++)
								((u16*)buffer)[i] = RGB15TO16_REVERSE(((u16*)GPU_screen)[i]);
						}
						else
						{
							for(int y = 0; y < 384; y++)
							{
								for(int x = 0; x < 256; x++)
									((u16*)buffer)[x] = RGB15TO16_REVERSE(((u16*)GPU_screen)[(y * 384) + x]);

								buffer += ddsd.lPitch;
							}
						}
					}
					break;
				case 90:
				case 270:
					{
						if(ddsd.lPitch == 768)
						{
							for(int i = 0; i < 98304; i++)
								((u16*)buffer)[i] = RGB15TO16_REVERSE(((u16*)GPU_screen)[i]);
						}
						else
						{
							for(int y = 0; y < 256; y++)
							{
								for(int x = 0; x < 384; x++)
									((u16*)buffer)[x] = RGB15TO16_REVERSE(((u16*)GPU_screen)[(y * 256) + x]);

								buffer += ddsd.lPitch;
							}
						}
					}
					break;
				}
			}
			break;

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
		if(ScreenGap > 0)
		{
			//u32 color = gapColors[win_fw_config.fav_colour];
			//u32 color_rev = (((color & 0xFF) << 16) | (color & 0xFF00) | ((color & 0xFF0000) >> 16));
			u32 color_rev = 0xFFFFFF;

			HDC dc;
			HBRUSH brush;

			dc = GetDC(MainWindow->getHWnd());
			brush = CreateSolidBrush(color_rev);

			FillRect(dc, &GapRect, brush);

			DeleteObject((HGDIOBJ)brush);
			ReleaseDC(MainWindow->getHWnd(), dc);
		}
	}
	else
	{
		if (res==DDERR_SURFACELOST)
		{
			LOG("DirectDraw buffers is lost\n");
			if (IDirectDrawSurface7_Restore(lpPrimarySurface)==DD_OK)
				IDirectDrawSurface7_Restore(lpBackSurface);
		}
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

DWORD WINAPI run()
{
	char txt[80];
	u32 cycles = 0;
	int wait=0;
	u64 freq;
	u64 OneFrameTime;
	int framestoskip=0;
	int framesskipped=0;
	int skipnextframe=0;
	u64 lastticks=0;
	u64 curticks=0;
	u64 diffticks=0;
	u32 framecount=0;
	u64 onesecondticks=0;
	int fps=0;
	int fpsframecount=0;
	u64 fpsticks=0;
	int	res;
	HWND	hwnd = MainWindow->getHWnd();

	DDCAPS	hw_caps, sw_caps;

	InitSpeedThrottle();

	osd->setRotate(GPU_rotation);

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

	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	QueryPerformanceCounter((LARGE_INTEGER *)&lastticks);
	OneFrameTime = freq / 60;

	while(!finished)
	{
		while(execute)
		{
			input_process();
			FCEUMOV_AddInputState();
				
			if (ShowInputDisplay) osd->addFixed(Hud.InputDisplay.x, Hud.InputDisplay.y, "%s",InputDisplayString.c_str());

			{
				Lock lock;
				NDS_exec<false>();
				win_sound_samplecounter = 735;
			}
			DRV_AviVideoUpdate((u16*)GPU_screen);

			static int fps3d = 0;

			if (FpsDisplay) osd->addFixed(Hud.FpsDisplay.x, Hud.FpsDisplay.y, "Fps:%02d/%02d", fps, fps3d);
			if (frameCounterDisplay) 
			{
				if (movieMode == MOVIEMODE_PLAY)
					osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d/%d",currFrameCounter,currMovieData.records.size());
				else if(movieMode == MOVIEMODE_RECORD) 
					osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d",currFrameCounter);
				else
					osd->addFixed(Hud.FrameCounter.x, Hud.FrameCounter.y, "%d (no movie)",currFrameCounter);
			}

			if(!AVI_IsRecording()) osd->update();
			Display();
			osd->clear();

			gfx3d.frameCtrRaw++;
			if(gfx3d.frameCtrRaw == 60) {
				fps3d = gfx3d.frameCtr;
				gfx3d.frameCtrRaw = 0;
				gfx3d.frameCtr = 0;
			}


			// TODO: make that thing properly threaded
			static DWORD tools_time_last = 0;
			DWORD time_now = timeGetTime();
			if((time_now - tools_time_last) >= 50)
			{
				if(MemView_IsOpened(ARMCPU_ARM9)) MemView_Refresh(ARMCPU_ARM9);
				if(MemView_IsOpened(ARMCPU_ARM7)) MemView_Refresh(ARMCPU_ARM7);
			//	if(IORegView_IsOpened()) IORegView_Refresh();

				tools_time_last = time_now;
			}
			if(SoundView_IsOpened()) SoundView_Refresh();

			Update_RAM_Watch();
			Update_RAM_Search();

			fpsframecount++;
			QueryPerformanceCounter((LARGE_INTEGER *)&curticks);
			bool oneSecond = curticks >= fpsticks + freq;
			if(oneSecond) // TODO: print fps on screen in DDraw
			{
				fps = fpsframecount;
				fpsframecount = 0;
				QueryPerformanceCounter((LARGE_INTEGER *)&fpsticks);
			}

			if(nds.idleFrameCounter==0 || oneSecond) 
			{
				//calculate a 16 frame arm9 load average
				int load = 0;
				for(int i=0;i<16;i++)
					load = load/8 + nds.runCycleCollector[(i+nds.idleFrameCounter)&15]*7/8;
				load = std::min(100,std::max(0,(int)(load*100/1120380)));
				sprintf(txt,"(%02d%%) %s", load, DESMUME_NAME_AND_VERSION);
				SetWindowText(hwnd, txt);
			}

			if(!skipnextframe)
			{

				framesskipped = 0;

				if (framestoskip > 0)
					skipnextframe = 1;
			}
			else
			{
				framestoskip--;

				if (framestoskip < 1)
					skipnextframe = 0;
				else
					skipnextframe = 1;

				framesskipped++;

				NDS_SkipNextFrame();
			}

			if(FastForward) {

				if(framesskipped < 9)
				{
					skipnextframe = 1;
					framestoskip = 1;
				}
				if (framestoskip < 1)
				framestoskip += 9;
			}

			else if(autoframeskipenab || FrameLimit)
				while(SpeedThrottle())
				{
				}

				if (autoframeskipenab)
				{
					framecount++;

					if (framecount > 60)
					{
						framecount = 1;
						onesecondticks = 0;
					}

					QueryPerformanceCounter((LARGE_INTEGER *)&curticks);
					diffticks = curticks-lastticks;

					if(ThrottleIsBehind() && (framesskipped < 9))
					{
						skipnextframe = 1;
						framestoskip = 1;
					}

					onesecondticks += diffticks;
					lastticks = curticks;
				}
				else
				{
					if (framestoskip < 1)
						framestoskip += frameskiprate;
				}
				if (frameAdvance)
				{
					frameAdvance = false;
					emu_halt();
					SPU_Pause(1);
				}
				if (ShowLagFrameCounter) osd->addFixed(Hud.LagFrameCounter.x, Hud.LagFrameCounter.y, "%d",TotalLagFrames);
				if (ShowMicrophone) osd->addFixed(Hud.Microphone.x, Hud.Microphone.y, "%d",MicDisplay);
//				DisplayMessage();
				CheckMessages();
	
		}

		paused = TRUE;
		CheckMessages();
		Sleep(100);
	}

	if (lpDDClipPrimary!=NULL) IDirectDraw7_Release(lpDDClipPrimary);
	if (lpPrimarySurface != NULL) IDirectDraw7_Release(lpPrimarySurface);
	if (lpBackSurface != NULL) IDirectDraw7_Release(lpBackSurface);
	if (lpDDraw != NULL) IDirectDraw7_Release(lpDDraw);
	return 1;
}

void NDS_Pause()
{
	if (!paused)
	{
		emu_halt();
		paused = TRUE;
		SPU_Pause(1);
		while (!paused) {}
		INFO("Emulation paused\n");
	}
}

void NDS_UnPause()
{
	if (romloaded && paused)
	{
		paused = FALSE;
		pausedByMinimize = FALSE;
		execute = TRUE;
		SPU_Pause(0);
		INFO("Emulation unpaused\n");
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
			sprintf(ntxt, "%d %s", i+1, savestates[i].date);
			UpdateSaveStateMenu(i, ntxt);
		}
	}
}



#ifdef EXPERIMENTAL_GBASLOT
static BOOL LoadROM(const char * filename)
#else
static BOOL LoadROM(const char * filename, const char *cflash_disk_image)
#endif
{
	ResetSaveStateTimes();
	NDS_Pause();
	//if (strcmp(filename,"")!=0) INFO("Attempting to load ROM: %s\n",filename);

#ifdef EXPERIMENTAL_GBASLOT
	if (NDS_LoadROM(filename) > 0)
#else
	if (NDS_LoadROM(filename, cflash_disk_image) > 0)
#endif
	{
		INFO("Loading %s was successful\n",filename);
		LoadSaveStateInfo();
		lagframecounter=0;
		UpdateRecentRoms(filename);
		osd->setRotate(GPU_rotation);
		if (AutoRWLoad)
		{
			//Open Ram Watch if its auto-load setting is checked
			OpenRWRecentFile(0);	
			RamWatchHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_RAMWATCH), MainWindow->getHWnd(), (DLGPROC) RamWatchProc);
		}
		SetRomName(filename);
		return TRUE;		
	}
	INFO("Loading %s FAILED.\n",filename);
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
		MainWindow->checkMenu(i, MF_BYCOMMAND | MF_UNCHECKED);

	MainWindow->checkMenu(id, MF_BYCOMMAND | MF_CHECKED);
}

void ChangeLanguage(int id)
{
	SetLanguage(id);

	MenuDeinit();
	MenuInit();
}

int RegClass(WNDPROC Proc, LPCTSTR szName)
{
	WNDCLASS	wc;

	wc.style = CS_DBLCLKS;
	wc.cbClsExtra = wc.cbWndExtra=0;
	wc.lpfnWndProc=Proc;
	wc.hInstance=hAppInst;
	wc.hIcon=LoadIcon(hAppInst,"ICONDESMUME");
	wc.hCursor=LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)(COLOR_BACKGROUND);
	wc.lpszMenuName=(LPCTSTR)NULL;
	wc.lpszClassName=szName;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	return RegisterClass(&wc);
}

static void ExitRunLoop()
{
	finished = TRUE;
	emu_halt();
}

class WinDriver : public BaseDriver
{
	virtual bool WIFI_Host_InitSystem() {
		#ifdef EXPERIMENTAL_WIFI
			//require wpcap.dll
			HMODULE temp = LoadLibrary("wpcap.dll");
			if(temp == NULL) {
				printf("Failed initializing wpcap.dll - softAP support disabled\n");
				return false;
			}
			FreeLibrary(temp);
			return true;
		#else
			return false ;
		#endif
	}
	virtual void WIFI_Host_ShutdownSystem() {
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
};


int _main()
{

#ifdef WX_STUB
	wxInitialize();
#endif

	driver = new WinDriver();

	InitializeCriticalSection(&win_sync);

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


	char text[80];

	GetINIPath();
#ifdef EXPERIMENTAL_GBASLOT
	addon_type = GetPrivateProfileInt("GBAslot", "type", NDS_ADDON_NONE, IniName);
	CFlashUsePath = GetPrivateProfileInt("GBAslot.CFlash", "usePath", 1, IniName);
	CFlashUseRomPath = GetPrivateProfileInt("GBAslot.CFlash", "useRomPath", 1, IniName);
	GetPrivateProfileString("GBAslot.CFlash", "path", "", CFlashPath, MAX_PATH, IniName);
	GetPrivateProfileString("GBAslot.CFlash", "filename", "", CFlashName, MAX_PATH, IniName);
	GetPrivateProfileString("GBAslot.GBAgame", "filename", "", GBAgameName, MAX_PATH, IniName);

	switch (addon_type)
	{
	case NDS_ADDON_NONE:
		break;
	case NDS_ADDON_CFLASH:
		if (!strlen(CFlashPath)) CFlashUseRomPath = TRUE;
		if (!strlen(CFlashName)) CFlashUsePath = TRUE;
		// TODO: check for file exist
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
	default:
		addon_type = NDS_ADDON_NONE;
		break;
	}
	addonsChangePak(addon_type);
#endif

	//init_configured_features( &my_config);
	/*if ( !fill_configured_features( &my_config, lpszArgument)) {
		MessageBox(NULL,"Unable to parse command line arguments","Error",MB_OK);
		return 0;
	}*/
	ColorCtrl_Register();

	if (!RegClass(WindowProcedure, "DeSmuME"))
	{
		MessageBox(NULL, "Error registering windows class", "DeSmuME", MB_OK);
		exit(-1);
	}

	windowSize = GetPrivateProfileInt("Video","Window Size", 0, IniName);
	GPU_rotation =  GetPrivateProfileInt("Video","Window Rotate", 0, IniName);
	ForceRatio = GetPrivateProfileInt("Video","Window Force Ratio", 1, IniName);
	FpsDisplay = GetPrivateProfileInt("Display","Display Fps", 0, IniName);
	WndX = GetPrivateProfileInt("Video","WindowPosX", CW_USEDEFAULT, IniName);
	WndY = GetPrivateProfileInt("Video","WindowPosY", CW_USEDEFAULT, IniName);
	frameCounterDisplay = GetPrivateProfileInt("Display","FrameCounter", 0, IniName);
	ShowInputDisplay = GetPrivateProfileInt("Display","Display Input", 0, IniName);
	ShowLagFrameCounter = GetPrivateProfileInt("Display","Display Lag Counter", 0, IniName);
	ShowMicrophone = GetPrivateProfileInt("Display","Display Microphone", 0, IniName);
	ScreenGap = GetPrivateProfileInt("Display", "ScreenGap", 0, IniName);
	FrameLimit = GetPrivateProfileInt("FrameLimit", "FrameLimit", 1, IniName);
	CommonSettings.showGpu.main = GetPrivateProfileInt("Display", "MainGpu", 1, IniName) != 0;
	CommonSettings.showGpu.sub = GetPrivateProfileInt("Display", "SubGpu", 1, IniName) != 0;
	lostFocusPause = GetPrivateProfileInt("Focus", "BackgroundPause", 0, IniName);

	
	//Get Ram-Watch values
	RWSaveWindowPos = GetPrivateProfileInt("RamWatch", "SaveWindowPos", 0, IniName);
	ramw_x = GetPrivateProfileInt("RamWatch", "RWWindowPosX", 0, IniName);
	ramw_y = GetPrivateProfileInt("RamWatch", "RWWindowPosY", 0, IniName);
	AutoRWLoad = GetPrivateProfileInt("RamWatch", "Auto-load", 0, IniName);
	for(int i = 0; i < MAX_RECENT_WATCHES; i++)
	{
		char str[256];
		sprintf(str, "Recent Watch %d", i+1);
		GetPrivateProfileString("Watches", str, "", &rw_recent_files[i][0], 1024, IniName);
	}

	//i think we should override the ini file with anything from the commandline
	CommandLine cmdline;
	cmdline.loadCommonOptions();
	if(!cmdline.parse(__argc,__argv)) {
		cmdline.errorHelp(__argv[0]);
		return 1;
	}

	//sprintf(text, "%s", DESMUME_NAME_AND_VERSION);
	MainWindow = new WINCLASS(CLASSNAME, hAppInst);
	DWORD dwStyle = WS_CAPTION| WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	if (!MainWindow->create(DESMUME_NAME_AND_VERSION, WndX/*CW_USEDEFAULT*/, WndY/*CW_USEDEFAULT*/, 256,384+ScreenGap,
		WS_CAPTION| WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
		NULL))
	{
		MessageBox(NULL, "Error creating main window", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	gpu_SetRotateScreen(GPU_rotation);

	/* default the firmware settings, they may get changed later */
	NDS_FillDefaultFirmwareConfigData( &win_fw_config);

	GetPrivateProfileString("General", "Language", "0", text, 80, IniName);
	SetLanguage(atoi(text));

#ifndef EXPERIMENTAL_GBASLOT
	bad_glob_cflash_disk_image_file = my_config.cflash_disk_image_file;
#endif

	//hAccel = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_MAIN_ACCEL)); //Now that we have a hotkey system we down need the Accel table.  Not deleting just yet though



	if(MenuInit() == 0)
	{
		MessageBox(NULL, "Error creating main menu", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	if((GPU_rotation == 90) || (GPU_rotation == 270))
	{
		MainWindow->setMinSize(384+ScreenGap, 256);
	}
	else
	{
		MainWindow->setMinSize(256, 384+ScreenGap);
	}

	{
		if(windowSize == 0)
		{
			int w = GetPrivateProfileInt("Video", "Window width", 256, IniName);
			int h = GetPrivateProfileInt("Video", "Window height", 384, IniName);
			MainWindow->setClientSize(w, h);
		}
		else
		{
			ScaleScreen(windowSize);
		}
	}

	DragAcceptFiles(MainWindow->getHWnd(), TRUE);

#ifndef EXPERIMENTAL_GBASLOT
	EnableMenuItem(mainMenu, IDM_GBASLOT, MF_GRAYED);
#endif

#ifdef EXPERIMENTAL_WIFI
	EnableMenuItem(mainMenu, IDM_WIFISETTINGS, MF_ENABLED);
#endif


	InitCustomKeys(&CustomKeys);
	ResetHud(&Hud);

	void input_init();
	input_init();

	LOG("Init NDS\n");

	GInfo_Init();

	MemView_Init();
	SoundView_Init();

	ViewDisasm_ARM7 = new TOOLSCLASS(hAppInst, IDD_DESASSEMBLEUR_VIEWER7, (DLGPROC) ViewDisasm_ARM7Proc);
	ViewDisasm_ARM9 = new TOOLSCLASS(hAppInst, IDD_DESASSEMBLEUR_VIEWER9, (DLGPROC) ViewDisasm_ARM9Proc);
	//ViewMem_ARM7 = new TOOLSCLASS(hAppInst, IDD_MEM_VIEWER7, (DLGPROC) ViewMem_ARM7Proc);
	//ViewMem_ARM9 = new TOOLSCLASS(hAppInst, IDD_MEM_VIEWER9, (DLGPROC) ViewMem_ARM9Proc);
	ViewRegisters = new TOOLSCLASS(hAppInst, IDD_IO_REG, (DLGPROC) IoregView_Proc);
	ViewPalette = new TOOLSCLASS(hAppInst, IDD_PAL, (DLGPROC) ViewPalProc);
	ViewTiles = new TOOLSCLASS(hAppInst, IDD_TILE, (DLGPROC) ViewTilesProc);
	ViewMaps = new TOOLSCLASS(hAppInst, IDD_MAP, (DLGPROC) ViewMapsProc);
	ViewOAM = new TOOLSCLASS(hAppInst, IDD_OAM, (DLGPROC) ViewOAMProc);
	ViewMatrices = new TOOLSCLASS(hAppInst, IDD_MATRIX_VIEWER, (DLGPROC) ViewMatricesProc);
	ViewLights = new TOOLSCLASS(hAppInst, IDD_LIGHT_VIEWER, (DLGPROC) ViewLightsProc);

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
	GetPrivateProfileString("General", "Language", "0", text, 80, IniName);	//================================================== ???
	CheckLanguage(IDC_LANGENGLISH+atoi(text));

	GetPrivateProfileString("Video", "FrameSkip", "0", text, 80, IniName);

	if (strcmp(text, "AUTO") == 0)
	{
		autoframeskipenab=1;
		frameskiprate=0;
		MainWindow->checkMenu(IDC_FRAMESKIPAUTO, MF_BYCOMMAND | MF_CHECKED);
	}
	else
	{
		autoframeskipenab=0;
		frameskiprate=atoi(text);
		MainWindow->checkMenu(frameskiprate + IDC_FRAMESKIP0, MF_BYCOMMAND | MF_CHECKED);
	}

	int KeyInRepeatMSec=20;

	hKeyInputTimer = timeSetEvent (KeyInRepeatMSec, 0, KeyInputTimer, 0, TIME_PERIODIC);

	cur3DCore = GetPrivateProfileInt("3D", "Renderer", GPU3D_OPENGL, IniName);
	CommonSettings.HighResolutionInterpolateColor = GetPrivateProfileInt("3D", "HighResolutionInterpolateColor", 1, IniName);
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

	EnterCriticalSection(&win_sync);
	int spu_ret = SPU_ChangeSoundCore(sndcoretype, sndbuffersize);
	LeaveCriticalSection(&win_sync);
	if(spu_ret != 0)
	{
		MessageBox(MainWindow->getHWnd(),"Unable to initialize DirectSound","Error",MB_OK);
		return -1;
	}

	sndvolume = GetPrivateProfileInt("Sound","Volume",100, IniName);
	SPU_SetVolume(sndvolume);

	CommonSettings.DebugConsole = GetPrivateProfileInt("Emulation", "DebugConsole", FALSE, IniName);
	CommonSettings.UseExtBIOS = GetPrivateProfileInt("BIOS", "UseExtBIOS", FALSE, IniName);
	GetPrivateProfileString("BIOS", "ARM9BIOSFile", "bios9.bin", CommonSettings.ARM9BIOS, 256, IniName);
	GetPrivateProfileString("BIOS", "ARM7BIOSFile", "bios7.bin", CommonSettings.ARM7BIOS, 256, IniName);
	CommonSettings.SWIFromBIOS = GetPrivateProfileInt("BIOS", "SWIFromBIOS", FALSE, IniName);

	CommonSettings.UseExtFirmware = GetPrivateProfileInt("Firmware", "UseExtFirmware", FALSE, IniName);
	GetPrivateProfileString("Firmware", "FirmwareFile", "firmware.bin", CommonSettings.Firmware, 256, IniName);
	CommonSettings.BootFromFirmware = GetPrivateProfileInt("Firmware", "BootFromFirmware", FALSE, IniName);

	CommonSettings.wifiBridgeAdapterNum = GetPrivateProfileInt("Wifi", "BridgeAdapter", 0, IniName);

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
#ifdef EXPERIMENTAL_GBASLOT
		if(LoadROM(cmdline.nds_file.c_str()))
#else
		if(LoadROM(cmdline.nds_file.c_str(), bad_glob_cflash_disk_image_file))
#endif
		{
			romloaded = TRUE;
			if(!cmdline.start_paused)
				NDS_UnPause();
		}
	}

	cmdline.process_movieCommands();
	
	if(cmdline.load_slot != 0)
	{
		HK_StateLoadSlot(cmdline.load_slot);
	}

	MainWindow->Show(SW_NORMAL);
	run();
	SaveRecentRoms();
	NDS_DeInit();
	DRV_AviEnd();
	WAV_End();

	//------SHUTDOWN

#ifdef DEBUG
	//LogStop();
#endif

	GInfo_DeInit();

	MemView_DlgClose(ARMCPU_ARM9);
	MemView_DlgClose(ARMCPU_ARM7);
	SoundView_DlgClose();
	//IORegView_DlgClose();

	MemView_DeInit();
	SoundView_DeInit();

	//if (input!=NULL) delete input;
	if (ViewLights!=NULL) delete ViewLights;
	if (ViewMatrices!=NULL) delete ViewMatrices;
	if (ViewOAM!=NULL) delete ViewOAM;
	if (ViewMaps!=NULL) delete ViewMaps;
	if (ViewTiles!=NULL) delete ViewTiles;
	if (ViewPalette!=NULL) delete ViewPalette;
	if (ViewRegisters!=NULL) delete ViewRegisters;
//	if (ViewMem_ARM9!=NULL) delete ViewMem_ARM9;
//	if (ViewMem_ARM7!=NULL) delete ViewMem_ARM7;
	if (ViewDisasm_ARM9!=NULL) delete ViewDisasm_ARM9;
	if (ViewDisasm_ARM7!=NULL) delete ViewDisasm_ARM7;

	delete MainWindow;

	return 0;
}

int WINAPI WinMain (HINSTANCE hThisInstance,
					HINSTANCE hPrevInstance,
					LPSTR lpszArgument,
					int nFunsterStil)

{
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
	int defHeight = (384 + ScreenGap);
	float ratio;
	int oneScreenHeight, gapHeight;

	GetClientRect(hwnd, &rc);

	if((GPU_rotation == 90) || (GPU_rotation == 270))
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
	oneScreenHeight = (192 * ratio);
	gapHeight = (wndHeight - (oneScreenHeight * 2));

	if((GPU_rotation == 90) || (GPU_rotation == 270))
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
		if(ScreenGap == 0) MainScreenRect.right += gapHeight;

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
		if(ScreenGap == 0) MainScreenRect.bottom += gapHeight;

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
	if((GPU_rotation == 90) || (GPU_rotation == 270))
	{
		// Main screen
		MainScreenSrcRect.left = 0;
		MainScreenSrcRect.top = 0;
		MainScreenSrcRect.right = 192;
		MainScreenSrcRect.bottom = 256;

		// Sub screen
		SubScreenSrcRect.left = 192;
		SubScreenSrcRect.top = 0;
		SubScreenSrcRect.right = 384;
		SubScreenSrcRect.bottom = 256;
	}
	else
	{
		// Main screen
		MainScreenSrcRect.left = 0;
		MainScreenSrcRect.top = 0;
		MainScreenSrcRect.right = 256;
		MainScreenSrcRect.bottom = 192;

		// Sub screen
		SubScreenSrcRect.left = 0;
		SubScreenSrcRect.top = 192;
		SubScreenSrcRect.right = 256;
		SubScreenSrcRect.bottom = 384;
	}
}

void SetScreenGap(int gap)
{
	RECT rc;
	int height, width;
	int oldgap, diff;

	GetClientRect(MainWindow->getHWnd(), &rc);
	width = (rc.right - rc.left);
	height = (rc.bottom - rc.top);

	oldgap = ScreenGap;
	ScreenGap = gap;

	if((GPU_rotation == 90) || (GPU_rotation == 270))
	{
		diff = ((gap - oldgap) * (height / 256.0f));
		MainWindow->setMinSize((384 + gap), 256);
		MainWindow->setClientSize(width+diff, height);
	}
	else
	{
		diff = ((gap - oldgap) * (width / 256.0f));
		MainWindow->setMinSize(256, (384 + gap));
		MainWindow->setClientSize(width, height+diff);
	}
}

//========================================================================================
void SetRotate(HWND hwnd, int rot)
{
	RECT rc;
	int oldrot = GPU_rotation;
	int oldheight, oldwidth;
	int newheight, newwidth;

	GPU_rotation = rot;

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

	switch (rot)
	{
	case 0:
		GPU_width    = 256;
		GPU_height   = 192*2;
		rotationstartscan = 192;
		rotationscanlines = 192*2;
		MainWindow->setMinSize(GPU_width, (GPU_height + ScreenGap));
		break;

	case 90:
		GPU_rotation = 90;
		GPU_width    = 192*2;
		GPU_height   = 256;
		rotationstartscan = 0;
		rotationscanlines = 256;
		MainWindow->setMinSize((GPU_width + ScreenGap), GPU_height);
		break;

	case 180:
		GPU_rotation = 180;
		GPU_width    = 256;
		GPU_height   = 192*2;
		rotationstartscan = 0;
		rotationscanlines = 192*2;
		MainWindow->setMinSize(GPU_width, (GPU_height + ScreenGap));
		break;

	case 270:
		GPU_rotation = 270;
		GPU_width    = 192*2;
		GPU_height   = 256;
		rotationstartscan = 0;
		rotationscanlines = 256;
		MainWindow->setMinSize((GPU_width + ScreenGap), GPU_height);
		break;
	}

	MainWindow->setClientSize(newwidth, newheight);

	/* Recreate the DirectDraw back buffer */
	if (lpBackSurface!=NULL)
	{
		IDirectDrawSurface7_Release(lpBackSurface);

		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize          = sizeof(ddsd);
		ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN;
		ddsd.dwWidth         = GPU_width;
		ddsd.dwHeight        = GPU_height;

		IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpBackSurface, NULL);
	}

	WritePrivateProfileInt("Video","Window Rotate",GPU_rotation,IniName);

	gpu_SetRotateScreen(GPU_rotation);

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
	ofn.lpstrFile = szChoice;
	ofn.lpstrDefExt = "avi";
	ofn.lpstrTitle = "Save AVI as";

	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if(GetSaveFileName(&ofn))
	{
		DRV_AviBegin(szChoice);
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
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

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
#ifdef EXPERIMENTAL_GBASLOT
	if(LoadROM(filename))
#else
	if(LoadROM(filename, bad_glob_cflash_disk_image_file))
#endif
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
	strncpy (fileFilter, "All Usable Files (*.nds, *.ds.gba, *.zip, *.gz)|*.nds;*.ds.gba;*.zip;*.gz|",512);
#endif			

#ifdef HAVE_LIBZZIP
	strncat (fileFilter, "Zipped NDS ROM file (*.zip)|*.zip|",512 - strlen(fileFilter));
#endif
#ifdef HAVE_LIBZ
	strncat (fileFilter, "GZipped NDS ROM file (*.gz)|*.gz|",512 - strlen(fileFilter));
#endif
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
	ofn.Flags = OFN_NOCHANGEDIR;

	if(!GetOpenFileName(&ofn))
	{
		if (romloaded)
		{
			CheatsSearchReset();
			NDS_UnPause(); //Restart emulation if no new rom chosen
		}
		return 0;
	}

	//LOG("%s\r\n", filename);
#ifdef EXPERIMENTAL_GBASLOT
	if(LoadROM(filename))
#else
	if(LoadROM(filename, bad_glob_cflash_disk_image_file))
#endif
	{
		romloaded = TRUE;
		NDS_UnPause();
	}

	return 0;
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
				key->handleKeyDown(key->param);
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

static void Unpause()
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
	if(state) {
		if(first) {
			frameAdvance=true;
		}
		else {
			execute = TRUE;
			first=false;
		}
	}
	else {
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
	CONFIGSCREEN_MICROPHONE
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
	case CONFIGSCREEN_WIFI:
#ifdef EXPERIMENTAL_WIFI
		if(wifiMac.netEnabled)
		{
			DialogBox(hAppInst,MAKEINTRESOURCE(IDD_WIFISETTINGS), hwnd, (DLGPROC) WifiSettingsDlgProc);
		}
		else
		{
#endif
			MessageBox(MainWindow->getHWnd(),"winpcap failed to initialize, and so wifi cannot be configured.","wifi system failure",0);
#ifdef EXPERIMENTAL_WIFI
		}
#endif
		break;
	}

	if (tpaused)
		NDS_UnPause();
}

//========================================================================================
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
	static int tmp_execute;
	switch (message)                  // handle the messages
	{
		case WM_ENTERMENULOOP:		  //Update menu items that needs to be updated dynamically
		{
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
			EnableMenuItem(mainMenu, IDM_GAME_INFO,         MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_IMPORTBACKUPMEMORY,MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_EXPORTBACKUPMEMORY,MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_STATE_SAVE,        MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_STATE_LOAD,        MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_PRINTSCREEN,       MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_QUICK_PRINTSCREEN, MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_FILE_RECORDAVI,    MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_FILE_RECORDWAV,    MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_RESET,             MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_SHUT_UP,           MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_CHEATS_LIST,       MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_CHEATS_SEARCH,     MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_WIFISETTINGS,      MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);

			EnableMenuItem(mainMenu, IDM_RECORD_MOVIE,      MF_BYCOMMAND | (romloaded && movieMode == MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_PLAY_MOVIE,        MF_BYCOMMAND | (romloaded && movieMode == MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, IDM_STOPMOVIE,         MF_BYCOMMAND | (romloaded && movieMode != MOVIEMODE_INACTIVE) ? MF_ENABLED : MF_GRAYED);

			EnableMenuItem(mainMenu, ID_RAM_WATCH,          MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(mainMenu, ID_RAM_SEARCH,         MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			//Update savestate slot items based on ROM loaded
			for (int x = 0; x < 10; x++)
			{
				EnableMenuItem(mainMenu, IDM_STATE_SAVE_F1+x,   MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
				EnableMenuItem(mainMenu, IDM_STATE_LOAD_F1+x,   MF_BYCOMMAND | (romloaded) ? MF_ENABLED : MF_GRAYED);
			}
			
			//Gray the recent ROM menu item if there are no recent ROMs
			EnableMenuItem(mainMenu, ID_FILE_RECENTROM,      MF_BYCOMMAND | (RecentRoms.size()) ? MF_ENABLED : MF_GRAYED);

			//Updated Checked menu items
			
			//Pause
			MainWindow->checkMenu(IDM_PAUSE, MF_BYCOMMAND | ((paused)?MF_CHECKED:MF_UNCHECKED));
			//Force Maintain Ratio
			MainWindow->checkMenu(IDC_FORCERATIO, MF_BYCOMMAND | ((ForceRatio)?MF_CHECKED:MF_UNCHECKED));
			//Screen rotation
			MainWindow->checkMenu(IDC_ROTATE0, MF_BYCOMMAND | ((GPU_rotation==0)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_ROTATE90, MF_BYCOMMAND | ((GPU_rotation==90)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_ROTATE180, MF_BYCOMMAND | ((GPU_rotation==180)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_ROTATE270, MF_BYCOMMAND | ((GPU_rotation==270)?MF_CHECKED:MF_UNCHECKED));

			//Window Size
			MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND   | ((windowSize==1)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW1_5X, MF_BYCOMMAND |((windowSize==65535)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND   | ((windowSize==2)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW2_5X, MF_BYCOMMAND |((windowSize==65534)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND   | ((windowSize==3)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND   | ((windowSize==4)?MF_CHECKED:MF_UNCHECKED));

			//Screen Separation
			MainWindow->checkMenu(IDM_SCREENSEP_NONE, MF_BYCOMMAND |   ((ScreenGap==0)? MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDM_SCREENSEP_BORDER, MF_BYCOMMAND | ((ScreenGap==5)? MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP, MF_BYCOMMAND | ((ScreenGap==64)?MF_CHECKED:MF_UNCHECKED));
	
			//Counters / Etc.
			MainWindow->checkMenu(ID_VIEW_FRAMECOUNTER, MF_BYCOMMAND | ((frameCounterDisplay)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(ID_VIEW_DISPLAYFPS, MF_BYCOMMAND   | ((FpsDisplay)         ?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(ID_VIEW_DISPLAYINPUT, MF_BYCOMMAND | ((ShowInputDisplay)   ?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(ID_VIEW_DISPLAYLAG, MF_BYCOMMAND   | ((ShowLagFrameCounter)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(ID_VIEW_DISPLAYMICROPHONE, MF_BYCOMMAND | ((ShowMicrophone)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(ID_VIEW_HUDEDITOR, MF_BYCOMMAND    | ((HudEditorMode)      ?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_FRAMELIMIT, MF_BYCOMMAND       | ((FrameLimit)         ?MF_CHECKED:MF_UNCHECKED));
			
			//Frame Skip
			MainWindow->checkMenu(IDC_FRAMESKIPAUTO, MF_BYCOMMAND | ((autoframeskipenab)?MF_CHECKED:MF_UNCHECKED));

			MainWindow->checkMenu(IDC_FRAMESKIP0, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==0) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP1, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==1) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP2, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==2) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP3, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==3) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP4, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==4) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP5, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==5) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP6, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==6) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP7, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==7) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP8, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==8) ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDC_FRAMESKIP9, MF_BYCOMMAND | (!autoframeskipenab && frameskiprate==9) ? MF_CHECKED:MF_UNCHECKED);

			MainWindow->checkMenu(IDM_MGPU, MF_BYCOMMAND | CommonSettings.showGpu.main ? MF_CHECKED:MF_UNCHECKED);
			MainWindow->checkMenu(IDM_SGPU, MF_BYCOMMAND | CommonSettings.showGpu.sub ? MF_CHECKED:MF_UNCHECKED);

			//Language selection

			MainWindow->checkMenu(IDC_BACKGROUNDPAUSE, MF_BYCOMMAND | ((lostFocusPause)?MF_CHECKED:MF_UNCHECKED));

			//Save type
			const int savelist[] = {IDC_SAVETYPE1,IDC_SAVETYPE2,IDC_SAVETYPE3,IDC_SAVETYPE4,IDC_SAVETYPE5,IDC_SAVETYPE6,IDC_SAVETYPE7};
			for(int i=0;i<7;i++) MainWindow->checkMenu(savelist[i], MF_BYCOMMAND | MF_UNCHECKED);
			MainWindow->checkMenu(savelist[CommonSettings.manualBackupType], MF_BYCOMMAND | MF_CHECKED);


			return 0;
		}
		/*case WM_EXITMENULOOP:
		{
		if (tmp_execute==2) NDS_UnPause();
		return 0;
		}*/

	case WM_CREATE:
		{
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
				WritePrivateProfileInt("Display", "FrameCounter", frameCounterDisplay, IniName);
				WritePrivateProfileInt("Display", "ScreenGap", ScreenGap, IniName);

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
				MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDC_WINDOW1_5X, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDC_WINDOW2_5X, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND | MF_UNCHECKED);
			}

			MainWindow->sizingMsg(wParam, lParam, ForceRatio);
		}
		return 1;
		//break;

	case WM_KEYDOWN:
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

			osd->update();
			Display();
			osd->clear();

			EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_DROPFILES:
		{
			char filename[MAX_PATH] = "";
			DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
			DragFinish((HDROP)wParam);
#ifdef EXPERIMENTAL_GBASLOT
			if(LoadROM(filename))
#else
			if(LoadROM(filename, bad_glob_cflash_disk_image_file))
#endif
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
			RECT r ;
			s32 x = (s32)((s16)LOWORD(lParam));
			s32 y = (s32)((s16)HIWORD(lParam));
			GetClientRect(hwnd,&r);
			int defwidth = 256, defheight = (384+ScreenGap);
			int winwidth = (r.right-r.left), winheight = (r.bottom-r.top);

			// translate from scaling (screen resolution to 256x384 or 512x192) 
			switch (GPU_rotation)
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

			if(HudEditorMode) {
				EditHud(x,y, &Hud);
			}
			else {
				//translate for rotation
				if (GPU_rotation != 0)
					translateXY(x,y);
				else 
					y-=(192+ScreenGap);
				if(x<0) x = 0; else if(x>255) x = 255;
				if(y<0) y = 0; else if(y>192) y = 192;
				NDS_setTouchPos(x, y);
				return 0;
			}
		}
		NDS_releaseTouch();
		return 0;

	case WM_LBUTTONUP:
		if(click)
			ReleaseCapture();
		NDS_releaseTouch();
		HudClickRelease(&Hud);
		return 0;

	case WM_INITMENU: {
		HMENU menu = (HMENU)wParam;
		//last minute modification of menu before display
		#ifndef DEVELOPER
			RemoveMenu(menu,IDM_DISASSEMBLER,MF_BYCOMMAND);
		#endif
		break;
	}

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
		case IDM_PRINTSCREEN:
			HK_PrintScreen(0);
			return 0;
		case IDM_QUICK_PRINTSCREEN:
			{
				NDS_WriteBMP("./printscreen.bmp");
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
		case IDM_STATE_LOAD:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "DeSmuME Savestate (*.dst)\0*.dst\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  SavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dst";

				if(!GetOpenFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

				savestate_load(SavName);
				Update_RAM_Watch();			//adelikat: TODO this should be a single function call in main, that way we can expand as future dialogs need updating
				Update_RAM_Search();		//hotkey.cpp - HK_StateLoadSlot also calls these functions
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
				ofn.lpstrFilter = "DeSmuME Savestate (*.dst)\0*.dst\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  SavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dst";

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
				HK_StateSaveSlot( abs(IDM_STATE_SAVE_F1 - LOWORD(wParam)) +1);
				return 0;

		case IDM_STATE_LOAD_F1:
			HK_StateLoadSlot(1);
			return 0;
		case IDM_STATE_LOAD_F2:
			HK_StateLoadSlot(2);
			return 0;
		case IDM_STATE_LOAD_F3:
			HK_StateLoadSlot(3);
			return 0;
		case IDM_STATE_LOAD_F4:
			HK_StateLoadSlot(4);
			return 0;
		case IDM_STATE_LOAD_F5:
			HK_StateLoadSlot(5);
			return 0;
		case IDM_STATE_LOAD_F6:
			HK_StateLoadSlot(6);
			return 0;
		case IDM_STATE_LOAD_F7:
			HK_StateLoadSlot(7);
			return 0;
		case IDM_STATE_LOAD_F8:
			HK_StateLoadSlot(8);
			return 0;
		case IDM_STATE_LOAD_F9:
			HK_StateLoadSlot(9);
			return 0;
		case IDM_STATE_LOAD_F10:
			HK_StateLoadSlot(10);
			return 0;
		case IDM_IMPORTBACKUPMEMORY:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "All supported types\0*.duc;*.sav\0Action Replay DS Save (*.duc)\0*.duc\0Raw Save format (*.sav)\0*.sav\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  ImportSavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "duc";

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
				ofn.lpstrFilter = "Raw Save format (*.sav)\0*.sav\0\0";
				ofn.nFilterIndex = 0;
				ofn.lpstrFile =  ImportSavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "sav";

				if(!GetSaveFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

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
			ViewRegisters->open();
			//IORegView_DlgOpen(HWND_DESKTOP, "I/O registers");
			return 0;
		case IDM_MEMORY:
		/*	ViewMem_ARM7->regClass("MemViewBox7", ViewMem_ARM7BoxProc);
			if (!ViewMem_ARM7->open())
				ViewMem_ARM7->unregClass();
			ViewMem_ARM9->regClass("MemViewBox9", ViewMem_ARM9BoxProc);
			if (!ViewMem_ARM9->open())
				ViewMem_ARM9->unregClass();*/
			if(!MemView_IsOpened(ARMCPU_ARM9)) MemView_DlgOpen(HWND_DESKTOP, "ARM9 memory", ARMCPU_ARM9);
			if(!MemView_IsOpened(ARMCPU_ARM7)) MemView_DlgOpen(HWND_DESKTOP, "ARM7 memory", ARMCPU_ARM7);
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
				MainWindow->checkMenu(IDM_MBG0, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 0);
				MainWindow->checkMenu(IDM_MBG0, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_MBG1 : 
			if(MainScreen.gpu->dispBG[1])
			{
				GPU_remove(MainScreen.gpu, 1);
				MainWindow->checkMenu(IDM_MBG1, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 1);
				MainWindow->checkMenu(IDM_MBG1, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_MBG2 : 
			if(MainScreen.gpu->dispBG[2])
			{
				GPU_remove(MainScreen.gpu, 2);
				MainWindow->checkMenu(IDM_MBG2, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 2);
				MainWindow->checkMenu(IDM_MBG2, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_MBG3 : 
			if(MainScreen.gpu->dispBG[3])
			{
				GPU_remove(MainScreen.gpu, 3);
				MainWindow->checkMenu(IDM_MBG3, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(MainScreen.gpu, 3);
				MainWindow->checkMenu(IDM_MBG3, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_SBG0 : 
			if(SubScreen.gpu->dispBG[0])
			{
				GPU_remove(SubScreen.gpu, 0);
				MainWindow->checkMenu(IDM_SBG0, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 0);
				MainWindow->checkMenu(IDM_SBG0, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_SBG1 : 
			if(SubScreen.gpu->dispBG[1])
			{
				GPU_remove(SubScreen.gpu, 1);
				MainWindow->checkMenu(IDM_SBG1, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 1);
				MainWindow->checkMenu(IDM_SBG1, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_SBG2 : 
			if(SubScreen.gpu->dispBG[2])
			{
				GPU_remove(SubScreen.gpu, 2);
				MainWindow->checkMenu(IDM_SBG2, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 2);
				MainWindow->checkMenu(IDM_SBG2, MF_BYCOMMAND | MF_CHECKED);
			}
			return 0;
		case IDM_SBG3 : 
			if(SubScreen.gpu->dispBG[3])
			{
				GPU_remove(SubScreen.gpu, 3);
				MainWindow->checkMenu(IDM_SBG3, MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				GPU_addBack(SubScreen.gpu, 3);
				MainWindow->checkMenu(IDM_SBG3, MF_BYCOMMAND | MF_CHECKED);
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
			frameCounterDisplay ^= 1;
			MainWindow->checkMenu(ID_VIEW_FRAMECOUNTER, frameCounterDisplay ? MF_CHECKED : MF_UNCHECKED);
			return 0;

		case ID_VIEW_DISPLAYFPS:
			FpsDisplay ^= 1;
			MainWindow->checkMenu(ID_VIEW_DISPLAYFPS, FpsDisplay ? MF_CHECKED : MF_UNCHECKED);
			WritePrivateProfileInt("Display", "Display Fps", FpsDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYINPUT:
			ShowInputDisplay ^= 1;
			MainWindow->checkMenu(ID_VIEW_DISPLAYINPUT, ShowInputDisplay ? MF_CHECKED : MF_UNCHECKED);
			WritePrivateProfileInt("Display", "Display Input", ShowInputDisplay, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_DISPLAYLAG:
			ShowLagFrameCounter ^= 1;
			MainWindow->checkMenu(ID_VIEW_DISPLAYLAG, ShowLagFrameCounter ? MF_CHECKED : MF_UNCHECKED);
			WritePrivateProfileInt("Display", "Display Lag Counter", ShowLagFrameCounter, IniName);
			osd->clear();
			return 0;

		case ID_VIEW_HUDEDITOR:
			HudEditorMode ^= 1;
			MainWindow->checkMenu(ID_VIEW_HUDEDITOR, HudEditorMode ? MF_CHECKED : MF_UNCHECKED);
			osd->clear();
			osd->border(HudEditorMode);
			return 0;

		case ID_VIEW_DISPLAYMICROPHONE:
			ShowMicrophone ^= 1;
			MainWindow->checkMenu(ID_VIEW_DISPLAYMICROPHONE, ShowMicrophone ? MF_CHECKED : MF_UNCHECKED);
			WritePrivateProfileInt("Display", "Display Microphone", ShowMicrophone, IniName);
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
			MainWindow->checkMenu(IDC_FRAMELIMIT, FrameLimit ? MF_CHECKED : MF_UNCHECKED);
			WritePrivateProfileInt("FrameLimit", "FrameLimit", FrameLimit, IniName);
			return 0;
		case IDM_SCREENSEP_NONE:
			{
				SetScreenGap(0);
				MainWindow->checkMenu(IDM_SCREENSEP_NONE, MF_BYCOMMAND | MF_CHECKED);
				MainWindow->checkMenu(IDM_SCREENSEP_BORDER, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP, MF_BYCOMMAND | MF_UNCHECKED);
				UpdateWndRects(hwnd);
			}
			return 0;
		case IDM_SCREENSEP_BORDER:
			{
				SetScreenGap(5);
				MainWindow->checkMenu(IDM_SCREENSEP_NONE, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDM_SCREENSEP_BORDER, MF_BYCOMMAND | MF_CHECKED);
				MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP, MF_BYCOMMAND | MF_UNCHECKED);
				UpdateWndRects(hwnd);
			}
			return 0;
		case IDM_SCREENSEP_NDSGAP:
			{
				SetScreenGap(64);
				MainWindow->checkMenu(IDM_SCREENSEP_NONE, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDM_SCREENSEP_BORDER, MF_BYCOMMAND | MF_UNCHECKED);
				MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP, MF_BYCOMMAND | MF_CHECKED);
				UpdateWndRects(hwnd);
			}
			return 0;
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
				RECT rc;
				GetClientRect(hwnd, &rc);
				ScaleScreen((rc.right - rc.left) / 256.0f);
				ForceRatio = TRUE;
				WritePrivateProfileInt("Video","Window Force Ratio",1,IniName);
				WritePrivateProfileInt("Video", "Window width", (rc.right - rc.left), IniName);
				WritePrivateProfileInt("Video", "Window height", (rc.bottom - rc.top), IniName);
			}
			break;


		case IDM_DEFSIZE:
			{
				if(windowSize)
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
					CommonSettings.HighResolutionInterpolateColor = IsDlgButtonChecked(hw,IDC_INTERPOLATECOLOR);
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

					CommonSettings.UseExtBIOS = IsDlgButtonChecked(hDlg, IDC_USEEXTBIOS);
					cur = GetDlgItem(hDlg, IDC_ARM9BIOS);
					GetWindowText(cur, CommonSettings.ARM9BIOS, 256);
					cur = GetDlgItem(hDlg, IDC_ARM7BIOS);
					GetWindowText(cur, CommonSettings.ARM7BIOS, 256);
					CommonSettings.SWIFromBIOS = IsDlgButtonChecked(hDlg, IDC_BIOSSWIS);

					CommonSettings.UseExtFirmware = IsDlgButtonChecked(hDlg, IDC_USEEXTFIRMWARE);
					cur = GetDlgItem(hDlg, IDC_FIRMWARE);
					GetWindowText(cur, CommonSettings.Firmware, 256);
					CommonSettings.BootFromFirmware = IsDlgButtonChecked(hDlg, IDC_FIRMWAREBOOT);

					CommonSettings.DebugConsole = IsDlgButtonChecked(hDlg, IDC_CHECKBOX_DEBUGGERMODE);

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
					ofn.Flags = OFN_NOCHANGEDIR;

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

			UseMicSample = GetPrivateProfileInt("Use Mic Sample", "UseMicSample", FALSE, IniName);
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

						UseMicSample = IsDlgButtonChecked(hDlg, IDC_USEMICSAMPLE);
						cur = GetDlgItem(hDlg, IDC_MICSAMPLE);
						GetWindowText(cur, MicSampleName, 256);
		
						WritePrivateProfileInt("Use Mic Sample", "UseMicSample", ((UseMicSample == true) ? 1 : 0), IniName);
						WritePrivateProfileString("Use Mic Sample", "MicSampleFile", MicSampleName, IniName);
						LoadSample(MicSampleName);
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
					ofn.lpstrFilter = "Any file(*.*)\0*.*\0\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFile = fileName;
					ofn.nMaxFile = 256;
					ofn.lpstrDefExt = "bin";
					ofn.Flags = OFN_NOCHANGEDIR;

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

			if(PCAP::pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1)
			{
				EndDialog(hDlg, TRUE);
				return TRUE;
			}

			cur = GetDlgItem(hDlg, IDC_BRIDGEADAPTER);
			for(i = 0, d = alldevs; d != NULL; i++, d = d->next)
			{
				ComboBox_AddString(cur, d->description);
			}
			ComboBox_SetCurSel(cur, CommonSettings.wifiBridgeAdapterNum);
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

					if((!romloaded) || (val == IDYES))
					{
						HWND cur;

						cur = GetDlgItem(hDlg, IDC_BRIDGEADAPTER);
						CommonSettings.wifiBridgeAdapterNum = ComboBox_GetCurSel(cur);

						WritePrivateProfileInt("Wifi", "BridgeAdapter", CommonSettings.wifiBridgeAdapterNum, IniName);

						if(romloaded)
						{
							NDS_Reset();
						}
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
					GetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr, 6);
					sscanf(tempstr, "%d", &sndbuffersize);
					WritePrivateProfileString("Sound", "SoundBufferSize", tempstr, IniName);

					if(sndcoretype != SPU_currentCoreNum)
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


