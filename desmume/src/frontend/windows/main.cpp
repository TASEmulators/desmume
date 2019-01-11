/*
	Copyright (C) 2006 Theo Berkau
	Copyright (C) 2006-2018 DeSmuME team

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

// icon gradient: #f6f6fb to #8080c0
// RGB(246, 246, 251) to RGB(128, 128, 192)

#include "main.h"
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

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

//emulator core
#include "MMU.h"
#include "armcpu.h"
#include "NDSSystem.h"
#include "driver.h"
#include "debug.h"
#include "saves.h"
#include "slot1.h"
#include "slot2.h"
#include "GPU.h"
#include "SPU.h"
#include "OGLRender.h"
#include "OGLRender_3_2.h"
#include "rasterize.h"
#include "gfx3d.h"
#include "render3D.h"
#include "gdbstub.h"
#include "wifi.h"
#include "cheatSystem.h"
#include "mic.h"
#include "movie.h"
#include "firmware.h"
#include "lua-engine.h"
#include "path.h"
#include "utils/advanscene.h"

//other random stuff
#include "rthreads/rthreads.h"
#include "recentroms.h"
#include "resource.h"
#include "CWindow.h"
#include "version.h"
#include "inputdx.h"
#include "console.h"
#include "throttle.h"
#include "hotkey.h"
#include "snddx.h"
#include "sndxa2.h"
#include "commandline.h"
#include "FEX_Interface.h"
#include "OpenArchive.h"
#include "utils/xstring.h"
#include "directx/ddraw.h"
#include "video.h"
#include "frontend/modules/osd/agg/agg_osd.h"
#include "frontend/modules/osd/agg/aggdraw.h"
#include "frontend/modules/osd/agg/agg2d.h"
#include "frontend/modules/ImageOut.h"
#include "winutil.h"
#include "ogl.h"
#include "display.h"

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
#include "slot1_config.h"
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
#include "importSave.h"
#include "fsnitroView.h"
//
//static size_t heapram = 0;
//void* operator new[](size_t amt)
//{
//	if(amt>5*1024*1024)
//	{
//		int zzz=9;
//	}
//	printf("Heap alloc up to %d bytes\n",heapram);
//	heapram += amt;
//	u32* buf = (u32*)malloc(amt+4);
//	*buf = amt;
//	return buf+1;
//}
//
//void operator delete[](void* ptr)
//{
//	if(!ptr) return;
//	u32* buf = (u32*)ptr;
//	buf--;
//	heapram -= *buf;
//	free(buf);
//}
//
//void* operator new(size_t amt)
//{
//	return operator new[](amt);
//}
//
//void operator delete(void* ptr)
//{
//	return operator delete[](ptr);
//}

//#include <libelf/libelf.h>
//#include <libelf/gelf.h> 
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <io.h>
//void testelf()
//{
//	//http://webcache.googleusercontent.com/search?q=cache:XeQF6AILcrkJ:chris.rohlf.googlepages.com/libelf-howto.c+libelf+example&cd=2&hl=en&ct=clnk&gl=us
//Elf32_Ehdr *elf_header;  /* ELF header */
//Elf *elf;                       /* Our Elf pointer for libelf */
//Elf_Scn *scn;                   /* Section Descriptor */
//Elf_Data *edata;                /* Data Descriptor */
//GElf_Sym sym;   /* Symbol */
//GElf_Shdr shdr;                 /* Section Header */ 
//int fd;   // File Descriptor
//const char* file = "d:\\devkitPro\\examples\\nds\\hello_world\\hello_world.elf";
//struct stat elf_stats; // fstat struct 
//char *base_ptr;  // ptr to our object in memory
//#define ERR -1 
//    if((fd = open(file, O_RDWR)) == ERR)
//        {
//             printf("couldnt open %s\n", file);
//             return;
//        } 
//        if((fstat(fd, &elf_stats)))
//        {
//             printf("could not fstat %s\n", file);
//                close(fd);
//             return ;
//        } 
//        if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
//        {
//             printf("could not malloc\n");
//                close(fd);
//             return ;
//        } 
//        if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
//        {
//             printf("could not read %s\n", file);
//                free(base_ptr);
//                close(fd);
//             return ;
//        }
//      /* Check libelf version first */
//      if(elf_version(EV_CURRENT) == EV_NONE)
//      {
//             printf("WARNING Elf Library is out of date!\n");
//      } 
//elf_header = (Elf32_Ehdr *) base_ptr; // point elf_header at our object in memory
//elf = elf_begin(fd, ELF_C_READ, NULL); // Initialize 'elf' pointer to our file descriptor 
///* Iterate through section headers */
//while((scn = elf_nextscn(elf, scn)) != 0)
//{
//      gelf_getshdr(scn, &shdr); 
//             // print the section header type
//                printf("Type: "); 
//                switch(shdr.sh_type)
//                {
//                        case SHT_NULL: printf( "SHT_NULL\t");               break;
//                        case SHT_PROGBITS: printf( "SHT_PROGBITS");       break;
//                        case SHT_SYMTAB: printf( "SHT_SYMTAB");           break;
//                        case SHT_STRTAB: printf( "SHT_STRTAB");           break;
//                        case SHT_RELA: printf( "SHT_RELA\t");               break;
//                        case SHT_HASH: printf( "SHT_HASH\t");               break;
//                        case SHT_DYNAMIC: printf( "SHT_DYNAMIC");         break;
//                        case SHT_NOTE: printf( "SHT_NOTE\t");               break;
//                        case SHT_NOBITS: printf( "SHT_NOBITS");           break;
//                        case SHT_REL: printf( "SHT_REL\t");                 break;
//                        case SHT_SHLIB: printf( "SHT_SHLIB");             break;
//                        case SHT_DYNSYM: printf( "SHT_DYNSYM");           break;
//                        case SHT_INIT_ARRAY: printf( "SHT_INIT_ARRAY");   break;
//                        case SHT_FINI_ARRAY: printf( "SHT_FINI_ARRAY");   break;
//                        case SHT_PREINIT_ARRAY: printf( "SHT_PREINIT_ARRAY"); break;
//                        case SHT_GROUP: printf( "SHT_GROUP");             break;
//                        case SHT_SYMTAB_SHNDX: printf( "SHT_SYMTAB_SHNDX"); break;
//                        case SHT_NUM: printf( "SHT_NUM\t");                 break;
//                        case SHT_LOOS: printf( "SHT_LOOS\t");               break;
//                        case SHT_GNU_verdef: printf( "SHT_GNU_verdef");   break;
//                        case SHT_GNU_verneed: printf( "SHT_VERNEED");     break;
//                        case SHT_GNU_versym: printf( "SHT_GNU_versym");   break;
//                        default: printf( "(none) ");                      break;
//                } 
//             // print the section header flags
//             printf("\t(");
//                if(shdr.sh_flags & SHF_WRITE) { printf("W"); }
//                if(shdr.sh_flags & SHF_ALLOC) { printf("A"); }
//                if(shdr.sh_flags & SHF_EXECINSTR) { printf("X"); }
//                if(shdr.sh_flags & SHF_STRINGS) { printf("S"); }
//             printf(")\t"); 
//      // the shdr name is in a string table, libelf uses elf_strptr() to find it
//      // using the e_shstrndx value from the elf_header
//      printf("%s\n", elf_strptr(elf, elf_header->e_shstrndx, shdr.sh_name));
//} 
//}

using namespace std;

//====================== Message box
#define MSG_ARG \
	char msg_buf[1024] = {0}; \
	{ \
		va_list args; \
		va_start (args, fmt); \
		vsprintf (msg_buf, fmt, args); \
		va_end (args); \
	}
void msgWndInfo(const char *fmt, ...)
{
	MSG_ARG;
	printf("[INFO] %s\n", msg_buf);
	MessageBox(MainWindow->getHWnd(), msg_buf, EMU_DESMUME_NAME_AND_VERSION(), MB_OK | MB_ICONINFORMATION);
}

bool msgWndConfirm(const char *fmt, ...)
{
	MSG_ARG;
	printf("[CONF] %s\n", msg_buf);
	return (MessageBox(MainWindow->getHWnd(), msg_buf, EMU_DESMUME_NAME_AND_VERSION(), MB_YESNO | MB_ICONQUESTION) == IDYES);
}

void msgWndError(const char *fmt, ...)
{
	MSG_ARG;
	printf("[ERR] %s\n", msg_buf);
	MessageBox(MainWindow->getHWnd(), msg_buf, EMU_DESMUME_NAME_AND_VERSION(), MB_OK | MB_ICONERROR);
}

void msgWndWarn(const char *fmt, ...)
{
	MSG_ARG;
	printf("[WARN] %s\n", msg_buf);
	MessageBox(MainWindow->getHWnd(), msg_buf, EMU_DESMUME_NAME_AND_VERSION(), MB_OK | MB_ICONWARNING);
}

msgBoxInterface msgBoxWnd = {
	msgWndInfo,
	msgWndConfirm,
	msgWndError,
	msgWndWarn,
};
//====================== Dialogs end

#include "winpcap.h"

#ifndef PUBLIC_RELEASE
#define DEVELOPER_MENU_ITEMS
#endif

static BOOL OpenCore(const char* filename);
BOOL Mic_DeInit_Physical();
BOOL Mic_Init_Physical();

void UpdateHotkeyAssignments();				//Appends hotkey mappings to corresponding menu items

DWORD dwMainThread;
HMENU mainMenu = NULL; //Holds handle to the main DeSmuME menu
CToolBar* MainWindowToolbar;

DWORD hKeyInputTimer;
bool start_paused;
extern bool killStylusTopScreen;
extern bool killStylusOffScreen;

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

//===================== Input vars
#define WM_CUSTKEYDOWN	(WM_USER+50)
#define WM_CUSTKEYUP	(WM_USER+51)

//#define WM_CUSTINVOKE	(WM_USER+52)

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

LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

char SavName[MAX_PATH] = {0};
char ImportSavName[MAX_PATH] = {0};
char szClassName[ ] = "DeSmuME";
int romnum = 0;

DWORD threadID;

WINCLASS	*MainWindow=NULL;
bool		gShowConsole = false;
bool		gConsoleTopmost = false;

//HWND hwnd;
//HDC  hdc;
HACCEL hAccel;
HINSTANCE hAppInst = NULL;
int WndX = 0;
int WndY = 0;

extern HWND RamSearchHWnd;
static bool lostFocusPause = true;
static bool lastPauseFromLostFocus = false;
bool FrameLimit = true;
extern bool allowBackgroundInput;

std::vector<HWND> LuaScriptHWnds;
LRESULT CALLBACK LuaScriptProc(HWND, UINT, WPARAM, LPARAM);

bool autoLoadLua;
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
TOOLSCLASS	*ViewFSNitro = NULL;

volatile bool execute = false;
volatile bool paused = true;
volatile BOOL pausedByMinimize = FALSE;
u32 glock = 0;

BOOL finished = FALSE;
bool romloaded = false;

void SetScreenGap(int gap);

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
int sndvolume=100;

const int possibleMSAA[] = {0, 2, 4, 8, 16, 32};
const int possibleBPP[] = {15, 18, 24};
const int possibleTexScale[] = {1, 2, 4};

int maxSamples = 0;
bool didGetMaxSamples = false;

SoundInterface_struct *SNDCoreList[] = {
	&SNDDummy,
	&SNDDIRECTX,
    &SNDXAUDIO2,
	NULL
};

GPU3DInterface *core3DList[] = {
	&gpu3DNull,
	&gpu3Dgl_3_2,
	&gpu3DRasterize,
	&gpu3DglOld,
	NULL
};

bool autoframeskipenab=1;
bool continuousframeAdvancing = false;

unsigned short windowSize = 0;
bool fsWindow = false;
bool autoHideCursor = false;

class GPUEventHandlerWindows : public GPUEventHandlerDefault
{
public:
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo)
	{
		if (isFrameSkipped)
		{
			return;
		}

		DRV_AviVideoUpdate(latestDisplayInfo);
	}
};

class WinPCapInterface : public ClientPCapInterface
{
public:
	virtual int findalldevs(void **alldevs, char *errbuf)
	{
		return _pcap_findalldevs((pcap_if_t **)alldevs, errbuf);
	}

	virtual void freealldevs(void *alldevs)
	{
		_pcap_freealldevs((pcap_if_t *)alldevs);
	}

	virtual void* open(const char *source, int snaplen, int flags, int readtimeout, char *errbuf)
	{
		return _pcap_open_live(source, snaplen, flags, readtimeout, errbuf);
	}

	virtual void close(void *dev)
	{
		_pcap_close((pcap_t *)dev);
	}

	virtual int setnonblock(void *dev, int nonblock, char *errbuf)
	{
		return _pcap_setnonblock((pcap_t *)dev, nonblock, errbuf);
	}

	virtual int sendpacket(void *dev, const void *data, int len)
	{
		return _pcap_sendpacket((pcap_t *)dev, (u_char *)data, len);
	}

	virtual int dispatch(void *dev, int num, void *callback, void *userdata)
	{
		if (callback == NULL)
		{
			return -1;
		}

		return _pcap_dispatch((pcap_t *)dev, num, (pcap_handler)callback, (u_char *)userdata);
	}
	
	virtual void breakloop(void *dev)
	{
		_pcap_breakloop((pcap_t *)dev);
	}
};

GPUEventHandlerWindows *WinGPUEvent = NULL;

LRESULT CALLBACK HUDFontSettingsDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp);
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
	} joyState [256*16];
	static bool initialized = false;

	if(!initialized) {
		for(int i = 0; i < 256*16; i++) {
			joyState[i].wasPressed = false;
			joyState[i].repeatCount = 1;
		}
		initialized = true;
	}

	const int nloops = (JOYSTICK) ? 16 : 1;
	const HWND mainWindow = MainWindow->getHWnd();
	const bool willAcceptInput = ( allowBackgroundInput || (mainWindow == GetForegroundWindow()) );

	for (int j = 0; j < nloops; j++)
	{
		for (int z = 0; z < 256; z++)
		{
			int i = z | (JOYSTICK?0x8000:0);
			i |= (j<<8);
			int n = i&0xFFF;
			bool active = willAcceptInput && !S9xGetState(i);

			if (active)
			{
				bool keyRepeat = (currentTime - joyState[n].firstPressedTime) >= (DWORD)KeyInDelayMSec;
				if (!joyState[n].wasPressed || keyRepeat)
				{
					if (!joyState[n].wasPressed)
						joyState[n].firstPressedTime = currentTime;
					joyState[n].lastPressedTime = currentTime;
					if (keyRepeat && joyState[n].repeatCount < 0xffff)
						joyState[n].repeatCount++;
					int mods = GetInitialModifiers(i);
					WPARAM wparam = i | (mods << 16) | (j<<8);
					PostMessage(mainWindow, WM_CUSTKEYDOWN, wparam,(LPARAM)(joyState[n].repeatCount | (joyState[n].wasPressed ? 0x40000000 : 0)));
				}
			}
			else
			{
				joyState[n].repeatCount = 1;
				if (joyState[n].wasPressed)
				{
					int mods = GetInitialModifiers(i);
					WPARAM wparam = i | (mods << 16) | (j<<8);
					PostMessage(mainWindow, WM_CUSTKEYUP, wparam,(LPARAM)(joyState[n].repeatCount | (joyState[n].wasPressed ? 0x40000000 : 0)));
				}
			}
			joyState[n].wasPressed = active;
		}
	}
	lastTime = currentTime;
}

VOID CALLBACK KeyInputTimer( UINT idEvent, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	InputTimer<false>();
	InputTimer<true>();
}

void ScaleScreen(float factor, bool user)
{
	if(user) // have to exit maximized mode if the user told us to set a specific window size
	{
		HWND hwnd = MainWindow->getHWnd();
		if (IsZoomed(hwnd) || fsWindow)
		{
			RestoreWindow(hwnd);
			windowSize = factor;
		}
	}

	if(windowSize == 0)
	{
		int defw = GetPrivateProfileInt("Video", "Window width", GPU_FRAMEBUFFER_NATIVE_WIDTH, IniName);
		int defh = GetPrivateProfileInt("Video", "Window height", GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2, IniName);

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
				w1x = video.rotatedwidthgap() * 2 / screenSizeRatio;
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

		//correct aspect ratio if that option's been turned on
		if(ForceRatio)
		{
			if(defh*w1x < h1x*defw)
				defh = defw*h1x/w1x;
			else if(defw*h1x < w1x*defh)
				defw = defh*w1x/h1x;
		}

		MainWindow->setClientSize(defw, defh);
	}
	else
	{
		if(factor==kScale1point5x)
			factor = 1.5f;
		else if(factor==kScale2point5x)
			factor = 2.5f;
		
		//don't incorporate prescale into these calculations
		factor /= video.prescaleHD;

		if (video.layout == 0)
			MainWindow->setClientSize((int)(video.rotatedwidthgap() * factor), (int)(video.rotatedheightgap() * factor));
		else
			if (video.layout == 1)
			{
				MainWindow->setClientSize((int)(video.rotatedwidthgap() * factor * 2 / screenSizeRatio), (int)(video.rotatedheightgap() * factor / 2));
			}
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
	const bool isMainGPUFirst = (GPU->GetDisplayInfo().engineID[NDSDisplayID_Main] == GPUEngineID_Main);

	if (video.layout == 0 || video.layout == 2)
	{
		if(whichScreen)
		{
			bool topOnTop = (video.swap == 0) || (video.swap == 2 && isMainGPUFirst) || (video.swap == 3 && !isMainGPUFirst);
			bool bottom = (whichScreen > 0);
			if(topOnTop)
				y += bottom ? -GPU_FRAMEBUFFER_NATIVE_HEIGHT : 0;
			else
				y += (y < GPU_FRAMEBUFFER_NATIVE_HEIGHT) ? (bottom ? 0 : GPU_FRAMEBUFFER_NATIVE_HEIGHT) : (bottom ? 0 : -GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
	}
	else if (video.layout == 1) // side-by-side
	{
		if(whichScreen)
		{
			bool topOnTop = (video.swap == 0) || (video.swap == 2 && isMainGPUFirst) || (video.swap == 3 && !isMainGPUFirst);
			bool bottom = (whichScreen > 0);
			if(topOnTop && bottom)
			{
				x = (x - GPU_FRAMEBUFFER_NATIVE_WIDTH  * screenSizeRatio) / (2 - screenSizeRatio);
				if(ForceRatio)
				{
					y *= screenSizeRatio / (2 - screenSizeRatio);
					if (vCenterResizedScr)
						y += GPU_FRAMEBUFFER_NATIVE_HEIGHT * (1 - screenSizeRatio) / (2 - screenSizeRatio);
				}
			}
			else
			{
				x /= screenSizeRatio;
				if(!bottom) 
				{
					if(x < GPU_FRAMEBUFFER_NATIVE_WIDTH)
						x += GPU_FRAMEBUFFER_NATIVE_WIDTH;
					else
						x += -GPU_FRAMEBUFFER_NATIVE_WIDTH;
				}
			}
		}
		else
		{
			if(x >= GPU_FRAMEBUFFER_NATIVE_WIDTH * screenSizeRatio)
			{
				x = (x - GPU_FRAMEBUFFER_NATIVE_WIDTH  * screenSizeRatio) / (2 - screenSizeRatio);
				if(ForceRatio)
				{
					y *= screenSizeRatio / (2 - screenSizeRatio);
					if (vCenterResizedScr)
						y += GPU_FRAMEBUFFER_NATIVE_HEIGHT * (1 - screenSizeRatio) / (2 - screenSizeRatio);
				}
				y += GPU_FRAMEBUFFER_NATIVE_HEIGHT;
				if(y < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
					y = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
			}
			else
			{
				x /= screenSizeRatio;
				if(x < 0)
					x = 0;
				if(y > GPU_FRAMEBUFFER_NATIVE_HEIGHT)
					y = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
			}
		}
	}
}

//-----window style handling----
const u32 DWS_NORMAL = 0;
const u32 DWS_ALWAYSONTOP = 1;
const u32 DWS_LOCKDOWN = 2;
const u32 DWS_FULLSCREEN = 4;
const u32 DWS_VSYNC = 8;
const u32 DWS_FS_MENU = 256;
const u32 DWS_FS_WINDOW = 512;

static u32 currWindowStyle = DWS_NORMAL;
void SetStyle(u32 dws);
static u32 GetStyle() { return currWindowStyle; }

static void ToggleFullscreen()
{
	u32 style = GetStyle();
	HWND hwnd = MainWindow->getHWnd();

	if(style&DWS_FULLSCREEN)
		RestoreWindow(hwnd);
	else ShowFullScreen(hwnd);
}
//---------

void SaveWindowSizePos(HWND hwnd)
{
	SaveWindowPos(hwnd);
	SaveWindowSize(hwnd);
	WritePrivateProfileInt("Video", "Window Size", windowSize, IniName);
}

void RestoreWindow(HWND hwnd)
{
	fsWindow = false;
	u32 style = GetStyle();

	if (style & DWS_FULLSCREEN)
	{
		style &= ~DWS_FULLSCREEN;
		SetStyle(style);
		ShowWindow(hwnd, SW_NORMAL);

		windowSize = GetPrivateProfileInt("Video", "Window Size", 0, IniName);
		WndX = GetPrivateProfileInt("Video", "WindowPosX", CW_USEDEFAULT, IniName);
		WndY = GetPrivateProfileInt("Video", "WindowPosY", CW_USEDEFAULT, IniName);

		RECT rc;
		rc.left = WndX;
		rc.top = WndY;
		rc.right = rc.left + GetPrivateProfileInt("Video", "Window width", GPU_FRAMEBUFFER_NATIVE_WIDTH, IniName);
		rc.bottom = rc.top + GetPrivateProfileInt("Video", "Window height", GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2, IniName);

		AdjustWindowRect(&rc, GetWindowLong(hwnd, GWL_STYLE), true);
		SetWindowPos(hwnd, 0, WndX, WndY, rc.right - rc.left, rc.bottom - rc.top, SWP_NOOWNERZORDER | SWP_NOZORDER);
	}
	else
		ShowWindow(hwnd, SW_NORMAL);
}

void ShowFullScreen(HWND hwnd)
{
	SaveWindowSizePos(hwnd);

	u32 style = GetStyle();
	style |= DWS_FULLSCREEN;
	SetStyle(style);

	if (style & DWS_FS_WINDOW)
	{
		windowSize = 0;
		fsWindow = true;

		HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &info);
		int monitor_width = info.rcMonitor.right - info.rcMonitor.left;
		int monitor_height = info.rcMonitor.bottom - info.rcMonitor.top;

		SetWindowPos(hwnd, 0, info.rcMonitor.left, info.rcMonitor.top, monitor_width, monitor_height, SWP_NOOWNERZORDER | SWP_NOZORDER);
	}
	else
	{
		fsWindow = false;
		ShowWindow(hwnd, SW_MAXIMIZE);
	}
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

void FixAspectRatio();

void LCDsSwap(int swapVal)
{
	if(swapVal == -1) swapVal = video.swap ^ 1; // -1 means to flip the existing setting
	if(swapVal < 0 || swapVal > 3) swapVal = 0;
	if(osd && !(swapVal & video.swap & 1)) osd->swapScreens = !osd->swapScreens; // 1-frame fixup
	video.swap = swapVal;
	WritePrivateProfileInt("Video", "LCDsSwap", video.swap, IniName);
}

void doLCDsLayout(int videoLayout)
{
	HWND hwnd = MainWindow->getHWnd();
	u32 style = GetStyle();
	bool maximized = IsZoomed(hwnd) || fsWindow;
	if (maximized)
		RestoreWindow(hwnd);

	if (videoLayout > 2) videoLayout = 0;

	if(videoLayout != 0)
	{
		// rotation is not supported in the alternate layouts
		if(video.rotation != 0)
			SetRotate(hwnd, 0, false);
	}

	video.layout = videoLayout;

	osd->singleScreen = (video.layout == 2);

	RECT rc = { 0 };
	int oldheight, oldwidth;
	int newheight, newwidth;

	GetClientRect(hwnd, &rc);
	oldwidth = (rc.right - rc.left);
	oldheight = (rc.bottom - rc.top) - MainWindowToolbar->GetHeight();
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

		MainWindowToolbar->EnableButton(IDC_ROTATE90, true);
		MainWindowToolbar->EnableButton(IDC_ROTATE270, true);

		if (video.layout_old == 1)
		{
			newwidth = oldwidth * screenSizeRatio / 2;
			newheight = (oldheight * 2) + (video.screengap * oldheight / GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
		else if (video.layout_old == 2)
		{
			newwidth = oldwidth;
			newheight = (oldheight * 2) + (video.screengap * oldheight / GPU_FRAMEBUFFER_NATIVE_HEIGHT);
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

		// As rotation was reset to 0, the button IDs were reset to 
		// IDC_ROTATE90 and IDC_ROTATE270.
		MainWindowToolbar->EnableButton(IDC_ROTATE90, false);
		MainWindowToolbar->EnableButton(IDC_ROTATE270, false);

		int scaledGap = oldheight - ((MainScreenRect.bottom - MainScreenRect.top) + (SubScreenRect.bottom - SubScreenRect.top));

		if (video.layout == 1)
		{
			if (video.layout_old == 0)
			{
				newwidth = oldwidth * 2 / screenSizeRatio;
				newheight = (oldheight - scaledGap) / 2;
			}
			else if (video.layout_old == 2)
			{
				newwidth = oldwidth * 2 / screenSizeRatio;
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
		else if (video.layout == 2)
		{
			if (video.layout_old == 0)
			{
				newwidth = oldwidth;
				newheight = (oldheight - scaledGap) / 2;
			}
			else if (video.layout_old == 1)
			{
				newwidth = oldwidth * screenSizeRatio / 2;
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
	
	if (maximized)
	{
		if (style & DWS_FULLSCREEN)
			ShowFullScreen(hwnd);
		else
			ShowWindow(hwnd, SW_MAXIMIZE);
	}
}

void CheckMessages()
{
	MSG msg;
	HWND hwnd = MainWindow->getHWnd();

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
	if (gShowConsole)
		readConsole();
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

	DRV_AviFrameStart();

	inFrameBoundary = false;
	{
		Lock lock;
		NDS_exec<false>();
		SPU_Emulate_user();
		win_sound_samplecounter = DESMUME_SAMPLE_RATE/60;
	}
	inFrameBoundary = true;

	DRV_AviFileWriteStart();

	CallRegisteredLuaFunctions(LUACALL_AFTEREMULATION);
	ServiceDisplayThreadInvocations();
}

static void StepRunLoop_Paused()
{
	paused = TRUE;
	Sleep(50);

	// periodically update single-core OSD when paused and in the foreground
	if(CommonSettings.single_core() && GetActiveWindow() == mainLoopData.hwnd)
	{
		video.srcBuffer = (u8*)GPU->GetDisplayInfo().masterCustomBuffer;
		DoDisplay();
	}

	ServiceDisplayThreadInvocations();
}

static void StepRunLoop_User()
{
	const int kFramesPerToolUpdate = 1;

	Hud.fps = mainLoopData.fps;
	Hud.fps3d = GPU->GetFPSRender3D();

	if (mainLoopData.framesskipped == 0)
	{
		WaitForSingleObject(display_done_event, display_done_timeout);
		Display();
	}
	ResetEvent(display_done_event);

	mainLoopData.fps3d = Hud.fps3d;

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
		u32 loadAvgARM9;
		u32 loadAvgARM7;
		NDS_GetCPULoadAverage(loadAvgARM9, loadAvgARM7);
		
		Hud.cpuload[ARMCPU_ARM9] = (int)loadAvgARM9;
		Hud.cpuload[ARMCPU_ARM7] = (int)loadAvgARM7;
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
		emu_halt(EMUHALT_REASON_USER_REQUESTED_HALT, NDSErrorTag_None);
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

	u32 res = ddraw.create(hwnd);
	if (res != 0)
	{
		MessageBox(hwnd,DDerrors[res],EMU_DESMUME_NAME_AND_VERSION(),MB_OK | MB_ICONERROR);
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

//returns true if it just now paused
bool NDS_Pause(bool showMsg)
{
	if(paused) return false;

	emu_halt(EMUHALT_REASON_USER_REQUESTED_HALT, NDSErrorTag_None);
	paused = TRUE;
	SPU_Pause(1);
	while (!paused) {}
	if (showMsg) INFO("Emulation paused\n");

	SetWindowText(MainWindow->getHWnd(), "Paused");
	MainWindowToolbar->ChangeButtonBitmap(IDM_PAUSE, IDB_PLAY);
	return true;
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

		SetWindowText(MainWindow->getHWnd(), (char*)EMU_DESMUME_NAME_AND_VERSION());
		MainWindowToolbar->ChangeButtonBitmap(IDM_PAUSE, IDB_PAUSE);
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
	ModifyMenu(mainMenu,IDM_STATE_SAVE_F10 + pos, MF_BYCOMMAND | MF_STRING, IDM_STATE_SAVE_F10 + pos, txt);
	ModifyMenu(mainMenu,IDM_STATE_LOAD_F10 + pos, MF_BYCOMMAND | MF_STRING, IDM_STATE_LOAD_F10 + pos, txt);
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
	char ntxt[16];
	for(int i = 0; i < NB_STATES;i++)
	{
		snprintf(ntxt, sizeof(ntxt), "&%d", i);
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
			snprintf(ntxt, sizeof(ntxt), "&%d    %s", i, savestates[i].date);
			UpdateSaveStateMenu(i, ntxt);
		}
	}
}

static BOOL LoadROM(const char * filename, const char * physicalName, const char * logicalName)
{
	ResetSaveStateTimes();
	Pause();
	//if (strcmp(filename,"")!=0) INFO("Attempting to load ROM: %s\n",filename);

	if (NDS_LoadROM(filename, physicalName, logicalName) > 0)
	{
		INFO("Loading %s was successful\n",logicalName);
		
		NDS_SLOT2_TYPE selectedSlot2Type = slot2_GetSelectedType();
		Guitar.Enabled	= (selectedSlot2Type == NDS_SLOT2_GUITARGRIP)?true:false;
		Piano.Enabled	= (selectedSlot2Type == NDS_SLOT2_EASYPIANO)?true:false;
		Paddle.Enabled	= (selectedSlot2Type == NDS_SLOT2_PADDLE)?true:false;
		
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
	else
		msgbox->error("Loading %s FAILED.\n", logicalName);

	return FALSE;
}

void OpenRecentROM(int listNum)
{
	if (listNum > MAX_RECENT_ROMS) return; //Just in case
	if (listNum >= (int)RecentRoms.size()) return;
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

bool DemandLua()
{
	HMODULE mod = LoadLibrary("lua51.dll");
	if(!mod)
	{
		MessageBox(NULL, "lua51.dll was not found. Please get it into your PATH or in the same directory as desmume.exe", "DeSmuME", MB_OK | MB_ICONERROR);
		return false;
	}
	FreeLibrary(mod);
	return true;
}


int MenuInit()
{
	MENUITEMINFO mm = {0};

	mainMenu = LoadMenu(hAppInst, MAKEINTRESOURCE(MENU_PRINCIPAL)); //Load Menu, and store handle
	if (!MainWindow->setMenu(mainMenu)) return 0;

	InitRecentRoms();

	ResetSaveStateTimes();

	HMENU configMenu = GetSubMenuByIdOfFirstChild(mainMenu, IDM_3DCONFIG);
	HMENU toolsMenu = GetSubMenuByIdOfFirstChild(mainMenu, IDM_DISASSEMBLER);

#ifndef DEVELOPER_MENU_ITEMS
	// menu items that are only useful for desmume developers (maybe)
	HMENU fileMenu = GetSubMenu(mainMenu, 0);
	DeleteMenu(fileMenu, IDM_FILE_RECORDUSERSPUWAV, MF_BYCOMMAND);
#endif

	//zero 27-mar-2015 - removing this.. its just glitchy and rarely maintained.
	//add a different dialog, near the save import (perhaps based on it, where the save is cleared and re-initialized instead of imported) to restore this in the future if needed
//#ifdef DEVELOPER
//	for(int i=0; i<MAX_SAVE_TYPES; i++)
//	{
//		memset(&mm, 0, sizeof(MENUITEMINFO));
//		
//		mm.cbSize = sizeof(MENUITEMINFO);
//		mm.fMask = MIIM_TYPE | MIIM_ID;
//		mm.fType = MFT_STRING;
//		mm.wID = IDC_SAVETYPE+i+1;
//		mm.dwTypeData = (LPSTR)save_types[i].descr;
//
//		MainWindow->addMenuItem(IDC_SAVETYPE, false, &mm);
//	}
//	memset(&mm, 0, sizeof(MENUITEMINFO));
//	mm.cbSize = sizeof(MENUITEMINFO);
//	mm.fMask = MIIM_TYPE;
//	mm.fType = MFT_SEPARATOR;
//	MainWindow->addMenuItem(IDC_SAVETYPE, false, &mm);
//#else
//	DeleteMenu(configMenu,GetSubMenuIndexByHMENU(configMenu,advancedMenu),MF_BYPOSITION);
//#endif

	return 1;
}

void MenuDeinit()
{
	MainWindow->setMenu(NULL);
	DestroyMenu(mainMenu);
}


static void ExitRunLoop()
{
	finished = TRUE;
	emu_halt(EMUHALT_REASON_USER_REQUESTED_HALT, NDSErrorTag_None);
}

//-----------------------------------------------------------------------------
//   Platform driver for Win32
//-----------------------------------------------------------------------------

class WinDriver : public BaseDriver
{
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

		if(finished) ExitProcess(0); //I guess.... fixes hangs when exiting while a lua script is running

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

	virtual void EMU_DebugIdleUpdate()
	{
		CheckMessages();
		Sleep(1);
	}
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

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

static void SyncGpuBpp()
{
	//either of these works. 666 must be packed as 888
	if(gpu_bpp == 18)
		GPU->SetColorFormat(NDSColorFormat_BGR666_Rev);
	else if(gpu_bpp == 15)
		GPU->SetColorFormat(NDSColorFormat_BGR555_Rev);
	else
		GPU->SetColorFormat(NDSColorFormat_BGR888_Rev);
}

static BOOL OpenCoreSystemCP(const char* filename_syscp)
{
	wchar_t wgarbage[1024];
	char garbage[1024];
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, filename_syscp, -1, wgarbage, 1024);
	int q = WideCharToMultiByte(CP_UTF8, 0, wgarbage, -1, garbage, 1024, NULL, NULL);
	return OpenCore(garbage);
}

#define RENDERID_NULL_SAVED -1
#define GPU3D_DEFAULT  GPU3D_SWRAST

DWORD wmTimerRes;
int _main()
{
	//7zip initialization
	InitDecoder();

	dwMainThread = GetCurrentThreadId();

	//enable opengl 3.2 driver in this port
	OGLLoadEntryPoints_3_2_Func = OGLLoadEntryPoints_3_2;
	OGLCreateRenderer_3_2_Func = OGLCreateRenderer_3_2;

	bool isSocketsSupported = false;
	bool isPCapSupported = false;

	WSADATA wsaData; 	 
	WORD version = MAKEWORD(2,2); 

	// Start up Windows Sockets.
	if (WSAStartup(version, &wsaData) == 0)
	{
		// Check for a matching DLL version. If the version doesn't match, then bail.
		if ( (LOBYTE(wsaData.wVersion) == 2) && (HIBYTE(wsaData.wVersion) == 2) )
		{
			isSocketsSupported = true;
		}
		else
		{
			WSACleanup();
		}
	}

	LoadWinPCap(isPCapSupported);

	driver = new WinDriver();
	WinGPUEvent = new GPUEventHandlerWindows;

	InitializeCriticalSection(&win_execute_sync);
	InitializeCriticalSection(&win_backbuffer_sync);
	InitializeCriticalSection(&display_invoke_handler_cs);
	display_invoke_ready_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	display_invoke_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	display_wakeup_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	display_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);

//	struct configured_features my_config;

	oglrender_init = windows_opengl_init;


	//try and detect this for users who don't specify it on the commandline
	//(can't say I really blame them)
	//this helps give a substantial speedup for singlecore users
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	CommonSettings.num_cores = systemInfo.dwNumberOfProcessors;

	msgbox = &msgBoxWnd;

	char text[80] = {0};
	char scrRatStr[4] = "1.0";

	path.ReadPathSettings();

	CommonSettings.loadToMemory = GetPrivateProfileBool("General", "ROM Loading Mode", false, IniName);
	CommonSettings.cheatsDisable = GetPrivateProfileBool("General", "cheatsDisable", false, IniName);
	CommonSettings.autodetectBackupMethod = GetPrivateProfileInt("General", "autoDetectMethod", 0, IniName);
	CommonSettings.backupSave = GetPrivateProfileBool("General", "backupSave", false, IniName);

	ColorCtrl_Register();
	if (!RegWndClass("DeSmuME", WindowProcedure, CS_DBLCLKS, LoadIcon(hAppInst, MAKEINTRESOURCE(ICONDESMUME))))
	{
		MessageBox(NULL, "Error registering windows class", "DeSmuME", MB_OK);
		exit(-1);
	}

	u32 style = DWS_NORMAL;
	if(GetPrivateProfileBool("Video","Window Always On Top", false, IniName)) style |= DWS_ALWAYSONTOP;
	if(GetPrivateProfileBool("Video","Window Lockdown", false, IniName)) style |= DWS_LOCKDOWN;
	if(GetPrivateProfileBool("Display", "Show Menu In Fullscreen Mode", false, IniName)) style |= DWS_FS_MENU;
	if (GetPrivateProfileBool("Display", "Non-exclusive Fullscreen Mode", false, IniName)) style |= DWS_FS_WINDOW;
	
	gldisplay.filter = GetPrivateProfileBool("Video","Display Method Filter", false, IniName);
	if(GetPrivateProfileBool("Video","VSync", false, IniName))
		style |= DWS_VSYNC;
	displayMethod = GetPrivateProfileInt("Video","Display Method", DISPMETHOD_DDRAW_HW, IniName);

	windowSize = GetPrivateProfileInt("Video","Window Size", 0, IniName);
	video.rotation =  GetPrivateProfileInt("Video","Window Rotate", 0, IniName);
	video.rotation_userset =  GetPrivateProfileInt("Video","Window Rotate Set", video.rotation, IniName);
	ForceRatio = GetPrivateProfileBool("Video","Window Force Ratio", 1, IniName);
	PadToInteger = GetPrivateProfileBool("Video","Window Pad To Integer", 0, IniName);
	WndX = GetPrivateProfileInt("Video","WindowPosX", CW_USEDEFAULT, IniName);
	WndY = GetPrivateProfileInt("Video","WindowPosY", CW_USEDEFAULT, IniName);
	if(WndX < -10000) WndX = CW_USEDEFAULT; // fix for missing window problem
	if(WndY < -10000) WndY = CW_USEDEFAULT; // (happens if you close desmume while it's minimized)
  video.width = GPU_FRAMEBUFFER_NATIVE_WIDTH;
  video.height = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	//video.width = GetPrivateProfileInt("Video", "Width", GPU_FRAMEBUFFER_NATIVE_WIDTH, IniName);
	//video.height = GetPrivateProfileInt("Video", "Height", GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2, IniName);
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
	CommonSettings.hud.ShowRTC = GetPrivateProfileBool("Display","Display RTC", false, IniName);

	CommonSettings.micMode = (TCommonSettings::MicMode)GetPrivateProfileInt("MicSettings", "MicMode", (int)TCommonSettings::InternalNoise, IniName);
	GetPrivateProfileString("MicSettings", "MicSampleFile", "micsample.raw", MicSampleName, MAX_PATH, IniName);
	RefreshMicSettings();
	
	autoHideCursor = GetPrivateProfileBool("Display", "Auto-Hide Cursor", false, IniName);
	GetPrivateProfileString("Display", "Screen Size Ratio", "1.0", scrRatStr, 4, IniName);
	screenSizeRatio = atof(scrRatStr);
	vCenterResizedScr = GetPrivateProfileBool("Display", "Vertically Center Resized Screen", 1, IniName);
	video.screengap = GetPrivateProfileInt("Display", "ScreenGap", 0, IniName);
	SeparationBorderDrag = GetPrivateProfileBool("Display", "Window Split Border Drag", true, IniName);
	ScreenGapColor = GetPrivateProfileInt("Display", "ScreenGapColor", 0xFFFFFF, IniName);
	CommonSettings.showGpu.main = GetPrivateProfileInt("Display", "MainGpu", 1, IniName) != 0;
	CommonSettings.showGpu.sub = GetPrivateProfileInt("Display", "SubGpu", 1, IniName) != 0;
	CommonSettings.spu_advanced = GetPrivateProfileBool("Sound", "SpuAdvanced", true, IniName);
	CommonSettings.advanced_timing = GetPrivateProfileBool("Emulation", "AdvancedTiming", true, IniName);
	CommonSettings.gamehacks.en = GetPrivateProfileBool("Emulation", "GameHacks", true, IniName);
	CommonSettings.GFX3D_Renderer_TextureDeposterize =  GetPrivateProfileBool("3D", "TextureDeposterize", 0, IniName);
	CommonSettings.GFX3D_Renderer_TextureSmoothing =  GetPrivateProfileBool("3D", "TextureSmooth", 0, IniName);
	gpu_bpp = GetValid3DIntSetting("GpuBpp", 18, possibleBPP, 3);
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

	autoLoadLua = GetPrivateProfileBool("Scripting", "AutoLoad", false, IniName);

	for(int i = 0; i < MAX_RECENT_SCRIPTS; i++)
	{
		char str[256];
		sprintf(str, "Recent Lua Script %d", i+1);
		GetPrivateProfileString("Scripting", str, "", &Recent_Scripts[i][0], 1024, IniName);
	}

#ifdef HAVE_JIT
	//zero 06-sep-2012 - shouldnt be defaulting this to true for now, since the jit is buggy. 
	//id rather have people discover a bonus speedhack than discover new bugs in a new version
	CommonSettings.use_jit = GetPrivateProfileBool("Emulation", "CPUmode", false, IniName);
	CommonSettings.jit_max_block_size = GetPrivateProfileInt("Emulation", "JitSize", 12, IniName);

	if (CommonSettings.jit_max_block_size < 1)
	{
		CommonSettings.jit_max_block_size = 1;
	}
	else if (CommonSettings.jit_max_block_size > 100)
	{
		CommonSettings.jit_max_block_size = 100;
	}
#else
	CommonSettings.use_jit = false;
#endif

	//i think we should override the ini file with anything from the commandline
	CommandLine cmdline;
	if(!cmdline.parse(__argc,__argv)) {
		cmdline.errorHelp(__argv[0]);
		return 1;
	}
	cmdline.validate();
	start_paused = cmdline.start_paused!=0;
	
	FrameLimit = (cmdline.disable_limiter == 1) ? false : GetPrivateProfileBool("FrameLimit", "FrameLimit", true, IniName);
	
	// Temporary scanline parameter setting for Windows.
	// TODO: When videofilter.cpp becomes Windows-friendly, replace the direct setting of
	// variables with SetFilterParameteri()	.
	//myVideoFilterObject->SetFilterParameteri(VF_PARAM_SCANLINE_A, _scanline_filter_a);
    //myVideoFilterObject->SetFilterParameteri(VF_PARAM_SCANLINE_B, _scanline_filter_b);
    //myVideoFilterObject->SetFilterParameteri(VF_PARAM_SCANLINE_C, _scanline_filter_c);
    //myVideoFilterObject->SetFilterParameteri(VF_PARAM_SCANLINE_D, _scanline_filter_d);
	scanline_filter_a = _scanline_filter_a;
	scanline_filter_b = _scanline_filter_b;
	scanline_filter_c = _scanline_filter_c;
	scanline_filter_d = _scanline_filter_d;

	Desmume_InitOnce();
	aggDraw.hud->setFont(fonts_list[GetPrivateProfileInt("Display","HUD Font", font_Nums-1, IniName)].name);

	//in case this isnt actually a singlecore system, but the user requested it
	//then restrict ourselves to one core
	if(CommonSettings.single_core())
		SetProcessAffinityMask(GetCurrentProcess(),1);

	MainWindow = new WINCLASS("DeSmuME", hAppInst);
	if (!MainWindow->create((char*)EMU_DESMUME_NAME_AND_VERSION(), WndX, WndY, video.width,video.height+video.screengap,
		WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
		NULL))
	{
		MessageBox(NULL, "Error creating main window", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	//disable wacky stylus stuff
	//TODO - we are obliged to call GlobalDeleteAtom
	GlobalAddAtom(MICROSOFT_TABLETPENSERVICE_PROPERTY);
	SetProp(MainWindow->getHWnd(),MICROSOFT_TABLETPENSERVICE_PROPERTY,(HANDLE)(
		TABLET_DISABLE_PRESSANDHOLD |
			TABLET_DISABLE_PENTAPFEEDBACK |
			TABLET_DISABLE_PENBARRELFEEDBACK |
			TABLET_DISABLE_TOUCHUIFORCEON |
			TABLET_DISABLE_TOUCHUIFORCEOFF |
			TABLET_DISABLE_TOUCHSWITCH |
			TABLET_DISABLE_FLICKS |
			TABLET_DISABLE_SMOOTHSCROLLING |
			TABLET_DISABLE_FLICKFALLBACKKEYS
			));

	if(MenuInit() == 0)
	{
		MessageBox(NULL, "Error creating main menu", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	SetStyle(style);

	DragAcceptFiles(MainWindow->getHWnd(), TRUE);
	EnableMenuItem(mainMenu, IDM_WIFISETTINGS, MF_ENABLED);

	InitCustomKeys(&CustomKeys);
	Hud.reset();

	input_init();

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
	ViewFSNitro = new TOOLSCLASS(hAppInst, IDD_TOOL_FSNITRO, (DLGPROC) ViewFSNitroProc);

	// Slot 1 / Slot 2 (GBA slot)
	cmdline.slot1_fat_dir = GetPrivateProfileStdString("Slot1", "fat_path", "");
	cmdline._slot1_fat_dir_type = GetPrivateProfileBool("Slot1", "fat_path_type", false, IniName);
	if (cmdline._slot1_fat_dir_type != 0 && cmdline._slot1_fat_dir_type != 1)
		cmdline._slot1_fat_dir_type = 0;
	slot1_R4_path_type = cmdline._slot1_fat_dir_type;

	slot1_Init();
	slot2_Init();

	int slot2_device_id = (NDS_SLOT2_TYPE)GetPrivateProfileInt("Slot2", "id", slot2_List[NDS_SLOT2_AUTO]->info()->id(), IniName);
	NDS_SLOT2_TYPE slot2_device_type = NDS_SLOT2_AUTO;
	slot2_getTypeByID(slot2_device_id, slot2_device_type);

	win32_CFlash_cfgMode = GetPrivateProfileInt("Slot2.CFlash", "fileMode", ADDON_CFLASH_MODE_RomPath, IniName);
	win32_CFlash_cfgDirectory = GetPrivateProfileStdString("Slot2.CFlash", "path", "");
	win32_CFlash_cfgFileName = GetPrivateProfileStdString("Slot2.CFlash", "filename", "");
	win32_GBA_cfgRomPath = GetPrivateProfileStdString("Slot2.GBAgame", "filename", "");

	cmdline.process_addonCommands();
	WIN_InstallCFlash();
	WIN_InstallGBACartridge();
	
	slot1_R4_path_type = cmdline._slot1_fat_dir_type;

	if(cmdline.is_cflash_configured)
	{
	    slot2_device_type = NDS_SLOT2_CFLASH;
		//push the commandline-provided options into the current config slots
		if(CFlash_Mode == ADDON_CFLASH_MODE_Path)
			win32_CFlash_cfgDirectory = CFlash_Path;
		else 
			win32_CFlash_cfgFileName = CFlash_Path;
	}
	
	//override slot1 type with commandline, if present
	int slot1_device_id = (NDS_SLOT1_TYPE)GetPrivateProfileInt("Slot1", "id", slot1_List[NDS_SLOT1_RETAIL_AUTO]->info()->id(), IniName);
	NDS_SLOT1_TYPE slot1_device_type = NDS_SLOT1_RETAIL_AUTO;
	slot1_getTypeByID(slot1_device_id, slot1_device_type);

	if(cmdline.slot1 != "")
		WritePrivateProfileInt("Slot1","type",slot1_device_type,IniName);
	else
		slot1_Change((NDS_SLOT1_TYPE)slot1_device_type);
	
	if(cmdline.gbaslot_rom != "")
	{
		slot2_device_type = NDS_SLOT2_GBACART;
		win32_GBA_cfgRomPath = cmdline.gbaslot_rom;
	}

	switch (slot2_device_type)
	{
		case NDS_SLOT2_NONE:
			break;
		case NDS_SLOT2_AUTO:
			break;
		case NDS_SLOT2_CFLASH:
			break;
		case NDS_SLOT2_RUMBLEPAK:
			break;
		case NDS_SLOT2_GBACART:
			if (win32_GBA_cfgRomPath.empty())
			{
				slot2_device_type = NDS_SLOT2_NONE;
				break;
			}
			// TODO: check for file exist
			break;
		case NDS_SLOT2_GUITARGRIP:
			break;
		case NDS_SLOT2_EXPMEMORY:
			break;
		case NDS_SLOT2_EASYPIANO:
			break;
		case NDS_SLOT2_PADDLE:
			break;
		case NDS_SLOT2_PASSME:
			break;
		default:
			slot2_device_type = NDS_SLOT2_NONE;
			break;
	}

	slot2_Change((NDS_SLOT2_TYPE)slot2_device_type);

	Guitar.Enabled	= (slot2_device_type == NDS_SLOT2_GUITARGRIP)?true:false;
	Piano.Enabled	= (slot2_device_type == NDS_SLOT2_EASYPIANO)?true:false;
	Paddle.Enabled	= (slot2_device_type == NDS_SLOT2_PADDLE)?true:false;

	CommonSettings.WifiBridgeDeviceID = GetPrivateProfileInt("Wifi", "BridgeAdapter", 0, IniName);

	osd = new OSDCLASS(-1);

	NDS_Init();

	GPU->SetEventHandler(WinGPUEvent);

	WinPCapInterface *winpcapInterface = (isPCapSupported) ? new WinPCapInterface : NULL;
	wifiHandler->SetPCapInterface(winpcapInterface);
	wifiHandler->SetSocketsSupported(isSocketsSupported);
	wifiHandler->SetBridgeDeviceIndex(CommonSettings.WifiBridgeDeviceID);

	if (GetPrivateProfileBool("Wifi", "Enabled", false, IniName))
	{
		if (GetPrivateProfileBool("Wifi", "Compatibility Mode", false, IniName))
			wifiHandler->SetEmulationLevel(WifiEmulationLevel_Compatibility);
		else
			wifiHandler->SetEmulationLevel(WifiEmulationLevel_Normal);
	}
	else
		wifiHandler->SetEmulationLevel(WifiEmulationLevel_Off);
	
	CommonSettings.GFX3D_Renderer_TextureScalingFactor = (cmdline.texture_upscale != -1) ? cmdline.texture_upscale : GetValid3DIntSetting("TextureScalingFactor", 1, possibleTexScale, 3);
	int newPrescaleHD = (cmdline.gpu_resolution_multiplier != -1) ? cmdline.gpu_resolution_multiplier : GetPrivateProfileInt("3D", "PrescaleHD", 1, IniName);
	video.SetPrescale(newPrescaleHD, 1);
	GPU->SetCustomFramebufferSize(GPU_FRAMEBUFFER_NATIVE_WIDTH*video.prescaleHD, GPU_FRAMEBUFFER_NATIVE_HEIGHT*video.prescaleHD);
	SyncGpuBpp();
	GPU->ClearWithColor(0xFFFFFF);

	SetMinWindowSize();
	ScaleScreen(windowSize, false);
	
#ifdef GDB_STUB
    gdbstub_mutex_init();

	// Activate the GDB stubs. This has to come after the NDS_Init() where the CPUs are set up.
	gdbstub_handle_t arm9_gdb_stub = NULL;
	gdbstub_handle_t arm7_gdb_stub = NULL;
	
	if (cmdline.arm9_gdb_port > 0)
	{
		arm9_gdb_stub = createStub_gdb(cmdline.arm9_gdb_port, &NDS_ARM9, &arm9_direct_memory_iface);
		if (arm9_gdb_stub == NULL)
		{
			MessageBox(MainWindow->getHWnd(), "Failed to create ARM9 gdbstub", "Error", MB_OK);
			return -1;
		}
		else
		{
			activateStub_gdb(arm9_gdb_stub);
		}
	}
	
	if (cmdline.arm7_gdb_port > 0)
	{
		arm7_gdb_stub = createStub_gdb(cmdline.arm7_gdb_port, &NDS_ARM7, &arm7_base_memory_iface);
		if (arm7_gdb_stub == NULL)
		{
			MessageBox(MainWindow->getHWnd(), "Failed to create ARM7 gdbstub", "Error", MB_OK);
			return -1;
		}
		else
		{
			activateStub_gdb(arm7_gdb_stub);
		}
	}
#endif

	osd->singleScreen = (video.layout == 2);
	
	GetPrivateProfileString("General", "Language", "0", text, 80, IniName);	

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
	if(cur3DCore == RENDERID_NULL_SAVED)
		cur3DCore = RENDERID_NULL;
	else if(cur3DCore == RENDERID_NULL) // this value shouldn't be saved anymore
		cur3DCore = GPU3D_DEFAULT;

	if(cmdline.render3d == COMMANDLINE_RENDER3D_NONE) cur3DCore = RENDERID_NULL;
	if(cmdline.render3d == COMMANDLINE_RENDER3D_SW) cur3DCore = GPU3D_SWRAST;
	if(cmdline.render3d == COMMANDLINE_RENDER3D_OLDGL) cur3DCore = GPU3D_OPENGL_OLD;
	if(cmdline.render3d == COMMANDLINE_RENDER3D_GL) cur3DCore = GPU3D_OPENGL_3_2; //no way of forcing it, at least not right now. I dont care.
	if(cmdline.render3d == COMMANDLINE_RENDER3D_AUTOGL) cur3DCore = GPU3D_OPENGL_3_2; //this will fallback i guess

	CommonSettings.GFX3D_HighResolutionInterpolateColor = GetPrivateProfileBool("3D", "HighResolutionInterpolateColor", 1, IniName);
	CommonSettings.GFX3D_EdgeMark = GetPrivateProfileBool("3D", "EnableEdgeMark", 1, IniName);
	CommonSettings.GFX3D_Fog = GetPrivateProfileBool("3D", "EnableFog", 1, IniName);
	CommonSettings.GFX3D_Texture = GetPrivateProfileBool("3D", "EnableTexture", 1, IniName);
	CommonSettings.GFX3D_LineHack = GetPrivateProfileBool("3D", "EnableLineHack", 1, IniName);
	CommonSettings.GFX3D_TXTHack = GetPrivateProfileBool("3D", "EnableTXTHack", 0, IniName); // Default is off.
	CommonSettings.OpenGL_Emulation_ShadowPolygon = GetPrivateProfileBool("3D", "EnableShadowPolygon", 1, IniName);
	CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = GetPrivateProfileBool("3D", "EnableSpecialZeroAlphaBlending", 1, IniName);
	CommonSettings.OpenGL_Emulation_NDSDepthCalculation = GetPrivateProfileBool("3D", "EnableNDSDepthCalculation", 1, IniName);
	CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = GetPrivateProfileBool("3D", "EnableDepthLEqualPolygonFacing", 0, IniName); // Default is off.
	CommonSettings.GFX3D_Renderer_MultisampleSize = GetValid3DIntSetting("MultisampleSize", 0, possibleMSAA, 6);
	Change3DCoreWithFallbackAndSave(cur3DCore);


#ifdef BETA_VERSION
	EnableMenuItem (mainMenu, IDM_SUBMITBUGREPORT, MF_GRAYED);
#endif
	LOG("Init sound core\n");
	sndcoretype = (cmdline.disable_sound == 1) ? SNDCORE_DUMMY : GetPrivateProfileInt("Sound","SoundCore2", SNDCORE_DIRECTX, IniName);
	sndbuffersize = GetPrivateProfileInt("Sound","SoundBufferSize2", DESMUME_SAMPLE_RATE*8/60, IniName);
	CommonSettings.spuInterpolationMode = (SPUInterpolationMode)GetPrivateProfileInt("Sound","SPUInterpolation", 2, IniName);

	if (cmdline._spu_sync_mode == -1)
		CommonSettings.SPU_sync_mode = GetPrivateProfileInt("Sound","SynchMode",1,IniName);
	if (cmdline._spu_sync_method == -1)
		CommonSettings.SPU_sync_method = GetPrivateProfileInt("Sound","SynchMethod",0,IniName);
	
	EnterCriticalSection(&win_execute_sync);
	int spu_ret = SPU_ChangeSoundCore(sndcoretype, sndbuffersize);
	LeaveCriticalSection(&win_execute_sync);
	if(spu_ret != 0)
	{
		MessageBox(MainWindow->getHWnd(),"Unable to initialize DirectSound","Error",MB_OK);
		sndcoretype = 0;
	}

	sndvolume = GetPrivateProfileInt("Sound","Volume",100, IniName);
	SPU_SetVolume(sndvolume);
	
	CommonSettings.DebugConsole = GetPrivateProfileBool("Emulation", "DebugConsole", false, IniName);
	CommonSettings.EnsataEmulation = GetPrivateProfileBool("Emulation", "EnsataEmulation", false, IniName);
	CommonSettings.UseExtBIOS = GetPrivateProfileBool("BIOS", "UseExtBIOS", false, IniName);
	GetPrivateProfileString("BIOS", "ARM9BIOSFile", "bios9.bin", CommonSettings.ARM9BIOS, 256, IniName);
	GetPrivateProfileString("BIOS", "ARM7BIOSFile", "bios7.bin", CommonSettings.ARM7BIOS, 256, IniName);
	CommonSettings.SWIFromBIOS = GetPrivateProfileBool("BIOS", "SWIFromBIOS", false, IniName);
	CommonSettings.PatchSWI3 = GetPrivateProfileBool("BIOS", "PatchSWI3", false, IniName);

	CommonSettings.UseExtFirmware = GetPrivateProfileBool("Firmware", "UseExtFirmware", false, IniName);
	CommonSettings.UseExtFirmwareSettings = GetPrivateProfileBool("Firmware", "UseExtFirmwareSettings", false, IniName);
	GetPrivateProfileString("Firmware", "FirmwareFile", "firmware.bin", CommonSettings.ExtFirmwarePath, 256, IniName);
	CommonSettings.BootFromFirmware = GetPrivateProfileBool("Firmware", "BootFromFirmware", false, IniName);

	video.setfilter(GetPrivateProfileInt("Video", "Filter", video.NONE, IniName));
	FilterUpdate(MainWindow->getHWnd(),false);

	//default the firmware settings, they may get changed later
	NDS_GetDefaultFirmwareConfig(CommonSettings.fwConfig);
	
	// Generate the unique MAC address.
	{
		// Get the host's IP4 address.
		char hostname[256];
		if (gethostname(hostname, 256) != 0)
			strncpy(hostname, "127.0.0.1", 256);
		
		hostent *he = gethostbyname(hostname);
		u32 ipaddr;
		if (he == NULL || he->h_addr_list[0] == NULL)
			ipaddr = 0x0100007F; // 127.0.0.1
		else
			ipaddr = *(u32 *)he->h_addr_list[0];
		
		u32 hash = (u32)GetCurrentProcessId();
		
		while ((hash & 0xFF000000) == 0)
		{
			hash <<= 1;
		}
		
		hash >>= 1;
		hash += ipaddr >> 8;
		hash &= 0x00FFFFFF;
		
		CommonSettings.fwConfig.MACAddress[0] = 0x00;
		CommonSettings.fwConfig.MACAddress[1] = 0x09;
		CommonSettings.fwConfig.MACAddress[2] = 0xBF;
		CommonSettings.fwConfig.MACAddress[3] = hash >> 16;
		CommonSettings.fwConfig.MACAddress[4] = (hash >> 8) & 0xFF;
		CommonSettings.fwConfig.MACAddress[5] = hash & 0xFF;
	}
	
	// Read the firmware settings from the init file
	CommonSettings.fwConfig.favoriteColor = GetPrivateProfileInt("Firmware","favColor", 10, IniName);
	CommonSettings.fwConfig.birthdayMonth = GetPrivateProfileInt("Firmware","bMonth", 7, IniName);
	CommonSettings.fwConfig.birthdayDay = GetPrivateProfileInt("Firmware","bDay", 15, IniName);
	CommonSettings.fwConfig.language = GetPrivateProfileInt("Firmware","Language", 1, IniName);

	{
		/*
		* Read in the nickname and message.
		* Convert the strings into Unicode UTF-16 characters.
		*/
		char temp_str[27];
		int char_index;
		GetPrivateProfileString("Firmware","nickName", "yopyop", temp_str, 11, IniName);
		CommonSettings.fwConfig.nicknameLength = strlen( temp_str);

		if (CommonSettings.fwConfig.nicknameLength == 0) {
			strcpy( temp_str, "yopyop");
			CommonSettings.fwConfig.nicknameLength = strlen( temp_str);
		}

		for ( char_index = 0; char_index < CommonSettings.fwConfig.nicknameLength; char_index++) {
			CommonSettings.fwConfig.nickname[char_index] = temp_str[char_index];
		}

		GetPrivateProfileString("Firmware","Message", "DeSmuME makes you happy!", temp_str, 27, IniName);
		CommonSettings.fwConfig.messageLength = strlen( temp_str);
		for ( char_index = 0; char_index < CommonSettings.fwConfig.messageLength; char_index++) {
			CommonSettings.fwConfig.message[char_index] = temp_str[char_index];
		}
	}

	if (cmdline.nds_file != "")
	{
		if(OpenCoreSystemCP(cmdline.nds_file.c_str()))
		{
			romloaded = TRUE;
		}
	}

	//not supported; use the GUI
	//if(cmdline.language != -1) CommonSettings.fwConfig.language = cmdline.language;

	cmdline.process_movieCommands();
	
	if(cmdline.load_slot != -1)
	{
		int load = cmdline.load_slot;
		if(load==10) load=0;
		HK_StateLoadSlot(load, true);
	}

	MainWindow->Show(SW_NORMAL);

	if(cmdline.windowed_fullscreen)
		ToggleFullscreen();

	//DEBUG TEST HACK
	//driver->VIEW3D_Init();
	//driver->view3d->Launch();
	//---------
	
	//------DO EVERYTHING
	run();


	//------SHUTDOWN
	input_deinit();

	KillDisplay();

	DRV_AviEnd();
	WAV_End();

#ifdef GDB_STUB
	destroyStub_gdb(arm9_gdb_stub);
	arm9_gdb_stub = NULL;
	
	destroyStub_gdb(arm7_gdb_stub);
	arm7_gdb_stub = NULL;

    gdbstub_mutex_destroy();
#endif
	
	if (wifiHandler->IsSocketsSupported())
	{
		WSACleanup();
	}

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
	if (ViewFSNitro!=NULL) delete ViewFSNitro;
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

	ddraw.release();

	UnregWndClass("DeSmuME");

	return 0;
}

int WINAPI WinMain (HINSTANCE hThisInstance,
					HINSTANCE hPrevInstance,
					LPSTR lpszArgument,
					int nFunsterStil)

{
//	testelf();

    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);

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

	hAppInst=hThisInstance;

	GetINIPath();

	#if !defined(PUBLIC_RELEASE) || defined(DEVELOPER)
		static const bool defaultConsoleEnable = true;
	#else
		static const bool defaultConsoleEnable = false;
	#endif

	gShowConsole = GetPrivateProfileBool("Console", "Show", defaultConsoleEnable, IniName);
	gConsoleTopmost = GetPrivateProfileBool("Console", "Always On Top", false, IniName);

	if (gShowConsole)
	{
		OpenConsole();			// Init debug console
		ConsoleAlwaysTop(gConsoleTopmost);
	}

	//--------------------------------
	int ret = _main();
	//--------------------------------

	printf("returning from main\n");

	timeEndPeriod (wmTimerRes);

	//dump any console input to keep the shell from processing it after desmume exits.
	//(we would be unique in doing this, as every other program has this "problem", but I am saving the code for future reference)
	//for(;;)
	//{
	//  INPUT_RECORD ir;
	//  DWORD nEventsRead;
	// BOOL ret = PeekConsoleInput(GetStdHandle(STD_INPUT_HANDLE),&ir,1,&nEventsRead);
	//  printf("%d %d %d\n",ir.Event.KeyEvent.uChar,nEventsRead,ret);
	//  if(nEventsRead==0) break;
	//  ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE),&ir,1,&nEventsRead);
	//}

	CloseConsole();

	return ret;
}

// Checks for incorrect values, updates ini and returns a valid value. Requires an array to check.
int GetValid3DIntSetting(char *settingName, const int defVal, const int arrName[], const int arrSize)
{
	const int curVal = GetPrivateProfileInt("3D", settingName, defVal, IniName);

	for (int i = 0; i <= arrSize; i++)
	{
		if (curVal == arrName[i])
		{
			return curVal;
		}
	}
	// Sets to defaults if ini value is incorrect.
	WritePrivateProfileInt("3D", settingName, defVal, IniName);
	return defVal;
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
		ScaleScreen(windowSize, false);
	}
	else if(ForceRatio)
	{
		RECT rc;
		GetWindowRect(MainWindow->getHWnd(), &rc);
		SendMessage(MainWindow->getHWnd(), WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&rc); // calls WINCLASS::sizingMsg
		MoveWindow(MainWindow->getHWnd(), rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
	}
}

void SetScreenGap(int gap)
{
	HWND hwnd = MainWindow->getHWnd();
	u32 style = GetStyle();
	bool maximized = IsZoomed(hwnd) || fsWindow;
	if (maximized)
		RestoreWindow(hwnd);
	
	RECT clientRect;
	GetClientRect(MainWindow->getHWnd(), &clientRect);
	RECT rc;
	GetWindowRect(MainWindow->getHWnd(), &rc);
	if (video.rotation == 0 || video.rotation == 180)
		rc.bottom += (gap - video.screengap) * (clientRect.right - clientRect.left) / GPU_FRAMEBUFFER_NATIVE_WIDTH;
	else
		rc.right += (gap - video.screengap) * (clientRect.bottom - clientRect.top - MainWindowToolbar->GetHeight()) / GPU_FRAMEBUFFER_NATIVE_WIDTH;
	video.screengap = gap;
	MoveWindow(MainWindow->getHWnd(), rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

	SetMinWindowSize();
	FixAspectRatio();
	UpdateWndRects(MainWindow->getHWnd());

	if (maximized)
	{
		if (style & DWS_FULLSCREEN)
			ShowFullScreen(hwnd);
		else
			ShowWindow(hwnd, SW_MAXIMIZE);
	}
}

//========================================================================================
void SetRotate(HWND hwnd, int rot, bool user)
{
	if (video.layout != 0) return;

	u32 style = GetStyle();
	bool maximized = IsZoomed(hwnd) || fsWindow;

	if(((rot == 90) || (rot == 270)) == ((video.rotation == 90) || (video.rotation == 270)))
		maximized = false; // no need to toggle out to windowed if the dimensions aren't changing
	if (maximized)
		RestoreWindow(hwnd);
	{
	Lock lock (win_backbuffer_sync);

	RECT rc;
	int oldrot = video.rotation;
	int oldheight, oldwidth;
	int newheight, newwidth;

	video.rotation = rot;

	GetClientRect(hwnd, &rc);
	oldwidth = (rc.right - rc.left);
	oldheight = (rc.bottom - rc.top) - MainWindowToolbar->GetHeight();
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

	int cwid, ccwid;
	switch (rot)
	{
		case 0: cwid = IDC_ROTATE90; ccwid = IDC_ROTATE270; break;
		case 90: cwid = IDC_ROTATE180; ccwid = IDC_ROTATE0; break;
		case 180: cwid = IDC_ROTATE270; ccwid = IDC_ROTATE90; break;
		case 270: cwid = IDC_ROTATE0; ccwid = IDC_ROTATE180; break;
	}

	MainWindowToolbar->ChangeButtonID(4, ccwid);
	MainWindowToolbar->ChangeButtonID(5, cwid);

	WritePrivateProfileInt("Video","Window Rotate",video.rotation,IniName);
	if(user)
	{
		video.rotation_userset = video.rotation;
		WritePrivateProfileInt("Video","Window Rotate Set",video.rotation_userset,IniName);
	}

	UpdateScreenRects();
	UpdateWndRects(hwnd);
	}
	if (maximized)
	{
		if (style & DWS_FULLSCREEN)
			ShowFullScreen(hwnd);
		else
			ShowWindow(hwnd, SW_MAXIMIZE);
	}
}

void AviEnd()
{
	NDS_Pause();

	DRV_AviEnd();
	LOG("AVI recording ended.");
	driver->AddLine("AVI recording ended.");

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
	char outFilename[MAX_PATH] = "";
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = MainWindow->getHWnd();
	ofn.lpstrFilter = "AVI Files (*.avi)\0*.avi\0\0";
	ofn.lpstrDefExt = "avi";
	ofn.lpstrTitle = "Save AVI as";

	std::string dir = path.getpath(path.AVI_FILES);
	ofn.lpstrInitialDir = dir.c_str();
	path.formatname(outFilename);
	ofn.lpstrFile = outFilename;

	int len = strlen(outFilename);
	if(len + dir.length() > MAX_PATH - 4)
		outFilename[MAX_PATH - dir.length() - 4] = '\0';
	strcat(outFilename, ".avi");

	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

	if (GetSaveFileName(&ofn))
	{
		if (AVI_IsRecording())
		{
			DRV_AviEnd();
			LOG("AVI recording ended.");
			driver->AddLine("AVI recording ended.");
		}

		bool result = DRV_AviBegin(outFilename);
		if (result)
		{
			LOG("AVI recording started.");
			driver->AddLine("AVI recording started.");
		}

		dir = Path::GetFileDirectoryPath(outFilename);
		path.setpath(path.AVI_FILES, dir);
		WritePrivateProfileString(SECTION, AVIKEY, dir.c_str(), IniName);
	}

	NDS_UnPause();
}

void WavEnd()
{
	NDS_Pause();
	WAV_End();
	NDS_UnPause();
}

void UpdateToolWindows()
{
	Update_RAM_Search();	//Update_RAM_Watch() is also called; hotkey.cpp - HK_StateLoadSlot & State_Load also call these functions

	if(SoundView_IsOpened()) SoundView_Refresh();
	RefreshAllToolWindows();
	mainLoopData.toolframecount = 0;
}

//Shows an Open File menu and starts recording an WAV
void WavRecordTo(int wavmode)
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

	//	strcat(szChoice, ".wav");
	//}
	//else
	//{
	//	extern char FileBase[];
	//	sprintf(szChoice, "%s.wav", FileBase);
	//}

	// wav record file browser
	char outFilename[MAX_PATH] = "";
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = MainWindow->getHWnd();
	ofn.lpstrFilter = "WAV Files (*.wav)\0*.wav\0\0";
	ofn.lpstrDefExt = "wav";
	ofn.lpstrTitle = "Save WAV as";

	std::string dir = path.getpath(path.AVI_FILES);
	ofn.lpstrInitialDir = dir.c_str();
	path.formatname(outFilename);
	ofn.lpstrFile = outFilename;

	int len = strlen(outFilename);
	if (len + dir.length() > MAX_PATH - 4)
		outFilename[MAX_PATH - dir.length() - 4] = '\0';
	strcat(outFilename, ".wav");

	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

	if(GetSaveFileName(&ofn))
	{
		WAV_Begin(outFilename, (WAVMode)wavmode);

		dir = Path::GetFileDirectoryPath(outFilename);
		path.setpath(path.AVI_FILES, dir);
		WritePrivateProfileString(SECTION, AVIKEY, dir.c_str(), IniName);
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

	if(LoadROM(filename, PhysicalName, LogicalName))
	{
		romloaded = TRUE;
		if(movieMode == MOVIEMODE_INACTIVE)
		{
			if(!start_paused)
				Unpause();
			start_paused = 0;
		}

		if(autoLoadLua)
		{
			string luaScript;
			luaScript.append(path.pathToLua);
			if(!Path::IsPathRooted(luaScript))
			{
				luaScript.clear();
				luaScript.append(path.pathToModule);
				luaScript.append(path.pathToLua);
			}
			luaScript.append("\\");
			luaScript.append(path.GetRomNameWithoutExtension());
			luaScript.append(".lua");

			FILE* file;
			file = fopen(luaScript.c_str(), "rb");
			if(file)
			{
				fclose(file);
				HWND hDlg = CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_LUA), MainWindow->getHWnd(), (DLGPROC) LuaScriptProc);
				SendDlgItemMessage(hDlg, IDC_EDIT_LUAPATH, WM_SETTEXT, (WPARAM) 512, (LPARAM) luaScript.c_str());
			}
		}

		// Update the toolbar
		MainWindowToolbar->EnableButton(IDM_PAUSE, true);
		MainWindowToolbar->EnableButton(IDM_CLOSEROM, true);
		MainWindowToolbar->EnableButton(IDM_RESET, true);
		MainWindowToolbar->ChangeButtonBitmap(IDM_PAUSE, IDB_PAUSE);

		// Warn the user if the battery save won't be written to an actual file on disk.
		char batteryPath[MAX_PATH] = {0};
		memset(batteryPath, 0, MAX_PATH);
		path.getpathnoext(path.BATTERY, batteryPath);
		std::string batteryPathString = std::string(batteryPath) + ".dsv";
		FILE *testFs = fopen(batteryPathString.c_str(), "rb+");
		if (testFs == NULL)
		{
			msgbox->warn("\
Could not get read/write access to the battery save file! The file will not be saved in this current session.\n\n\
Choose Config > Path Settings and ensure that the SaveRam directory exists and is available for read/write access.");
		}
		else
		{
			fclose(testFs);
		}
		
		return TRUE;
	}
	else return FALSE;
}

LRESULT OpenFile()
{
	HWND hwnd = MainWindow->getHWnd();

	int filterSize = 0, i = 0;
	OPENFILENAMEW ofn;
	wchar_t filename[MAX_PATH] = L"",
		fileFilter[512]=L"";
	NDS_Pause(); //Stop emulation while opening new rom

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;

	ofn.lpstrFilter = 
		L"All Usable Files (*.nds, *.ds.gba, *.srl, *.zip, *.7z, *.rar, *.gz)\0*.nds;*.ds.gba;*.srl;*.zip;*.7z;*.rar;*.gz\0"
		"NDS ROM file (*.nds,*.srl)\0*.nds;*.srl\0"
		"NDS/GBA ROM File (*.ds.gba)\0*.ds.gba\0"
		"Zipped NDS ROM file (*.zip)\0*.zip\0"
		"7Zipped NDS ROM file (*.7z)\0*.7z\0"
		"RARed NDS ROM file (*.rar)\0*.rar\0"
		"GZipped NDS ROM file (*.gz)\0*.gz\0"
		"Any file (*.*)\0*.*\0"
		"\0"
		; //gzip doesnt actually work right now

	ofn.nFilterIndex = 1;
	ofn.lpstrFile =  filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = L"nds";
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	std::wstring dir = mbstowcs(path.getpath(path.ROMS));
	ofn.lpstrInitialDir = dir.c_str();

	if (GetOpenFileNameW(&ofn) == NULL)
	{
		NDS_UnPause();
		return 0;
	}
	else
	{
		if(path.savelastromvisit)
		{
			std::string dir = Path::GetFileDirectoryPath(wcstombs(filename));
			path.setpath(path.ROMS, dir);
			WritePrivateProfileString(SECTION, ROMKEY, dir.c_str(), IniName);
		}
	}

	if(!OpenCore(wcstombs((std::wstring)filename).c_str()))
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
	romloaded = false;
	execute = false;
	Hud.resetTransient();
	NDS_FreeROM();

	// clear screen so the last frame we rendered doesn't stick around
	// (TODO: maybe NDS_Reset should do this?)
	GPU->ClearWithColor(0xFFFF);

	InvalidateRect(MainWindow->getHWnd(), NULL, TRUE); // make sure the window refreshes with the cleared screen

	MainWindowToolbar->EnableButton(IDM_PAUSE, false);
	MainWindowToolbar->EnableButton(IDM_CLOSEROM, false);
	MainWindowToolbar->EnableButton(IDM_RESET, false);
	MainWindowToolbar->ChangeButtonBitmap(IDM_PAUSE, IDB_PLAY);
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

	int bakedModifiers = (key >> 16) & (CUSTKEY_ALT_MASK | CUSTKEY_CTRL_MASK | CUSTKEY_SHIFT_MASK | CUSTKEY_NONE_MASK);
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
	return key & ~((CUSTKEY_ALT_MASK | CUSTKEY_CTRL_MASK | CUSTKEY_SHIFT_MASK | CUSTKEY_NONE_MASK) << 16);
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
		if(romloaded && ((wParam == VK_MENU || wParam == VK_F10) && (hitHotKey)))
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

void ScreenshotToClipboard(bool extraInfo)
{
	const char* nameandver = EMU_DESMUME_NAME_AND_VERSION();
	bool twolinever = strlen(nameandver) > 32;

	HFONT hFont = CreateFont(14, 8, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "Lucida Console");

	int exHeight = 0;
	if(extraInfo)
	{
		exHeight = (14 * (twolinever ? 8:7));
	}

	const NDSDisplayInfo& dispInfo = GPU->GetDisplayInfo();

	int width = dispInfo.customWidth;
	int height = dispInfo.customHeight*2;

	HDC hScreenDC = GetDC(NULL);
	HDC hMemDC = CreateCompatibleDC(hScreenDC);
	HBITMAP hMemBitmap = CreateCompatibleBitmap(hScreenDC, width, height + exHeight);
	HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hMemBitmap);
	HFONT hOldFont = (HFONT)SelectObject(hMemDC, hFont);


	RECT rc; SetRect(&rc, 0, 0, width, height + exHeight);

	FillRect(hMemDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	BITMAPV4HEADER bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bV4Size = sizeof(bmi);
	bmi.bV4Planes = 1;
	bmi.bV4BitCount = 32;
	bmi.bV4V4Compression = BI_RGB;
	bmi.bV4Width = width;
	bmi.bV4Height = -height;
	if(gpu_bpp == 15)
	{
		bmi.bV4Size = sizeof(bmi);
		bmi.bV4Planes = 1;
		bmi.bV4BitCount = 16;
		bmi.bV4V4Compression = BI_RGB | BI_BITFIELDS;
		bmi.bV4RedMask = 0x001F;
		bmi.bV4GreenMask = 0x03E0;
		bmi.bV4BlueMask = 0x7C00;
		bmi.bV4Width = width;
		bmi.bV4Height = -height;

		SetDIBitsToDevice(hMemDC, 0, 0, width, height, 0, 0, 0, height, dispInfo.masterCustomBuffer, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
	}
	else
	{
		u32* swapbuf = (u32*)malloc_alignedPage(width*height * 4);
		ColorspaceConvertBuffer888XTo8888Opaque<true, false>((const u32*)dispInfo.masterCustomBuffer, swapbuf, width * height);

		SetDIBitsToDevice(hMemDC, 0, 0, width, height, 0, 0, 0, height, swapbuf, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

		free_aligned(swapbuf);
	}

	//center-justify the extra text
	int xo = (width - 256)/2;
	
	if(extraInfo)
	{
		SetBkColor(hMemDC, RGB(255, 255, 255));
		SetTextColor(hMemDC, RGB(64, 64, 130));

		if (twolinever)
		{
			int i;
			for (i = 31; i > 0; i--)
				if (nameandver[i] == ' ')
					break;

			TextOut(hMemDC, xo + 0, height + 14, &nameandver[0], i+1);
			TextOut(hMemDC, xo + 8, height + 14*2, &nameandver[i+1], strlen(nameandver) - (i+1));
		}
		else
			TextOut(hMemDC, xo + 0, height + 14, nameandver, strlen(nameandver));

		char str[64] = {0};
		TextOut(hMemDC, xo + 8, height + 14 * (twolinever ? 3:2), gameInfo.ROMname, strlen(gameInfo.ROMname));
		TextOut(hMemDC, xo + 8, height + 14 * (twolinever ? 4:3), gameInfo.ROMserial, strlen(gameInfo.ROMserial));
		

		sprintf(str, "CPU: %s", CommonSettings.use_jit ? "JIT":"Interpreter");
		TextOut(hMemDC, xo + 8, height + 14 * (twolinever ? 5:4), str, strlen(str));

		sprintf(str, "FPS: %i/%i (%02d%%/%02d%%) | %s", mainLoopData.fps, mainLoopData.fps3d, Hud.cpuload[0], Hud.cpuload[1], paused ? "Paused":"Running");
		TextOut(hMemDC, xo + 8, height + 14 * (twolinever ? 6:5), str, strlen(str));

		sprintf(str, "3D %s (%d BPP)", core3DList[cur3DCore]->name, gpu_bpp);
		TextOut(hMemDC, xo + 8, height + 14 * (twolinever ? 7:6), str, strlen(str));
	}

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
		DialogBoxW(hAppInst,MAKEINTRESOURCEW(IDD_WIFISETTINGS), hwnd, (DLGPROC) WifiSettingsDlgProc);
		break;
	}

	if (tpaused)
		NDS_UnPause();
}

void FilterUpdate(HWND hwnd, bool user)
{
	u32 style = GetStyle();
	bool maximized = IsZoomed(hwnd) || fsWindow;
	if (maximized)
		RestoreWindow(hwnd);

	UpdateScreenRects();
	UpdateWndRects(hwnd);
	SetScreenGap(video.screengap);
	SetRotate(hwnd, video.rotation, false);
	if(user && windowSize==0) {}
	else ScaleScreen(windowSize, false);
	if (romloaded)
		Display();

	WritePrivateProfileInt("Video", "Filter", video.currentfilter, IniName);
	WritePrivateProfileInt("Video", "Width", video.width, IniName);
	WritePrivateProfileInt("Video", "Height", video.height, IniName);

	if (maximized)
	{
		if (style & DWS_FULLSCREEN)
			ShowFullScreen(hwnd);
		else
			ShowWindow(hwnd, SW_MAXIMIZE);
	}
}

void SaveWindowSize(HWND hwnd)
{
	//dont save if window was maximized
	if(IsZoomed(hwnd) || fsWindow) return;
	RECT rc;
	GetClientRect(hwnd, &rc);
	rc.top += MainWindowToolbar->GetHeight();
	WritePrivateProfileInt("Video", "Window width", (rc.right - rc.left), IniName);
	WritePrivateProfileInt("Video", "Window height", (rc.bottom - rc.top), IniName);
}

void SaveWindowPos(HWND hwnd)
{
	//dont save if window was maximized
	if(IsZoomed(hwnd) || fsWindow) return;
	WritePrivateProfileInt("Video", "WindowPosX", WndX/*MainWindowRect.left*/, IniName);
	WritePrivateProfileInt("Video", "WindowPosY", WndY/*MainWindowRect.top*/, IniName);
}

static void ShowToolWindow(HWND hwnd)
{
	ShowWindow(hwnd,SW_NORMAL);
	SetForegroundWindow(hwnd);
}

//========================================================================================
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
	static int tmp_execute;
	static UINT_PTR autoHideCursorTimer = 0;
	static bool mouseLeftClientArea = true;

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(tme);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hwnd;

	switch (message)                  // handle the messages
	{
		case WM_INITMENU:
			return 0;

		case WM_EXITMENULOOP:
			SPU_Pause(0);
			if (autoframeskipenab && frameskiprate) AutoFrameSkip_IgnorePreviousDelay();
			if(MainWindowToolbar->Visible())
				UpdateWindow(MainWindowToolbar->GetHWnd());
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
			DesEnableMenuItem(mainMenu, IDM_RESET,             romloaded && !(movieMode == MOVIEMODE_PLAY && movie_readonly));
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

			DesEnableMenuItem(mainMenu, IDC_BACKGROUNDINPUT,   !lostFocusPause);

			DesEnableMenuItem(mainMenu, ID_LOADTORAM,			!romloaded);
			DesEnableMenuItem(mainMenu, ID_STREAMFROMDISK,		!romloaded);

			//Update savestate slot items based on ROM loaded
			for (int x = 0; x < 10; x++)
			{
				DesEnableMenuItem(mainMenu, IDM_STATE_SAVE_F10+x,   romloaded);
				DesEnableMenuItem(mainMenu, IDM_STATE_LOAD_F10+x,   romloaded);
			}

			//Gray the recent ROM menu item if there are no recent ROMs
			DesEnableMenuItem(mainMenu, ID_FILE_RECENTROM, RecentRoms.size()>0);

			DesEnableMenuItem(mainMenu, ID_TOOLS_VIEWFSNITRO,     romloaded);

			//emulation menu
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
			MainWindow->checkMenu(IDC_VIEW_PADTOINTEGER, ((PadToInteger)));
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

			//Screen Size Ratio
			MainWindow->checkMenu(IDC_SCR_RATIO_1p0, ((int)(screenSizeRatio*10)==10));
			MainWindow->checkMenu(IDC_SCR_RATIO_1p1, ((int)(screenSizeRatio*10)==11));
			MainWindow->checkMenu(IDC_SCR_RATIO_1p2, ((int)(screenSizeRatio*10)==12));
			MainWindow->checkMenu(IDC_SCR_RATIO_1p3, ((int)(screenSizeRatio*10)==13));
			MainWindow->checkMenu(IDC_SCR_RATIO_1p4, ((int)(screenSizeRatio*10)==14));
			MainWindow->checkMenu(IDC_SCR_RATIO_1p5, ((int)(screenSizeRatio*10)==15));
			MainWindow->checkMenu(IDC_SCR_VCENTER, (vCenterResizedScr == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_RATIO_1p0, (video.layout == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_RATIO_1p1, (video.layout == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_RATIO_1p2, (video.layout == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_RATIO_1p3, (video.layout == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_RATIO_1p4, (video.layout == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_RATIO_1p5, (video.layout == 1));
			DesEnableMenuItem(mainMenu, IDC_SCR_VCENTER, (video.layout == 1));

			//Screen Separation
			MainWindow->checkMenu(IDM_SCREENSEP_NONE,   ((video.screengap==kGapNone)));
			MainWindow->checkMenu(IDM_SCREENSEP_BORDER, ((video.screengap==kGapBorder)));
			MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP, ((video.screengap==kGapNDS)));
			MainWindow->checkMenu(IDM_SCREENSEP_NDSGAP2, ((video.screengap==kGapNDS2)));
			MainWindow->checkMenu(IDM_SCREENSEP_DRAGEDIT, (SeparationBorderDrag));
			MainWindow->checkMenu(IDM_SCREENSEP_COLORWHITE, ((ScreenGapColor&0xFFFFFF) == 0xFFFFFF));
			MainWindow->checkMenu(IDM_SCREENSEP_COLORGRAY, ((ScreenGapColor&0xFFFFFF) == 0x7F7F7F));
			MainWindow->checkMenu(IDM_SCREENSEP_COLORBLACK, ((ScreenGapColor&0xFFFFFF) == 0x000000));
			DesEnableMenuItem(mainMenu, IDM_SCREENSEP_DRAGEDIT, !IsZoomed(hwnd) && video.layout==0);

			// Show toolbar
			MainWindow->checkMenu(IDM_SHOWTOOLBAR, MainWindowToolbar->Visible());
	
			// Show Menu In Fullscreen Mode
			MainWindow->checkMenu(IDM_FS_MENU, (GetStyle()&DWS_FS_MENU) != 0);

			// Non-exclusive Fullscreen Mode
			MainWindow->checkMenu(IDM_FS_WINDOW, (GetStyle()&DWS_FS_WINDOW) != 0);

			// Auto-Hide Cursor In Fullscreen Mode
			MainWindow->checkMenu(IDM_FS_HIDE_CURSOR, autoHideCursor);

			//Counters / Etc.
			MainWindow->checkMenu(ID_VIEW_FRAMECOUNTER,CommonSettings.hud.FrameCounterDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYFPS,CommonSettings.hud.FpsDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYINPUT,CommonSettings.hud.ShowInputDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYGRAPHICALINPUT,CommonSettings.hud.ShowGraphicalInputDisplay);
			MainWindow->checkMenu(ID_VIEW_DISPLAYLAG,CommonSettings.hud.ShowLagFrameCounter);
			MainWindow->checkMenu(ID_VIEW_DISPLAYMICROPHONE,CommonSettings.hud.ShowMicrophone);
			MainWindow->checkMenu(ID_VIEW_DISPLAYRTC,CommonSettings.hud.ShowRTC);
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
			MainWindow->checkMenu(IDM_RENDER_HQ4X, video.currentfilter == video.HQ4X );
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

			MainWindow->checkMenu(IDM_RENDER_2XBRZ, video.currentfilter == video._2XBRZ );
			MainWindow->checkMenu(IDM_RENDER_3XBRZ, video.currentfilter == video._3XBRZ );
			MainWindow->checkMenu(IDM_RENDER_4XBRZ, video.currentfilter == video._4XBRZ );
			MainWindow->checkMenu(IDM_RENDER_5XBRZ, video.currentfilter == video._5XBRZ );

			MainWindow->checkMenu(ID_DISPLAYMETHOD_VSYNC, (GetStyle()&DWS_VSYNC)!=0);
			MainWindow->checkMenu(ID_DISPLAYMETHOD_DIRECTDRAWHW, displayMethod == DISPMETHOD_DDRAW_HW);
			MainWindow->checkMenu(ID_DISPLAYMETHOD_DIRECTDRAWSW, displayMethod == DISPMETHOD_DDRAW_SW);
			MainWindow->checkMenu(ID_DISPLAYMETHOD_OPENGL, displayMethod == DISPMETHOD_OPENGL);
			MainWindow->checkMenu(ID_DISPLAYMETHOD_FILTER, gldisplay.filter);

			MainWindow->checkMenu(IDC_BACKGROUNDPAUSE, lostFocusPause);
			MainWindow->checkMenu(IDC_BACKGROUNDINPUT, allowBackgroundInput);

			MainWindow->checkMenu(IDM_CHEATS_DISABLE, CommonSettings.cheatsDisable == true);

			//Save type
			for(int i=0;i<MAX_SAVE_TYPES;i++)
				MainWindow->checkMenu(IDC_SAVETYPE+i, false);

			MainWindow->checkMenu(IDC_SAVETYPE+CommonSettings.manualBackupType, true);
			MainWindow->checkMenu(IDM_AUTODETECTSAVETYPE_INTERNAL, CommonSettings.autodetectBackupMethod == 0);
			MainWindow->checkMenu(IDM_AUTODETECTSAVETYPE_FROMDATABASE, CommonSettings.autodetectBackupMethod == 1);

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

			// load type
			MainWindow->checkMenu(ID_STREAMFROMDISK, ((CommonSettings.loadToMemory == false)));
			MainWindow->checkMenu(ID_LOADTORAM, ((CommonSettings.loadToMemory == true)));

			// Tools
			MainWindow->checkMenu(IDM_CONSOLE_ALWAYS_ON_TOP, gConsoleTopmost);
			MainWindow->checkMenu(IDM_CONSOLE_SHOWHIDE, gShowConsole);
			MainWindow->enableMenu(IDM_CONSOLE_ALWAYS_ON_TOP, gShowConsole);

			UpdateHotkeyAssignments();	//Add current hotkey mappings to menu item names

			CallRegisteredLuaFunctions(LUACALL_ONINITMENU);

			return 0;
		}

	case WM_CREATE:
		{
			pausedByMinimize = FALSE;
			UpdateScreenRects();

			MainWindowToolbar = new CToolBar(hwnd);
			MainWindowToolbar->AppendButton(IDM_OPEN, IDB_OPEN, TBSTATE_ENABLED, false);
			MainWindowToolbar->AppendSeparator();
			MainWindowToolbar->AppendButton(IDM_PAUSE, IDB_PLAY, 0, false);
			MainWindowToolbar->AppendSeparator();

			//zero 03-feb-2013 - this isnt necessary, since the SetRotate function gets called eventually
			//int cwid, ccwid;
			//switch (video.rotation)
			//{
			//	case 0: cwid = IDC_ROTATE90; ccwid = IDC_ROTATE270; break;
			//	case 90: cwid = IDC_ROTATE180; ccwid = IDC_ROTATE0; break;
			//	case 180: cwid = IDC_ROTATE270; ccwid = IDC_ROTATE90; break;
			//	case 270: cwid = IDC_ROTATE0; ccwid = IDC_ROTATE180; break;
			//}

			DWORD rotstate = (video.layout == 0) ? TBSTATE_ENABLED : 0;
			MainWindowToolbar->AppendButton(0, IDB_ROTATECCW, rotstate, false);
			MainWindowToolbar->AppendButton(0, IDB_ROTATECW, rotstate, false);

			//we WANT it to be hard to do these operations. accidents would be bad. lets not use these buttons
			//MainWindowToolbar->AppendSeparator();
			//MainWindowToolbar->AppendSeparator();
			//MainWindowToolbar->AppendSeparator();
			//MainWindowToolbar->AppendButton(IDM_CLOSEROM, IDB_STOP, 0, false);
			//MainWindowToolbar->AppendButton(IDM_RESET, IDB_RESET, 0, false);


			bool showtb = GetPrivateProfileBool("Display", "Show Toolbar", true, IniName);
			MainWindowToolbar->Show(showtb);

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
			delete MainWindowToolbar;
			KillTimer(hwnd, autoHideCursorTimer);
			return 0;
		}
	case WM_TIMER:
	{
		if (autoHideCursorTimer == wParam)
		{
			if(autoHideCursor && !mouseLeftClientArea)
				if ((IsZoomed(hwnd) && (GetStyle()&DWS_FULLSCREEN)) || fsWindow)
					while (ShowCursor(FALSE) >= 0);
		}
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

			InvalidateRect(hwnd, NULL, FALSE); 
			UpdateWindow(hwnd);
			
			if (fsWindow) return 1;

			if(wParam==999)
			{
				//special fullscreen message
				fullscreen = true;
				wParam = WMSZ_BOTTOMRIGHT;
			}
			else
			{
				//handling not to be done during fullscreen:

				//user specified a different size. disable the window fixed size mode
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
				else if (video.layout == 1)
				{
					minX = video.rotatedwidthgap() * 2;
					minY = video.rotatedheightgap() * screenSizeRatio / 2;
				}
				else if (video.layout == 2)
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
					wndWidth = (rc.bottom - rc.top) - MainWindowToolbar->GetHeight();
					wndHeight = (rc.right - rc.left);
					wndHeightGapless = (MainScreenRect.right - MainScreenRect.left) + (SubScreenRect.right - SubScreenRect.left);
				}
				else
				{
					wndWidth = (rc.right - rc.left);
					wndHeight = (rc.bottom - rc.top) - MainWindowToolbar->GetHeight();
					wndHeightGapless = (MainScreenRect.bottom - MainScreenRect.top) + (SubScreenRect.bottom - SubScreenRect.top);
				}

				video.screengap = GPU_FRAMEBUFFER_NATIVE_WIDTH * (wndHeight - wndHeightGapless) / wndWidth;

				rc.bottom -= rc.top; rc.top = 0;
				rc.right -= rc.left; rc.left = 0;
				UpdateWndRects(MainWindow->getHWnd(), &rc);
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
		//MainWindowToolbar->Show(false);
		input_acquire();
		if(wParam != VK_PAUSE)
			break;
	
	case WM_CUSTKEYDOWN:
			//since the user has used a gamepad,
			//send some fake input to keep the screensaver from running
			PreventScreensaver();
			//do not trigger twice on alt/f10 hotkeys
			if(PurgeModifiers(wParam) == VK_MENU || PurgeModifiers(wParam) == VK_F10)
				break;
			goto DOKEYDOWN;
	case WM_SYSKEYDOWN:
DOKEYDOWN:
		{
			static bool reenter = false;

			if(reenter) break;

			reenter = true;

			if(message == WM_SYSKEYDOWN && wParam==VK_RETURN && !(lParam&0x40000000))
			{
				if(IsZoomed(hwnd))
					ShowWindow(hwnd,SW_NORMAL); //maximize and fullscreen get mixed up so make sure no maximize now. IOW, alt+enter from fullscreen should never result in a maximized state
				ToggleFullscreen();
			}
			else
			{
				int modifiers = GetModifiers(wParam);
				wParam = PurgeModifiers(wParam);
				if(!HandleKeyMessage(wParam,lParam, modifiers))
				{
					reenter = false;
					return 0;
				}
			}
			reenter = false;
			break;
		}
	case WM_KEYUP:
		//MainWindowToolbar->Show(true);
		//handle ctr+printscreen: our own custom printscreener
		if (wParam == VK_SNAPSHOT) { 
			if(GetKeyState(VK_CONTROL)&0x8000)
			{
				//include extra info
				ScreenshotToClipboard(true);
				return 0;
			}
			else if(GetKeyState(VK_SHIFT)&0x8000)
			{
				//exclude extra info
				ScreenshotToClipboard(false);
				return 0;
			}
		}
		input_acquire();
		if(wParam != VK_PAUSE)
			break;
	case WM_SYSKEYUP:
		if (wParam == VK_MENU && GetMenu(hwnd) == NULL)
			return 0;
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
					if(ForceRatio)
					{
						int xfudge = (fswidth-fakewidth)/2;
						int yfudge = (fsheight-fakeheight)/2;
						OffsetRect(&FullScreenRect,xfudge,yfudge);
					}
					else
						FullScreenRect = fullscreen;
				}
				else if(wParam==SIZE_RESTORED)
				{
					FixAspectRatio();
				}
				
				UpdateWndRects(hwnd);
				MainWindowToolbar->OnSize();
				SetEvent(display_wakeup_event);
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
				//ugh, dont do this when creating the window. in case we're using ddraw display method, ddraw wont be setup yet
				if(ddraw.handle)
					Display();
			}
			else
			{
				if(CommonSettings.single_core())
				{
					const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
					video.srcBuffer = (u8*)dispInfo.masterCustomBuffer;
					DoDisplay();
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
				if (!romloaded)
					OpenFile();
				if (romloaded && !(fileDropped.find(".dsm") == string::npos))	//.dsm is at the end of the filename so that must be the extension		
					FCEUI_LoadMovie(fileDropped.c_str(), 1, false, false);		 //We are convinced it is a movie file, attempt to load it
			}
			
			//-------------------------------------------------------
			//Check if Savestate file
			//-------------------------------------------------------
			else if (!(fileDropped.find(".ds") == string::npos))
			{
				if(romloaded)
				{
					size_t extIndex = fileDropped.find(".ds");
					if (extIndex <= fileDropped.length()-4)	//Check to see it is both at the end (file extension) and there is on more character
					{
						if ((fileDropped[extIndex+3] >= '0' && fileDropped[extIndex+3] <= '9') || fileDropped[extIndex+3] == '-' || fileDropped[extIndex+3] == 't')	//If last character is 0-9 (making .ds0 - .ds9) or .dst
						{
							savestate_load(filename);
							UpdateToolWindows();
						}
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
					ShowToolWindow(RamWatchHWnd);
				Load_Watches(true, fileDropped.c_str());
			}
			
			//-------------------------------------------------------
			//Else load it as a ROM
			//-------------------------------------------------------

			else if(OpenCoreSystemCP(filename))
			{
				romloaded = TRUE;
			}
			NDS_UnPause();
		}
		return 0;

	#ifndef WM_POINTERDOWN
		#define WM_POINTERDOWN   0x0246
	#endif
	#ifndef WM_POINTERUPDATE
		#define WM_POINTERUPDATE   0x0245
	#endif
	#ifndef POINTER_MESSAGE_FLAG_INCONTACT
		#define POINTER_MESSAGE_FLAG_INCONTACT   0x40000
	#endif
	#ifndef POINTER_MESSAGE_FLAG_FIRSTBUTTON
		#define POINTER_MESSAGE_FLAG_FIRSTBUTTON   0x100000
	#endif
	case WM_POINTERDOWN:
	case WM_POINTERUPDATE:

	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if (mouseLeftClientArea)
		{
			TrackMouseEvent(&tme);
			mouseLeftClientArea = false;
		}
		while (ShowCursor(TRUE) <= 0);
		autoHideCursorTimer = SetTimer(hwnd, 1, 10000, NULL);

		if (((message==WM_POINTERDOWN || message== WM_POINTERUPDATE)
			&& ((wParam & (POINTER_MESSAGE_FLAG_INCONTACT | POINTER_MESSAGE_FLAG_FIRSTBUTTON))))
			|| (message != WM_POINTERDOWN && message != WM_POINTERUPDATE && (wParam & MK_LBUTTON)))
		{
			SetCapture(hwnd);

			s32 x = (s32)((s16)LOWORD(lParam));
			s32 y = (s32)((s16)HIWORD(lParam));
			if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE)
			{
				POINT point;
				point.x = x;
				point.y = y;
				ScreenToClient(hwnd, &point);
				x = point.x;
				y = point.y;
			}
			UnscaleScreenCoords(x,y);

			if(HudEditorMode)
			{
				ToDSScreenRelativeCoords(x,y,0);
				EditHud(x,y, &Hud);
			}
			else
			{
				const bool isMainGPUFirst = (GPU->GetDisplayInfo().engineID[NDSDisplayID_Main] == GPUEngineID_Main);
				if ((video.layout == 2) && ((video.swap == 0) || (video.swap == 2 && isMainGPUFirst) || (video.swap == 3 && !isMainGPUFirst))) return 0;
				
				bool untouch = false;
				ToDSScreenRelativeCoords(x,y,1);
				if(x<0) 
				{
					x = 0; 
					untouch |= killStylusOffScreen;
				}
				if(x>255)
				{
					x = 255;
					untouch |= killStylusOffScreen;
				}
				if(y>191)
				{
					y = 191;
					untouch |= killStylusOffScreen;
				}
				if(y<-191)
				{
					y = -191;
					untouch |= killStylusOffScreen;
				}
				if(y<0)
				{
					y = 0; 
					untouch |= killStylusTopScreen;
				}

				if(untouch)
					NDS_releaseTouch();
				else
					NDS_setTouchPos(x, y);

				winLastTouch.x = x;
				winLastTouch.y = y;
				userTouchesScreen = true;
				return 0;
			}
		}
		else if (message == WM_POINTERUPDATE
			&& !(wParam & POINTER_MESSAGE_FLAG_INCONTACT | POINTER_MESSAGE_FLAG_FIRSTBUTTON))
		{
			ReleaseCapture();
			HudClickRelease(&Hud);
		}
		if (!StylusAutoHoldPressed)
			NDS_releaseTouch();
		userTouchesScreen = false;
		return 0;

	#ifndef WM_POINTERUP
		#define WM_POINTERUP   0x0247
	#endif
	case WM_POINTERUP:

	case WM_LBUTTONUP:

		ReleaseCapture();
		HudClickRelease(&Hud);
		if (!StylusAutoHoldPressed)
			NDS_releaseTouch();
		userTouchesScreen = false;
		return 0;

	case WM_MOUSELEAVE:
		mouseLeftClientArea = true;
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
				return 0;
			}
			else if(wParam == recentRoms_clearid)
			{
				/* Clear all the recent ROMs */
				if(IDYES == MessageBox(hwnd, "Are you sure you want to clear the recent ROMs list?", "DeSmuME", MB_YESNO | MB_ICONQUESTION))
					ClearRecentRoms();
				return 0;
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

				return 0;
			}

			if(wParam >= IDC_LUASCRIPT_RESERVE_START &&
			   wParam <= IDC_LUASCRIPT_RESERVE_END)
			{
				unsigned int index = wParam - IDC_LUASCRIPT_RESERVE_START;
				if(LuaScriptHWnds.size() > index)
					ShowToolWindow(LuaScriptHWnds[index]);

				return 0;
			}

			if(wParam >= IDC_LUAMENU_RESERVE_START &&
			   wParam <= IDC_LUAMENU_RESERVE_END)
			{
				CallRegisteredLuaMenuHandlers(wParam);
				return 0;
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
			HK_QuickScreenShot(0, true);
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
		case IDM_RENDER_HQ4X:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video.HQ4X);
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
		case IDM_RENDER_2XBRZ:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video._2XBRZ);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_3XBRZ:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video._3XBRZ);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_4XBRZ:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video._4XBRZ);
				FilterUpdate(hwnd);
			}
			break;
		case IDM_RENDER_5XBRZ:
			{
				Lock lock (win_backbuffer_sync);
				video.setfilter(video._5XBRZ);
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
				std::string dir = path.getpath(path.STATES);
				ofn.lpstrInitialDir = dir.c_str();

				if(!GetOpenFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

				dir = Path::GetFileDirectoryPath(SavName);
				path.setpath(path.STATES, dir);
				WritePrivateProfileString(SECTION, STATEKEY, dir.c_str(), IniName);

				savestate_load(SavName);
				UpdateToolWindows();
				NDS_UnPause();
			}
			return 0;
		case IDM_STATE_SAVE:
			{
				OPENFILENAME ofn;
				bool unpause = NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "DeSmuME Savestate (*.dst or *.ds#)\0*.dst;*.ds0*;*.ds1*;*.ds2*;*.ds3*;*.ds4*;*.ds5*;*.ds6*;*.ds7*;*.ds8*;*.ds9*;*.ds-*\0DeSmuME Savestate (*.dst only)\0*.dst\0All files (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  SavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dst";
				ofn.Flags = OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;
				std::string dir = path.getpath(path.STATES);
				ofn.lpstrInitialDir = dir.c_str();

				if(GetSaveFileName(&ofn))
				{
					savestate_save(SavName);
					LoadSaveStateInfo();
				}

				dir = Path::GetFileDirectoryPath(SavName);
				path.setpath(path.STATES, dir);
				WritePrivateProfileString(SECTION, STATEKEY, dir.c_str(), IniName);

				if(unpause) NDS_UnPause();
				return 0;
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
				HK_StateSaveSlot(LOWORD(wParam)-IDM_STATE_SAVE_F10, true);
				return 0;

		case IDM_STATE_LOAD_F1:
		case IDM_STATE_LOAD_F2:
		case IDM_STATE_LOAD_F3:
		case IDM_STATE_LOAD_F4:
		case IDM_STATE_LOAD_F5:
		case IDM_STATE_LOAD_F6:
		case IDM_STATE_LOAD_F7:
		case IDM_STATE_LOAD_F8:
		case IDM_STATE_LOAD_F9:
		case IDM_STATE_LOAD_F10:
			HK_StateLoadSlot(LOWORD(wParam)-IDM_STATE_LOAD_F10, true);
			return 0;

		case IDM_IMPORTBACKUPMEMORY:
			{
				NDS_Pause();
				if (!importSave(hwnd, hAppInst))
					MessageBox(hwnd,"Save was not successfully imported", "Error", MB_OK | MB_ICONERROR);
				NDS_UnPause();
				return 0;
			}
		case IDM_EXPORTBACKUPMEMORY:
			{
				NDS_Pause();
				if (!exportSave(hwnd, hAppInst))
					MessageBox(hwnd,"Save was not successfully exported","Error",MB_OK);
				NDS_UnPause();
				return 0;
			}
		case IDM_FILE_IMPORT_DB:
			{
				OPENFILENAME ofn;
				NDS_Pause();
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwnd;
				ofn.lpstrFilter = "ADVANsCEne database (XML)\0*.xml;\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  ImportSavName;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "xml";
				ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

				char buffer[MAX_PATH];
				ZeroMemory(buffer, sizeof(buffer));
				strcpy(buffer, path.pathToModule);
				ofn.lpstrInitialDir = buffer;
				strcat(buffer, "desmume.ddb");
				advsc.setDatabase(buffer);

				if(!GetOpenFileName(&ofn))
				{
					NDS_UnPause();
					return 0;
				}

				EMUFILE_FILE outf(advsc.getDatabase(),"wb");
				u32 count = advsc.convertDB(ImportSavName,outf);
				if (count > 0)
				{
					sprintf(buffer, "ADVANsCEne database was successfully imported\n(%i records)", count);
					MessageBox(hwnd,buffer,"DeSmuME",MB_OK|MB_ICONINFORMATION);
				}
				else
				{
					MessageBox(hwnd,"ADVANsCEne database was not successfully imported.","DeSmuME",MB_OK|MB_ICONERROR);
					if(advsc.lastImportErrorMessage != "") MessageBox(hwnd,advsc.lastImportErrorMessage.c_str(),"DeSmuME",MB_OK|MB_ICONERROR);
				}
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
			if (AreMovieEmulationSettingsActive())
			{
				MessageBox(hwnd, "The current settings have been set by a movie. Reset or unload the current game if you want to restore your saved settings.\n\n"
					"If you make changes here, the new settings will overwrite your currently saved settings.", "Movie Settings Active", MB_OK);
			}
			RunConfig(CONFIGSCREEN_FIRMWARE);
			return 0;
		case IDM_SOUNDSETTINGS:
			RunConfig(CONFIGSCREEN_SOUND);
			return 0;
		case IDM_WIFISETTINGS:
			RunConfig(CONFIGSCREEN_WIFI);
			return 0;
		case IDM_EMULATIONSETTINGS:
			if (AreMovieEmulationSettingsActive())
			{
				MessageBox(hwnd, "The current settings have been set by a movie. Reset or unload the current game if you want to restore your saved settings.\n\n"
					"If you make changes here (whether you reset now or not), the new settings will overwrite your currently saved settings.", "Movie Settings Active", MB_OK);
			}
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
		case IDM_SOUND_VIEW:
			if(!SoundView_IsOpened()) SoundView_DlgOpen(HWND_DESKTOP);
			return 0;
		case IDM_DISASSEMBLER:
			ViewDisasm_ARM7->regClass("DesViewBox7",ViewDisasm_ARM7BoxProc);
			if(!ViewDisasm_ARM7->IsOpen())
			{
				if (!ViewDisasm_ARM7->open(false))
					ViewDisasm_ARM7->unregClass();
			}
			else ViewDisasm_ARM7->Activate();

			ViewDisasm_ARM9->regClass("DesViewBox9",ViewDisasm_ARM9BoxProc);
			if(!ViewDisasm_ARM9->IsOpen())
			{
				if (!ViewDisasm_ARM9->open(false))
					ViewDisasm_ARM9->unregClass();
			}
			else ViewDisasm_ARM9->Activate();
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

		case ID_TOOLS_VIEWFSNITRO:
			ViewFSNitro->open();
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

		case IDM_MBG0: TwiddleLayer(IDM_MBG0,0,0); return 0;
		case IDM_MBG1: TwiddleLayer(IDM_MBG1,0,1); return 0;
		case IDM_MBG2: TwiddleLayer(IDM_MBG2,0,2); return 0;
		case IDM_MBG3: TwiddleLayer(IDM_MBG3,0,3); return 0;
		case IDM_MOBJ: TwiddleLayer(IDM_MOBJ,0,4); return 0;
		case IDM_SBG0: TwiddleLayer(IDM_SBG0,1,0); return 0;
		case IDM_SBG1: TwiddleLayer(IDM_SBG1,1,1); return 0;
		case IDM_SBG2: TwiddleLayer(IDM_SBG2,1,2); return 0;
		case IDM_SBG3: TwiddleLayer(IDM_SBG3,1,3); return 0;
		case IDM_SOBJ: TwiddleLayer(IDM_SOBJ,1,4); return 0;

		case IDM_PAUSE:
			TogglePause();
			return 0;

		case IDM_SLOT1:
			slot1Dialog(hwnd);
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
			CommonSettings.cheatsDisable = !CommonSettings.cheatsDisable;
			WritePrivateProfileBool("General", "cheatsDisable", CommonSettings.cheatsDisable, IniName);
			MainWindow->checkMenu(IDM_CHEATS_DISABLE, CommonSettings.cheatsDisable );
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
			doLCDsLayout(0);
			return 0;

		case ID_LCDS_HORIZONTAL:
			if (video.layout == 1) return 0;
			doLCDsLayout(1);
			return 0;

		case ID_LCDS_ONE:
			if (video.layout == 2) return 0;
			doLCDsLayout(2);
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

		case ID_VIEW_DISPLAYRTC:
			CommonSettings.hud.ShowRTC ^= true;
			WritePrivateProfileBool("Display", "Display RTC", CommonSettings.hud.ShowRTC, IniName);
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
				ShowToolWindow(RamSearchHWnd);
			break;

		case ID_RAM_WATCH:
			if(!RamWatchHWnd)
			{
				RamWatchHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_RAMWATCH), hwnd, (DLGPROC) RamWatchProc);
				//				DialogsOpen++;
			}
			else
				ShowToolWindow(RamWatchHWnd);
			return 0;

		case IDC_BACKGROUNDPAUSE:
			lostFocusPause = !lostFocusPause;
			allowBackgroundInput &= !lostFocusPause;
			WritePrivateProfileInt("Focus", "BackgroundPause", (int)lostFocusPause, IniName);
			WritePrivateProfileInt("Controls", "AllowBackgroundInput", (int)allowBackgroundInput, IniName);
			return 0;

		case IDC_BACKGROUNDINPUT:
			allowBackgroundInput = !allowBackgroundInput;
			WritePrivateProfileInt("Controls", "AllowBackgroundInput", (int)allowBackgroundInput, IniName);
			return 0;

		case ID_DISPLAYMETHOD_VSYNC:
			SetStyle(GetStyle()^DWS_VSYNC);
			WritePrivateProfileInt("Video","VSync", (GetStyle()&DWS_VSYNC)?1:0, IniName);
			break;

		case ID_DISPLAYMETHOD_DIRECTDRAWHW:
			{
				Lock lock (win_backbuffer_sync);
				displayMethod = DISPMETHOD_DDRAW_HW;
				ddraw.systemMemory = false;
				WritePrivateProfileInt("Video","Display Method", DISPMETHOD_DDRAW_HW, IniName);
				ddraw.createSurfaces(hwnd);
			}
			break;

		case ID_DISPLAYMETHOD_DIRECTDRAWSW:
			{
				Lock lock (win_backbuffer_sync);
				displayMethod = DISPMETHOD_DDRAW_SW;
				ddraw.systemMemory = true;
				WritePrivateProfileInt("Video","Display Method", DISPMETHOD_DDRAW_SW, IniName);
				ddraw.createSurfaces(hwnd);
			}
			break;

		case ID_DISPLAYMETHOD_OPENGL:
			{
				Lock lock (win_backbuffer_sync);
				displayMethod = DISPMETHOD_OPENGL;
				WritePrivateProfileInt("Video","Display Method", DISPMETHOD_OPENGL, IniName);
			}
			break;

		case ID_DISPLAYMETHOD_FILTER:
			{
				Lock lock (win_backbuffer_sync);
				gldisplay.filter = !gldisplay.filter;
				WritePrivateProfileInt("Video","Display Method Filter", gldisplay.filter?1:0, IniName);
			}
			break;

		case IDM_RESET:
			ResetGame();
			return 0;

		case ID_STREAMFROMDISK:
			CommonSettings.loadToMemory = false;
			WritePrivateProfileBool("General", "ROM Loading Mode", CommonSettings.loadToMemory, IniName);
			return 0;

		case ID_LOADTORAM:
			CommonSettings.loadToMemory = true;
			WritePrivateProfileBool("General", "ROM Loading Mode", CommonSettings.loadToMemory, IniName);
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

		case IDD_FONTCONFIG:
			{
				bool tpaused = false;
				if(execute)
				{
					tpaused = true;
					NDS_Pause();
				}

				DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_FONTSETTINGS), hwnd, (DLGPROC)HUDFontSettingsDlgProc);

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
			ShellExecute(NULL, "open", "http://desmume.org", NULL, NULL, SW_SHOWNORMAL);
			return 0;

		case IDM_FORUM:
			ShellExecute(NULL, "open", "http://forums.desmume.org/index.php", NULL, NULL, SW_SHOWNORMAL);
			return 0;

		case IDM_WIKI:
			ShellExecute(NULL, "open", "http://wiki.desmume.org", NULL, NULL, SW_SHOWNORMAL);
			return 0;

		case IDM_FAQ:
			ShellExecute(NULL, "open", "http://wiki.desmume.org/index.php?title=Faq", NULL, NULL, SW_SHOWNORMAL);
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
			ShellExecute(NULL, "open", "http://sourceforge.net/p/desmume/bugs/", NULL, NULL, SW_SHOWNORMAL);
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
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW2_5X:
			windowSize=kScale2point5x;
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW1X:
			windowSize=1;
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW2X:
			windowSize=2;
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW3X:
			windowSize=3;
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW4X:
			windowSize=4;
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;
		case IDC_WINDOW5X:
			windowSize=5;
			ScaleScreen(windowSize, true);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			break;

		case IDC_SCR_RATIO_1p0:
		case IDC_SCR_RATIO_1p1:
		case IDC_SCR_RATIO_1p2:
		case IDC_SCR_RATIO_1p3:
		case IDC_SCR_RATIO_1p4:
		case IDC_SCR_RATIO_1p5:
		{
			u32 style = GetStyle();
			bool maximized = IsZoomed(hwnd) || fsWindow;
			if (maximized)
				RestoreWindow(hwnd);

			float oldScreenSizeRatio = screenSizeRatio;
			screenSizeRatio = LOWORD(wParam) - IDC_SCR_RATIO_1p0;
			screenSizeRatio = screenSizeRatio * 0.1 + 1;

			RECT rc;
			GetClientRect(hwnd, &rc);
			MainWindow->setClientSize((int)((rc.right - rc.left) * (oldScreenSizeRatio / screenSizeRatio)), rc.bottom - rc.top);

			char scrRatStr[4] = "1.0";
			sprintf(scrRatStr, "%.1f", screenSizeRatio);
			WritePrivateProfileString("Display", "Screen Size Ratio", scrRatStr, IniName);

			if (maximized)
			{
				if (style & DWS_FULLSCREEN)
					ShowFullScreen(hwnd);
				else
					ShowWindow(hwnd, SW_MAXIMIZE);
			}
		}
		return 0;

		case IDC_SCR_VCENTER:
		{
			vCenterResizedScr = (!vCenterResizedScr) ? TRUE : FALSE;
			UpdateWndRects(hwnd);
			WritePrivateProfileInt("Display", "Vertically Center Resized Screen", vCenterResizedScr, IniName);
		}
		return 0;

		case IDC_VIEW_PADTOINTEGER:
			PadToInteger = (!PadToInteger)?TRUE:FALSE;
			WritePrivateProfileInt("Video","Window Pad To Integer",PadToInteger,IniName);
			UpdateWndRects(hwnd);
			break;

		case IDC_FORCERATIO:
		{
			u32 style = GetStyle();
			bool maximized = IsZoomed(hwnd) || fsWindow;
			if (maximized)
				RestoreWindow(hwnd);

			ForceRatio = (!ForceRatio)?TRUE:FALSE;
			if((int)(screenSizeRatio * 10) > 10) UpdateWndRects(hwnd);
			if(ForceRatio)
				FixAspectRatio();
			WritePrivateProfileInt("Video", "Window Force Ratio", ForceRatio, IniName);

			if (maximized)
			{
				if (style & DWS_FULLSCREEN)
					ShowFullScreen(hwnd);
				else
					ShowWindow(hwnd, SW_MAXIMIZE);
			}
		}	
		break;

		case IDM_DEFSIZE:
			{
				windowSize = 1;
				ScaleScreen(1, true);
			}
			break;
		case IDM_LOCKDOWN:
			{
				u32 style = GetStyle();
				bool maximized = IsZoomed(hwnd) || fsWindow;
				if (maximized)
					RestoreWindow(hwnd);

				RECT rc; 
				GetClientRect(hwnd, &rc);

				SetStyle(GetStyle()^DWS_LOCKDOWN);

				MainWindow->setClientSize(rc.right-rc.left, rc.bottom-rc.top);

				WritePrivateProfileBool("Video", "Window Lockdown", (GetStyle()&DWS_LOCKDOWN)!=0, IniName);

				if (maximized)
				{
					if (style & DWS_FULLSCREEN)
						ShowFullScreen(hwnd);
					else
						ShowWindow(hwnd, SW_MAXIMIZE);
				}
			}
			return 0;
		case IDM_ALWAYS_ON_TOP:
			{
				SetStyle(GetStyle()^DWS_ALWAYSONTOP);
				WritePrivateProfileBool("Video", "Window Always On Top", (GetStyle()&DWS_ALWAYSONTOP)!=0, IniName);
			}
			return 0;

		case IDM_CONSOLE_ALWAYS_ON_TOP:
			{
				gConsoleTopmost = !gConsoleTopmost;
				ConsoleAlwaysTop(gConsoleTopmost);
				WritePrivateProfileBool("Console", "Always On Top", gConsoleTopmost, IniName);
			}
			return 0;

		case IDM_CONSOLE_SHOWHIDE:
		{
			gShowConsole = !gShowConsole;
			WritePrivateProfileBool("Console", "Show", gShowConsole, IniName);
			if(gShowConsole) 
			{
				OpenConsole();			// Init debug console
				ConsoleAlwaysTop(gConsoleTopmost);
			}
			else
			{
				CloseConsole();
			}
			return 0;
		}

		case IDM_SHOWTOOLBAR:
			{
				u32 style = GetStyle();
				bool maximized = IsZoomed(hwnd) || fsWindow;
				if (maximized)
					RestoreWindow(hwnd);

				RECT rc; 
				GetClientRect(hwnd, &rc); rc.top += MainWindowToolbar->GetHeight();

				bool nowvisible = !MainWindowToolbar->Visible();
				MainWindowToolbar->Show(nowvisible);

				MainWindow->setClientSize(rc.right-rc.left, rc.bottom-rc.top);

				WritePrivateProfileBool("Display", "Show Toolbar", nowvisible, IniName);

				if (maximized)
				{
					if (style & DWS_FULLSCREEN)
						ShowFullScreen(hwnd);
					else
						ShowWindow(hwnd, SW_MAXIMIZE);
				}
			}
			return 0;

		case IDM_FS_MENU:
			{
				SetStyle(GetStyle()^DWS_FS_MENU);
				WritePrivateProfileBool("Display", "Show Menu In Fullscreen Mode", (GetStyle()&DWS_FS_MENU)!= 0, IniName);
			}
			return 0;

		case IDM_FS_WINDOW:
		{
			u32 style = GetStyle();
			bool maximized = IsZoomed(hwnd) || fsWindow;
			if (maximized)
				RestoreWindow(hwnd);

			SetStyle(GetStyle() ^ DWS_FS_WINDOW);
			WritePrivateProfileBool("Display", "Non-exclusive Fullscreen Mode", (GetStyle()&DWS_FS_WINDOW) != 0, IniName);

			if (maximized)
			{
				if (style & DWS_FULLSCREEN)
					ShowFullScreen(hwnd);
				else
					ShowWindow(hwnd, SW_MAXIMIZE);
			}
		}
		return 0;

		case IDM_FS_HIDE_CURSOR:
		{
			autoHideCursor = !autoHideCursor;
			WritePrivateProfileBool("Display", "Auto-Hide Cursor", autoHideCursor, IniName);
		}
		return 0;

		case IDM_AUTODETECTSAVETYPE_INTERNAL: 
		case IDM_AUTODETECTSAVETYPE_FROMDATABASE: 
			CommonSettings.autodetectBackupMethod = LOWORD(wParam) - IDM_AUTODETECTSAVETYPE_INTERNAL; 
			WritePrivateProfileInt("General", "autoDetectMethod", CommonSettings.autodetectBackupMethod, IniName);
		return 0;

		case IDC_SAVETYPE_FORCE: backup_forceManualBackupType(); return 0; 

		default:
			{
				u32 id = LOWORD(wParam);
				if ((id >= IDC_SAVETYPE) && (id < IDC_SAVETYPE+MAX_SAVE_TYPES+1))
				{
					backup_setManualBackupType(id-IDC_SAVETYPE);
				}
				return 0;
			}
		}
		break;

		case WM_NOTIFY:
			{
				NMHDR nmhdr = *(NMHDR*)lParam;
				switch (nmhdr.code)
				{
				case TBN_DROPDOWN:
					{
						NMTOOLBAR nmtb = *(NMTOOLBAR*)lParam;

						if (nmtb.iItem == IDM_OPEN)
						{
							// Get the recent roms menu (too lazy to make a new menu :P )
							HMENU _rrmenu = GetSubMenu(recentromsmenu, 0);

							// Here are the coordinates we want the recent roms menu to popup at
							POINT pt;
							pt.x = nmtb.rcButton.left;
							pt.y = nmtb.rcButton.bottom;

							// Convert the coordinates to screen coordinates
							ClientToScreen(hwnd, &pt);

							// Finally show the menu; once the user chose a ROM, we'll get a WM_COMMAND
							TrackPopupMenu(_rrmenu, 0, pt.x, pt.y, 0, hwnd, NULL);
							return TBDDRET_DEFAULT;
						}
					}
					return 0;

				case TTN_NEEDTEXT:
					{
						TOOLTIPTEXT* ttt = (TOOLTIPTEXT*)lParam;
						ttt->hinst = hAppInst;

						switch (ttt->hdr.idFrom)
						{
						case IDM_OPEN:
							if (RecentRoms.empty()) ttt->lpszText = "Open a ROM";
							else ttt->lpszText = "Open a ROM\nClick the arrow to open a recent ROM"; break;

						case IDM_PAUSE:
							if (paused) ttt->lpszText = "Resume emulation";
							else ttt->lpszText = "Pause emulation"; break;

						case IDM_CLOSEROM: ttt->lpszText = "Stop emulation"; break;
						case IDM_RESET: ttt->lpszText = "Reset emulation"; break;

						case IDC_ROTATE0:
							if (video.rotation == 90) ttt->lpszText = "Rotate CCW";
							else if (video.rotation == 270) ttt->lpszText = "Rotate CW"; break;
						case IDC_ROTATE90:
							if (video.rotation == 180) ttt->lpszText = "Rotate CCW";
							else if (video.rotation == 0) ttt->lpszText = "Rotate CW"; break;
						case IDC_ROTATE180:
							if (video.rotation == 270) ttt->lpszText = "Rotate CCW";
							else if (video.rotation == 90) ttt->lpszText = "Rotate CW"; break;
						case IDC_ROTATE270:
							if (video.rotation == 0) ttt->lpszText = "Rotate CCW";
							else if (video.rotation == 180) ttt->lpszText = "Rotate CW"; break;
						}
					}
					return 0;
				}
			}
			return 0;
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

void Change3DCoreWithFallbackAndSave(int newCore)
{
	printf("Attempting change to 3d core to: %s\n",core3DList[newCore]->name);

	if(newCore == GPU3D_OPENGL_OLD)
		goto TRY_OGL_OLD;

	if(newCore == GPU3D_SWRAST)
		goto TRY_SWRAST;

	if(newCore == RENDERID_NULL)
	{
		GPU->Change3DRendererByID(RENDERID_NULL);
		goto DONE;
	}

	if(!GPU->Change3DRendererByID(GPU3D_OPENGL_3_2))
	{
		printf("falling back to 3d core: %s\n",core3DList[GPU3D_OPENGL_OLD]->name);
		goto TRY_OGL_OLD;
	}
	goto DONE;

TRY_OGL_OLD:
	if(!GPU->Change3DRendererByID(GPU3D_OPENGL_OLD))
	{
		printf("falling back to 3d core: %s\n",core3DList[GPU3D_SWRAST]->name);
		goto TRY_SWRAST;
	}
	goto DONE;

TRY_SWRAST:
	GPU->Change3DRendererByID(GPU3D_SWRAST);
	
DONE:
	int gpu3dSaveValue = ((cur3DCore != RENDERID_NULL) ? cur3DCore : RENDERID_NULL_SAVED);
	WritePrivateProfileInt("3D", "Renderer", gpu3dSaveValue, IniName);
}

LRESULT CALLBACK HUDFontSettingsDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			for(int i=0;i<font_Nums;i++) ComboBox_AddString(GetDlgItem(hw, IDC_FONTCOMBO), fonts_list[i].name);
			ComboBox_SetCurSel(GetDlgItem(hw, IDC_FONTCOMBO),GetPrivateProfileInt("Display","HUD Font", font_Nums-1, IniName));
		}
		return TRUE;

		case WM_COMMAND:
		{
			switch(LOWORD(wp))
			{
			case IDOK:
				{
					int i = ComboBox_GetCurSel(GetDlgItem(hw, IDC_FONTCOMBO));
					aggDraw.hud->setFont(fonts_list[i].name);
					WritePrivateProfileInt("Display","HUD Font", i, IniName);
				}
			case IDCANCEL:
				{
					EndDialog(hw, TRUE);
				}
				return TRUE;
			}
			return TRUE;
		}
	}
	return FALSE;
}
LRESULT CALLBACK GFX3DSettingsDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			const char MSAADescriptions[6][9] = {"Disabled", "2x", "4x", "8x", "16x", "32x"};

			if (!didGetMaxSamples)
			{
				if (cur3DCore != 0 && cur3DCore != 2) //Get max device samples from the current renderer only if it's OpenGL.
				{
					maxSamples = CurrentRenderer->GetDeviceInfo().maxSamples;
					didGetMaxSamples = true;
				}
				else
				{
					bool isTempContextCreated = windows_opengl_init(); //Create a context just to get max device samples.
					if (isTempContextCreated) //Creating it here because it's only needed in this window.
					{
						GLint maxSamplesOGL = 0;
#if defined(GL_MAX_SAMPLES)
						glGetIntegerv(GL_MAX_SAMPLES, &maxSamplesOGL);
#elif defined(GL_MAX_SAMPLES_EXT)
						glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamplesOGL);
#endif
						maxSamples = maxSamplesOGL;
						didGetMaxSamples = true;
					}
				}
			}

			CheckDlgButton(hw, IDC_INTERPOLATECOLOR, CommonSettings.GFX3D_HighResolutionInterpolateColor);
			CheckDlgButton(hw, IDC_3DSETTINGS_LINEHACK, CommonSettings.GFX3D_LineHack);
			CheckDlgButton(hw, IDC_TXTHACK, CommonSettings.GFX3D_TXTHack);
			CheckDlgButton(hw, IDC_TEX_DEPOSTERIZE, CommonSettings.GFX3D_Renderer_TextureDeposterize);
			CheckDlgButton(hw, IDC_3DSETTINGS_EDGEMARK, CommonSettings.GFX3D_EdgeMark);
			CheckDlgButton(hw, IDC_3DSETTINGS_FOG, CommonSettings.GFX3D_Fog);
			CheckDlgButton(hw, IDC_3DSETTINGS_TEXTURE, CommonSettings.GFX3D_Texture);
			CheckDlgButton(hw, IDC_TEX_SMOOTH, CommonSettings.GFX3D_Renderer_TextureSmoothing);
			CheckDlgButton(hw, IDC_SHADOW_POLYGONS, CommonSettings.OpenGL_Emulation_ShadowPolygon);
			CheckDlgButton(hw, IDC_S_0_ALPHA_BLEND, CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending);
			CheckDlgButton(hw, IDC_NDS_DEPTH_CALC, CommonSettings.OpenGL_Emulation_NDSDepthCalculation);
			CheckDlgButton(hw, IDC_DEPTH_L_EQUAL_PF, CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing);

			SendDlgItemMessage(hw, IDC_NUD_PRESCALEHD, UDM_SETRANGE, 0, MAKELPARAM(16, 1));
			SendDlgItemMessage(hw, IDC_NUD_PRESCALEHD, UDM_SETPOS, 0, video.prescaleHD);

			// Generate the Color Depth pop-up menu
			ComboBox_AddString(GetDlgItem(hw, IDC_GPU_COLOR_DEPTH), "15 bit");
			ComboBox_AddString(GetDlgItem(hw, IDC_GPU_COLOR_DEPTH), "18 bit");
			ComboBox_AddString(GetDlgItem(hw, IDC_GPU_COLOR_DEPTH), "24 bit");
			ComboBox_SetCurSel(GetDlgItem(hw, IDC_GPU_COLOR_DEPTH), 1);
			// Generate the Texture Scaling pop-up menu
			ComboBox_AddString(GetDlgItem(hw, IDC_TEXSCALE), "1x");
			ComboBox_AddString(GetDlgItem(hw, IDC_TEXSCALE), "2x");
			ComboBox_AddString(GetDlgItem(hw, IDC_TEXSCALE), "4x");
			ComboBox_SetCurSel(GetDlgItem(hw, IDC_TEXSCALE), 0);

			for (int i = 0; i < 3; i++)
			{
				if (gpu_bpp == possibleBPP[i])
				{
					ComboBox_SetCurSel(GetDlgItem(hw, IDC_GPU_COLOR_DEPTH), i);
				}
				if (CommonSettings.GFX3D_Renderer_TextureScalingFactor == possibleTexScale[i])
				{
					ComboBox_SetCurSel(GetDlgItem(hw, IDC_TEXSCALE), i);
				}
			}

			if (CommonSettings.GFX3D_Renderer_MultisampleSize > maxSamples) 
			{
				CommonSettings.GFX3D_Renderer_MultisampleSize = maxSamples;
				WritePrivateProfileInt("3D", "MultisampleSize", maxSamples, IniName);
			}

			// Generate the MSAA pop-up menu
			ComboBox_AddString(GetDlgItem(hw, IDC_MULTISAMPLE_SIZE), MSAADescriptions[0]);
			ComboBox_SetCurSel(GetDlgItem(hw, IDC_MULTISAMPLE_SIZE), 0);
			for (int i = 1, z = 2; z <= maxSamples; i++, z*=2)
			{
				ComboBox_AddString(GetDlgItem(hw, IDC_MULTISAMPLE_SIZE), MSAADescriptions[i]);
				if (z == CommonSettings.GFX3D_Renderer_MultisampleSize) 
				{
					ComboBox_SetCurSel(GetDlgItem(hw, IDC_MULTISAMPLE_SIZE), i);
				}
			}

			// Generate the 3D Rendering Engine pop-up menu
			for (int i = 0; core3DList[i] != NULL; i++)
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
					CommonSettings.GFX3D_HighResolutionInterpolateColor = IsDlgCheckboxChecked(hw, IDC_INTERPOLATECOLOR);
					CommonSettings.GFX3D_LineHack = IsDlgCheckboxChecked(hw, IDC_3DSETTINGS_LINEHACK);
					CommonSettings.GFX3D_TXTHack = IsDlgCheckboxChecked(hw, IDC_TXTHACK);
					gpu_bpp = possibleBPP[SendDlgItemMessage(hw, IDC_GPU_COLOR_DEPTH, CB_GETCURSEL, 0, 0)];
					CommonSettings.GFX3D_Renderer_TextureScalingFactor = possibleTexScale[SendDlgItemMessage(hw, IDC_TEXSCALE, CB_GETCURSEL, 0, 0)];
					CommonSettings.GFX3D_Renderer_TextureDeposterize = IsDlgCheckboxChecked(hw, IDC_TEX_DEPOSTERIZE);
					CommonSettings.GFX3D_EdgeMark = IsDlgCheckboxChecked(hw, IDC_3DSETTINGS_EDGEMARK);
					CommonSettings.GFX3D_Fog = IsDlgCheckboxChecked(hw, IDC_3DSETTINGS_FOG);
					CommonSettings.GFX3D_Texture = IsDlgCheckboxChecked(hw, IDC_3DSETTINGS_TEXTURE);
					CommonSettings.GFX3D_Renderer_MultisampleSize = possibleMSAA[SendDlgItemMessage(hw, IDC_MULTISAMPLE_SIZE, CB_GETCURSEL, 0, 0)];
					CommonSettings.GFX3D_Renderer_TextureSmoothing = IsDlgCheckboxChecked(hw, IDC_TEX_SMOOTH);
					CommonSettings.OpenGL_Emulation_ShadowPolygon = IsDlgCheckboxChecked(hw, IDC_SHADOW_POLYGONS);
					CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = IsDlgCheckboxChecked(hw, IDC_S_0_ALPHA_BLEND);
					CommonSettings.OpenGL_Emulation_NDSDepthCalculation = IsDlgCheckboxChecked(hw, IDC_NDS_DEPTH_CALC);
					CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = IsDlgCheckboxChecked(hw, IDC_DEPTH_L_EQUAL_PF);
					
					int newPrescaleHD = video.prescaleHD;
					LRESULT scaleResult = SendDlgItemMessage(hw, IDC_NUD_PRESCALEHD, UDM_GETPOS, 0, 0);
					if (HIWORD(scaleResult) == 0)
					{
						newPrescaleHD = LOWORD(scaleResult);
					}

					{
						Lock lock(win_backbuffer_sync);
						if(display_mutex) slock_lock(display_mutex);
						Change3DCoreWithFallbackAndSave(ComboBox_GetCurSel(GetDlgItem(hw, IDC_3DCORE)));
						if (newPrescaleHD != video.prescaleHD)
						{
							video.SetPrescale(newPrescaleHD, 1);
							GPU->SetCustomFramebufferSize(GPU_FRAMEBUFFER_NATIVE_WIDTH*video.prescaleHD, GPU_FRAMEBUFFER_NATIVE_HEIGHT*video.prescaleHD);
						}
						SyncGpuBpp();
						UpdateScreenRects();
						if(display_mutex) slock_unlock(display_mutex);
						// shrink buffer size if necessary
						const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
						size_t newBufferSize = displayInfo.customWidth * displayInfo.customHeight * 2 * displayInfo.pixelBytes;
						if (newBufferSize < video.srcBufferSize) video.srcBufferSize = newBufferSize;
					}

					WritePrivateProfileBool("3D", "HighResolutionInterpolateColor", CommonSettings.GFX3D_HighResolutionInterpolateColor, IniName);
					WritePrivateProfileBool("3D", "EnableTXTHack", CommonSettings.GFX3D_TXTHack, IniName);
					WritePrivateProfileBool("3D", "EnableLineHack", CommonSettings.GFX3D_LineHack, IniName);
					WritePrivateProfileInt ("3D", "PrescaleHD", video.prescaleHD, IniName);
					WritePrivateProfileInt ("3D", "GpuBpp", gpu_bpp, IniName);
					WritePrivateProfileInt ("3D", "TextureScalingFactor", CommonSettings.GFX3D_Renderer_TextureScalingFactor, IniName);
					WritePrivateProfileBool("3D", "TextureDeposterize", CommonSettings.GFX3D_Renderer_TextureDeposterize, IniName);
					WritePrivateProfileBool("3D", "EnableEdgeMark", CommonSettings.GFX3D_EdgeMark, IniName);
					WritePrivateProfileBool("3D", "EnableFog", CommonSettings.GFX3D_Fog, IniName);
					WritePrivateProfileBool("3D", "EnableTexture", CommonSettings.GFX3D_Texture, IniName);
					WritePrivateProfileInt ("3D", "MultisampleSize", CommonSettings.GFX3D_Renderer_MultisampleSize, IniName);
					WritePrivateProfileBool("3D", "TextureSmooth", CommonSettings.GFX3D_Renderer_TextureSmoothing, IniName);
					WritePrivateProfileBool("3D", "EnableShadowPolygon", CommonSettings.OpenGL_Emulation_ShadowPolygon, IniName);
					WritePrivateProfileBool("3D", "EnableSpecialZeroAlphaBlending", CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending, IniName);
					WritePrivateProfileBool("3D", "EnableNDSDepthCalculation", CommonSettings.OpenGL_Emulation_NDSDepthCalculation, IniName);
					WritePrivateProfileBool("3D", "EnableDepthLEqualPolygonFacing", CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing, IniName);
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
			CheckDlgItem(hDlg, IDC_CHECBOX_ADVANCEDTIMING, CommonSettings.advanced_timing);
			CheckDlgItem(hDlg, IDC_CHECKBOX_GAMEHACKS, CommonSettings.gamehacks.en);
			CheckDlgItem(hDlg,IDC_USEEXTBIOS,CommonSettings.UseExtBIOS);
			CheckDlgItem(hDlg, IDC_BIOSSWIS, CommonSettings.SWIFromBIOS);
			CheckDlgItem(hDlg, IDC_PATCHSWI3, CommonSettings.PatchSWI3);
			SetDlgItemText(hDlg, IDC_ARM9BIOS, CommonSettings.ARM9BIOS);
			SetDlgItemText(hDlg, IDC_ARM7BIOS, CommonSettings.ARM7BIOS);
			CheckDlgItem(hDlg, IDC_CHECKBOX_DYNAREC, CommonSettings.use_jit);
#ifdef HAVE_JIT
			EnableWindow(GetDlgItem(hDlg, IDC_JIT_BLOCK_SIZE), CommonSettings.use_jit?true:false);
			char buf[4] = {0};
			itoa(CommonSettings.jit_max_block_size, &buf[0], 10);
			SetDlgItemText(hDlg, IDC_JIT_BLOCK_SIZE, buf);
#else
			EnableWindow(GetDlgItem(hDlg, IDC_CHECKBOX_DYNAREC), false);
#endif

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
				cur = GetDlgItem(hDlg, IDC_PATCHSWI3);
				EnableWindow(cur, FALSE);
			}

			CheckDlgButton(hDlg, IDC_USEEXTFIRMWARE, ((CommonSettings.UseExtFirmware == true) ? BST_CHECKED : BST_UNCHECKED));
			SetDlgItemText(hDlg, IDC_FIRMWARE, CommonSettings.ExtFirmwarePath);
			CheckDlgButton(hDlg, IDC_FIRMWAREBOOT, ((CommonSettings.BootFromFirmware == true) ? BST_CHECKED : BST_UNCHECKED));
			CheckDlgButton(hDlg, IDC_FIRMWAREEXTUSER, ((CommonSettings.UseExtFirmwareSettings == true) ? BST_CHECKED : BST_UNCHECKED));

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

			if(CommonSettings.UseExtFirmware == false)
			{
				cur = GetDlgItem(hDlg, IDC_FIRMWAREEXTUSER);
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
					HWND cur;
					int val = 0;
#ifdef HAVE_JIT
					u32 jit_size = 0;
					if (IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_DYNAREC))
					{
						char jit_size_buf[4] = {0};

						memset(&jit_size_buf[0], 0, sizeof(jit_size_buf));
						cur = GetDlgItem(hDlg, IDC_JIT_BLOCK_SIZE);
						GetWindowText(cur, jit_size_buf, sizeof(jit_size_buf));
						jit_size = atoi(jit_size_buf);
						if ((jit_size < 1) || (jit_size > 100))
						{
							MessageBox(hDlg, "JIT block size should be in range 1..100\nTry again", "DeSmuME", MB_OK | MB_ICONERROR);
							return FALSE;
						}
					}
#endif
					if(romloaded)
						val = MessageBox(hDlg, "The current ROM needs to be reset to apply changes.\nReset now ?", "DeSmuME", (MB_YESNO | MB_ICONQUESTION));

					UnloadMovieEmulationSettings();

					CommonSettings.UseExtBIOS = IsDlgCheckboxChecked(hDlg, IDC_USEEXTBIOS);
					cur = GetDlgItem(hDlg, IDC_ARM9BIOS);
					GetWindowText(cur, CommonSettings.ARM9BIOS, 256);
					cur = GetDlgItem(hDlg, IDC_ARM7BIOS);
					GetWindowText(cur, CommonSettings.ARM7BIOS, 256);
					CommonSettings.SWIFromBIOS = IsDlgCheckboxChecked(hDlg, IDC_BIOSSWIS);
					CommonSettings.PatchSWI3 = IsDlgCheckboxChecked(hDlg, IDC_PATCHSWI3);

					CommonSettings.UseExtFirmware = IsDlgCheckboxChecked(hDlg, IDC_USEEXTFIRMWARE);
					cur = GetDlgItem(hDlg, IDC_FIRMWARE);
					GetWindowText(cur, CommonSettings.ExtFirmwarePath, 256);
					CommonSettings.BootFromFirmware = IsDlgCheckboxChecked(hDlg, IDC_FIRMWAREBOOT);
					CommonSettings.UseExtFirmwareSettings = IsDlgCheckboxChecked(hDlg, IDC_FIRMWAREEXTUSER);

					CommonSettings.DebugConsole = IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_DEBUGGERMODE);
					CommonSettings.EnsataEmulation = IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_ENSATAEMULATION);
					CommonSettings.advanced_timing = IsDlgCheckboxChecked(hDlg, IDC_CHECBOX_ADVANCEDTIMING);
					CommonSettings.gamehacks.en = IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_GAMEHACKS);
#ifdef HAVE_JIT
					CommonSettings.use_jit = IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_DYNAREC);
					if (CommonSettings.use_jit)
					{
						CommonSettings.jit_max_block_size = jit_size;
						WritePrivateProfileInt("Emulation", "JitSize", CommonSettings.jit_max_block_size, IniName);
					}
#endif

					WritePrivateProfileInt("Emulation", "DebugConsole", ((CommonSettings.DebugConsole == true) ? 1 : 0), IniName);
					WritePrivateProfileInt("Emulation", "EnsataEmulation", ((CommonSettings.EnsataEmulation == true) ? 1 : 0), IniName);
					WritePrivateProfileBool("Emulation", "AdvancedTiming", CommonSettings.advanced_timing, IniName);
					WritePrivateProfileBool("Emulation", "GameHacks", CommonSettings.gamehacks.en, IniName);
					WritePrivateProfileInt("BIOS", "UseExtBIOS", ((CommonSettings.UseExtBIOS == true) ? 1 : 0), IniName);
					WritePrivateProfileString("BIOS", "ARM9BIOSFile", CommonSettings.ARM9BIOS, IniName);
					WritePrivateProfileString("BIOS", "ARM7BIOSFile", CommonSettings.ARM7BIOS, IniName);
					WritePrivateProfileInt("BIOS", "SWIFromBIOS", ((CommonSettings.SWIFromBIOS == true) ? 1 : 0), IniName);
					WritePrivateProfileInt("BIOS", "PatchSWI3", ((CommonSettings.PatchSWI3 == true) ? 1 : 0), IniName);

					WritePrivateProfileInt("Firmware", "UseExtFirmware", ((CommonSettings.UseExtFirmware == true) ? 1 : 0), IniName);
					WritePrivateProfileString("Firmware", "FirmwareFile", CommonSettings.ExtFirmwarePath, IniName);
					WritePrivateProfileInt("Firmware", "BootFromFirmware", ((CommonSettings.BootFromFirmware == true) ? 1 : 0), IniName);
					WritePrivateProfileInt("Firmware", "UseExtFirmwareSettings", ((CommonSettings.UseExtFirmwareSettings == true) ? 1 : 0), IniName);
#ifdef HAVE_JIT
					WritePrivateProfileInt("Emulation", "CPUmode", ((CommonSettings.use_jit == true) ? 1 : 0), IniName);
#endif

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
					cur = GetDlgItem(hDlg, IDC_PATCHSWI3);
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
					cur = GetDlgItem(hDlg, IDC_FIRMWAREEXTUSER);
					EnableWindow(cur, (enable && IsDlgButtonChecked(hDlg, IDC_USEEXTFIRMWARE)));
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
					std::string dir = path.getpath(path.FIRMWARE);
					ofn.lpstrInitialDir = dir.c_str();

					if(GetOpenFileName(&ofn))
					{
						std::string dir = Path::GetFileDirectoryPath(fileName);
						path.setpath(path.FIRMWARE, dir);
						WritePrivateProfileString(SECTION, FIRMWAREKEY, dir.c_str(), IniName);

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

			case IDC_CHECKBOX_DYNAREC:
				EnableWindow(GetDlgItem(hDlg, IDC_JIT_BLOCK_SIZE), IsDlgCheckboxChecked(hDlg, IDC_CHECKBOX_DYNAREC));
				return TRUE;
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
					std::string dir = path.getpath(path.SOUNDS);
					ofn.lpstrInitialDir = dir.c_str();

					if(GetOpenFileName(&ofn))
					{
						std::string dir = Path::GetFileDirectoryPath(fileName);
						path.setpath(path.SOUNDS, dir);
						WritePrivateProfileString(SECTION, SOUNDKEY, dir.c_str(), IniName);

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
	const bool isPCapSupported = wifiHandler->IsPCapSupported();
	const WifiEmulationLevel emulationLevel = wifiHandler->GetSelectedEmulationLevel();

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
#ifdef EXPERIMENTAL_WIFI_COMM
			if (emulationLevel != WifiEmulationLevel_Off)
			{
				CheckDlgItem(hDlg, IDC_WIFI_ENABLED, TRUE);
				CheckDlgItem(hDlg, IDC_WIFI_COMPAT, (emulationLevel == WifiEmulationLevel_Compatibility));
			}
			else
			{
				CheckDlgItem(hDlg, IDC_WIFI_ENABLED, FALSE);
				CheckDlgItem(hDlg, IDC_WIFI_COMPAT, GetPrivateProfileBool("Wifi", "Compatibility Mode", FALSE, IniName));
			}
#else
			CheckDlgItem(hDlg, IDC_WIFI_ENABLED, FALSE);
			CheckDlgItem(hDlg, IDC_WIFI_COMPAT, FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_WIFI_ENABLED), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_WIFI_COMPAT), FALSE);
#endif

			HWND deviceMenu = GetDlgItem(hDlg, IDC_BRIDGEADAPTER);
			int menuItemCount = ComboBox_GetCount(deviceMenu);
			std::vector<std::string> deviceStringList;
			int curSel = 0;
			BOOL enableWin = FALSE;
			
			for (int i = 0; i < menuItemCount; i++)
			{
				ComboBox_DeleteString(deviceMenu, 0);
			}

			if (isPCapSupported)
			{
				int deviceCount = wifiHandler->GetBridgeDeviceList(&deviceStringList);

				if (deviceCount < 0)
				{
					ComboBox_AddString(deviceMenu, "Error: Searching for a device failed.");
				}
				else if (deviceCount == 0)
				{
					ComboBox_AddString(deviceMenu, "No devices were found.");
				}
				else
				{
					for (size_t i = 0; i < deviceCount; i++)
					{
						ComboBox_AddString(deviceMenu, deviceStringList[i].c_str());
					}
					curSel = CommonSettings.WifiBridgeDeviceID;
					enableWin = TRUE;
				}
			}
			else
			{
				ComboBox_AddString(deviceMenu, "Error: Could not load WinPcap.");
			}
			ComboBox_SetCurSel(deviceMenu, curSel);
			EnableWindow(deviceMenu, enableWin);
			
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

#ifdef EXPERIMENTAL_WIFI_COMM
					if (IsDlgCheckboxChecked(hDlg, IDC_WIFI_ENABLED))
					{
						if (IsDlgCheckboxChecked(hDlg, IDC_WIFI_COMPAT))
							wifiHandler->SetEmulationLevel(WifiEmulationLevel_Compatibility);
						else
							wifiHandler->SetEmulationLevel(WifiEmulationLevel_Normal);
					}
					else
						wifiHandler->SetEmulationLevel(WifiEmulationLevel_Off);

					WritePrivateProfileBool("Wifi", "Enabled", IsDlgCheckboxChecked(hDlg, IDC_WIFI_ENABLED), IniName);
					WritePrivateProfileBool("Wifi", "Compatibility Mode", IsDlgCheckboxChecked(hDlg, IDC_WIFI_COMPAT), IniName);
#else
					wifiHandler->SetEmulationLevel(WifiEmulationLevel_Off);
#endif

					cur = GetDlgItem(hDlg, IDC_BRIDGEADAPTER);
					CommonSettings.WifiBridgeDeviceID = ComboBox_GetCurSel(cur);
					wifiHandler->SetBridgeDeviceIndex(CommonSettings.WifiBridgeDeviceID);
					WritePrivateProfileInt("Wifi", "BridgeAdapter", CommonSettings.WifiBridgeDeviceID, IniName);

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
			CheckDlgItem(hDlg,IDC_SYNCHMODE_DUAL, CommonSettings.SPU_sync_mode==0);
			CheckDlgItem(hDlg,IDC_SYNCHMODE_SYNCH,CommonSettings.SPU_sync_mode==1);
			SoundSettings_updateSynchMode(hDlg);

			//update the synch method
			CheckDlgItem(hDlg,IDC_SYNCHMETHOD_N, CommonSettings.SPU_sync_method==0);
			CheckDlgItem(hDlg,IDC_SYNCHMETHOD_Z, CommonSettings.SPU_sync_method==1);
			CheckDlgItem(hDlg,IDC_SYNCHMETHOD_P, CommonSettings.SPU_sync_method==2);

			//setup interpolation combobox
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"None (harsh, most accurate to NDS)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"Linear (smooth, most sound detail loss)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_ADDSTRING, 0, (LPARAM)"Cosine (balanced, smooth and accurate)");
			SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_SETCURSEL, (int)CommonSettings.spuInterpolationMode, 0);

			// Setup Sound Buffer Size Edit Text
			sprintf(tempstr, "%d", sndbuffersize);
			SetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr);

			// Setup Volume Slider
			SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_SETRANGE, 0, MAKELONG(0, 100));

			// Set Selected Volume
			SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_SETPOS, TRUE, sndvolume);
			SoundSettings_updateVolumeReadout(hDlg);

			// Set spu advanced
			CheckDlgItem(hDlg,IDC_SPU_ADVANCED,CommonSettings.spu_advanced);

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
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMODE_DUAL)) CommonSettings.SPU_sync_mode = 0;
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMODE_SYNCH)) CommonSettings.SPU_sync_mode = 1;
					WritePrivateProfileInt("Sound", "SynchMode", CommonSettings.SPU_sync_mode, IniName);

					//save the synch method
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMETHOD_N)) CommonSettings.SPU_sync_method = 0;
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMETHOD_Z)) CommonSettings.SPU_sync_method = 1;
					if(IsDlgCheckboxChecked(hDlg,IDC_SYNCHMETHOD_P)) CommonSettings.SPU_sync_method = 2;
					WritePrivateProfileInt("Sound", "SynchMethod", CommonSettings.SPU_sync_method, IniName);

					{
						Lock lock;
						SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);
					}

					//write interpolation type
					CommonSettings.spuInterpolationMode = (SPUInterpolationMode)SendDlgItemMessage(hDlg, IDC_SPU_INTERPOLATION_CB, CB_GETCURSEL, 0, 0);
					WritePrivateProfileInt("Sound","SPUInterpolation",(int)CommonSettings.spuInterpolationMode, IniName);

					//write spu advanced
					CommonSettings.spu_advanced = IsDlgCheckboxChecked(hDlg,IDC_SPU_ADVANCED);
					WritePrivateProfileBool("Sound","SpuAdvanced",CommonSettings.spu_advanced,IniName);

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
	if(!(movieMode == MOVIEMODE_PLAY && movie_readonly))
	{
		NDS_Reset();
		if(movieMode == MOVIEMODE_INACTIVE && !emu_paused)
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
	UpdateHotkeyAssignment(CustomKeys.QuickPrintScreen, IDM_QUICK_PRINTSCREEN);
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
		UpdateHotkeyAssignment(CustomKeys.Save[i], IDM_STATE_SAVE_F10 + i);
	for(int i = 0; i < 10; i++)
		UpdateHotkeyAssignment(CustomKeys.Load[i], IDM_STATE_LOAD_F10 + i);
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
		#pragma warning( push )  
			//'type cast': pointer truncation from 'HWND' to 'int'
			//it's just a hash key. it's probably safe
			#pragma warning( disable : 4311 )  
			RequestAbortLuaScript((int)scriptHWnd, "terminated to restart because of a call to emu.openscript");
		#pragma warning( pop )  

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

void WIN_InstallGBACartridge()
{
	GBACartridge_RomPath = win32_GBA_cfgRomPath;
	GBACartridge_SRAMPath = Path::GetFileNameWithoutExt(win32_GBA_cfgRomPath) + "." + GBA_SRAM_FILE_EXT;
}

void SetStyle(u32 dws)
{
	//pokefan's suggestion, there are a number of ways we could do this.
	//i sort of like this because it is very visually indicative of being locked down
	DWORD ws = GetWindowLong(MainWindow->getHWnd(), GWL_STYLE);
	ws &= ~(WS_CAPTION | WS_POPUP | WS_THICKFRAME | WS_DLGFRAME);
	if (dws & DWS_LOCKDOWN)
		ws |= WS_POPUP | WS_DLGFRAME;
	else if (!(dws&DWS_FULLSCREEN))
		ws |= WS_CAPTION | WS_THICKFRAME;
	else if(dws & DWS_FS_WINDOW)
		ws |= WS_POPUPWINDOW;

	SetWindowLong(MainWindow->getHWnd(), GWL_STYLE, ws);


	if ((dws&DWS_FULLSCREEN) && !(dws&DWS_FS_MENU))
		SetMenu(MainWindow->getHWnd(), NULL);
	else
		SetMenu(MainWindow->getHWnd(), mainMenu);

	currWindowStyle = dws;
	HWND insertAfter = HWND_NOTOPMOST;
	if (dws & DWS_ALWAYSONTOP)
		insertAfter = HWND_TOPMOST;
	SetWindowPos(MainWindow->getHWnd(), insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

	gldisplay.setvsync(!!(GetStyle()&DWS_VSYNC));
	ddraw.vSync = GetStyle()&DWS_VSYNC;
}
