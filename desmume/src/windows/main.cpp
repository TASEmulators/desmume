/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright 2006 Theo Berkau

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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <Winuser.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include "CWindow.h"
#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../debug.h"
#include "../saves.h"
#include "../cflash.h"
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
#include "ConfigKeys.h"
#include "FirmConfig.h"
#include "AboutBox.h"
#include "OGLRender.h"
#include "../render3D.h"
#include "../gdbstub.h"
#include "colorctrl.h"
#include "console.h"

#include "snddx.h"

#include <ddraw.h>
//===================== Init DirectDraw
LPDIRECTDRAW7			lpDDraw=NULL;
LPDIRECTDRAWSURFACE7	lpPrimarySurface=NULL;
LPDIRECTDRAWSURFACE7	lpBackSurface=NULL;
DDSURFACEDESC2			ddsd;
LPDIRECTDRAWCLIPPER		lpDDClipPrimary=NULL;
LPDIRECTDRAWCLIPPER		lpDDClipBack=NULL;

/* The compact flash disk image file */
static const char *bad_glob_cflash_disk_image_file;
static char cflash_filename_buffer[512];

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

/*  Make the class name into a global variable  */
char SavName[MAX_PATH] = "";
char ImportSavName[MAX_PATH] = "";
char szClassName[ ] = "DeSmuME";
int romnum = 0;

DWORD threadID;

HWND hwnd;
HDC  hdc;
HINSTANCE hAppInst;  
RECT	MainWindowRect;

volatile BOOL execute = FALSE;
volatile BOOL paused = TRUE;
u32 glock = 0;

BOOL click = FALSE;

BOOL finished = FALSE;
BOOL romloaded = FALSE;

BOOL ForceRatio = TRUE;
float DefaultWidth;
float DefaultHeight;
float widthTradeOff;
float heightTradeOff;

HMENU menu;
HANDLE runthread_ready=INVALID_HANDLE_VALUE;
HANDLE runthread=INVALID_HANDLE_VALUE;

const DWORD DI_tabkey[48] = {DIK_0,DIK_1,DIK_2,DIK_3,DIK_4,DIK_5,DIK_6,DIK_7,DIK_8,DIK_9,DIK_A,DIK_B,DIK_C,
							DIK_D,DIK_E,DIK_F,DIK_G,DIK_H,DIK_I,DIK_J,DIK_K,DIK_L,DIK_M,DIK_N,DIK_O,DIK_P,
							DIK_Q,DIK_R,DIK_S,DIK_T,DIK_U,DIK_V,DIK_W,DIK_X,DIK_Y,DIK_Z,DIK_SPACE,DIK_UP,
							DIK_DOWN,DIK_LEFT,DIK_RIGHT,DIK_TAB,DIK_LSHIFT,DIK_DELETE,DIK_INSERT,DIK_HOME,
							DIK_END,DIK_RETURN};
DWORD ds_up,ds_down,ds_left,ds_right,ds_a,ds_b,ds_x,ds_y,ds_l,ds_r,ds_select,ds_start,ds_debug;
static char IniName[MAX_PATH];
int sndcoretype=SNDCORE_DIRECTX;
int sndbuffersize=735*4;
int sndvolume=100;

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
&SNDFile,
&SNDDIRECTX,
NULL
};

GPU3DInterface *core3DList[] = {
&gpu3DNull,
&gpu3Dgl,
};

int autoframeskipenab=1;
int frameskiprate=0;
int emu_paused = 0;
static int backupmemorytype=MC_TYPE_AUTODETECT;
static u32 backupmemorysize=1;

/* the firmware settings */
struct NDS_fw_config_data win_fw_config;


LRESULT CALLBACK SoundSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                      LPARAM lParam);

struct configured_features {
  u16 arm9_gdb_port;
  u16 arm7_gdb_port;

  const char *cflash_disk_image_file;
};

static void
init_configured_features( struct configured_features *config) {
  config->arm9_gdb_port = 0;
  config->arm7_gdb_port = 0;

  config->cflash_disk_image_file = NULL;
}


static int
fill_configured_features( struct configured_features *config, LPSTR lpszArgument) {
	int good_args = 0;
	LPTSTR cmd_line;
	LPWSTR *argv;
	int argc;

	argv = CommandLineToArgvW( GetCommandLineW(), &argc);

	if ( argv != NULL) {
		int i;
		good_args = 1;
		for ( i = 1; i < argc && good_args; i++) {
			if ( wcsncmp( argv[i], L"--arm9gdb=", 10) == 0) {
				wchar_t *end_char;
				unsigned long port_num = wcstoul( &argv[i][10], &end_char, 10);

				if ( port_num > 0 && port_num < 65536) {
					config->arm9_gdb_port = port_num;
				}
				else {
			        MessageBox(NULL,"ARM9 GDB stub port must be in the range 1 to 65535","Error",MB_OK);
					good_args = 0;
				}
			}
			else if ( wcsncmp( argv[i], L"--arm7gdb=", 10) == 0) {
				wchar_t *end_char;
				unsigned long port_num = wcstoul( &argv[i][10], &end_char, 10);

				if ( port_num > 0 && port_num < 65536) {
					config->arm7_gdb_port = port_num;
				}
				else {
                                  MessageBox(NULL,"ARM9 GDB stub port must be in the range 1 to 65535","Error",MB_OK);
					good_args = 0;
				}
			}
			else if ( wcsncmp( argv[i], L"--cflash=", 9) == 0) {
				if ( config->cflash_disk_image_file == NULL) {
					size_t convert_count = wcstombs( &cflash_filename_buffer[0], &argv[i][9], 512);
					if ( convert_count > 0) {
						config->cflash_disk_image_file = cflash_filename_buffer;
					}
				}
				else {
			        MessageBox(NULL,"CFlash disk image file already set","Error",MB_OK);
					good_args = 0;
				}
			}
		}
		LocalFree( argv);
	}

	return good_args;
}

// Rotation definitions
short GPU_rotation      = 0;
DWORD GPU_width         = 256;
DWORD GPU_height        = 192*2;
DWORD rotationstartscan = 192;
DWORD rotationscanlines = 192*2;

void SetWindowClientSize(HWND hwnd, int cx, int cy) //found at: http://blogs.msdn.com/oldnewthing/archive/2003/09/11/54885.aspx
{
    HMENU hmenu = GetMenu(hwnd);
    RECT rcWindow = { 0, 0, cx, cy };

    /*
     *  First convert the client rectangle to a window rectangle the
     *  menu-wrap-agnostic way.
     */
    AdjustWindowRect(&rcWindow, WS_CAPTION| WS_SYSMENU |WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, hmenu != NULL);

    /*
     *  If there is a menu, then check how much wrapping occurs
     *  when we set a window to the width specified by AdjustWindowRect
     *  and an infinite amount of height.  An infinite height allows
     *  us to see every single menu wrap.
     */
    if (hmenu) {
        RECT rcTemp = rcWindow;
        rcTemp.bottom = 0x7FFF;     /* "Infinite" height */
        SendMessage(hwnd, WM_NCCALCSIZE, FALSE, (LPARAM)&rcTemp);

        /*
         *  Adjust our previous calculation to compensate for menu
         *  wrapping.
         */
        rcWindow.bottom += rcTemp.top;
    }
    SetWindowPos(hwnd, NULL, 0, 0, rcWindow.right - rcWindow.left,
                 rcWindow.bottom - rcWindow.top, SWP_NOMOVE | SWP_NOZORDER);

	if (lpBackSurface!=NULL)
	{
		IDirectDrawSurface7_Release(lpBackSurface);
		memset(&ddsd, 0, sizeof(ddsd));
		ddsd.dwSize          = sizeof(ddsd);
		ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN;
		ddsd.dwWidth         = cx;
		ddsd.dwHeight        = cy;
		
		IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpBackSurface, NULL);
	}
}

void ScaleScreen(float factor)
{
	SetWindowPos(hwnd, NULL, 0, 0, widthTradeOff + DefaultWidth * factor,
                 heightTradeOff + DefaultHeight * factor, SWP_NOMOVE | SWP_NOZORDER);
	return;
}
 
void translateXY(s32 *x, s32*y)
{
  s32 tmp;
  switch(GPU_rotation)
  {
   case 90:
           tmp = *x;
           *x = *y;
           *y = 192*2 -tmp;
           break;
   case 180:
           *x = 256-*x;
           *y = 192*2-*y;
           break;
   case 270:
            tmp = *x;
            *x = 255-*y;
            *y = tmp;
            break;
 }
 *y-=192;
}
     
 // END Rotation definitions

void Input_Post()
{
	char txt[255];
	BOOL bPressed;
	HRESULT hr;
	WORD keys[13][3]={{ds_a,0xFFFE,0x01},{ds_b,0xFFFD,0x02},{ds_select,0xFFFB,0x04},{ds_start,0xFFF7,0x08},
					{ds_right,0xFFEF,0x10},{ds_left,0xFFDF,0x20},{ds_up,0xFFBF,0x40},{ds_down,0xFF7F,0x80},
					{ds_r,0xFEFF,0x100},{ds_l,0xFDFF,0x200},
					{ds_x,0xFFFE,0x01},{ds_y,0xFFFD,0x02},{ds_debug,0xFFFB,0x04}};
	int i;
	
	for (i=0; i<10; i++)
	{
		bPressed=FALSE;
		if (keys[i][0]<48)
			if (g_cDIBuf[DI_tabkey[keys[i][0]]]&0x80) bPressed=TRUE;
		if (keys[i][0]>=48)
			if (g_cDIBuf[208+keys[i][0]]&0x80) bPressed=TRUE;
		if (bPressed)
		{
				((u16 *)ARM9Mem.ARM9_REG)[0x130>>1]&=keys[i][1];
				((u16 *)MMU.ARM7_REG)[0x130>>1]&=keys[i][1];
		}
		else
		{
				((u16 *)ARM9Mem.ARM9_REG)[0x130>>1]|=keys[i][2];
				((u16 *)MMU.ARM7_REG)[0x130>>1]|=keys[i][2];
		}
	}
	for (i=10; i<13; i++)
	{
		bPressed=FALSE;
		if (keys[i][0]<48)
			if (g_cDIBuf[DI_tabkey[keys[i][0]]]&0x80) bPressed=TRUE;
		if (keys[i][0]>=48)
			if (g_cDIBuf[208+keys[i][0]]&0x80) bPressed=TRUE;
		if (bPressed)
				((u16 *)MMU.ARM7_REG)[0x136>>1]&=keys[i][1];
		else
				((u16 *)MMU.ARM7_REG)[0x136>>1]|=keys[i][2];
	}
}

int CreateDDrawBuffers()
{
	if (lpDDClipPrimary!=NULL) IDirectDraw7_Release(lpDDClipPrimary);
	if (lpPrimarySurface != NULL) IDirectDraw7_Release(lpPrimarySurface);
	if (lpBackSurface != NULL) IDirectDraw7_Release(lpBackSurface);

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	ddsd.dwFlags = DDSD_CAPS;
	if (IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpPrimarySurface, NULL) != DD_OK) return -1;
	
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize          = sizeof(ddsd);
	ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps  = DDSCAPS_OFFSCREENPLAIN;
	//MainWindowRect.right
	ddsd.dwWidth         = 256;
	ddsd.dwHeight        = 384;
	if (IDirectDraw7_CreateSurface(lpDDraw, &ddsd, &lpBackSurface, NULL) != DD_OK) return -2;

	if (IDirectDraw7_CreateClipper(lpDDraw, 0, &lpDDClipPrimary, NULL) != DD_OK) return -3;

	if (IDirectDrawClipper_SetHWnd(lpDDClipPrimary, 0, hwnd) != DD_OK) return -4;
	if (IDirectDrawSurface7_SetClipper(lpPrimarySurface, lpDDClipPrimary) != DD_OK) return -5;

	return 1;
}

DWORD WINAPI run( LPVOID lpParameter)
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

	 DDCAPS	hw_caps, sw_caps;

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

    NDS_3D_SetDriver (GPU3D_OPENGL);
	 
	if (!gpu3D->NDS_3D_Init ())
	{
		MessageBox(hwnd,"Unable to initialize openGL","Error",MB_OK);
		return -1;
    }

     QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
     QueryPerformanceCounter((LARGE_INTEGER *)&lastticks);
     OneFrameTime = freq / 60;

	 SetEvent(runthread_ready);

     while(!finished)
     {
          while(execute)
          {
               cycles = NDS_exec((560190<<1)-cycles,FALSE);
               SPU_Emulate();
			   Input_Process();
			   Input_Post();

               if (!skipnextframe)
               {
					int res;
					memset(&ddsd, 0, sizeof(ddsd));
					ddsd.dwSize = sizeof(ddsd);
					ddsd.dwFlags=DDSD_ALL;
					res=IDirectDrawSurface7_Lock(lpBackSurface,NULL,&ddsd,DDLOCK_WAIT, NULL);
					
					if (res == DD_OK)
					{
						char* buffer = (char*)ddsd.lpSurface;
						
						int i, j, sz=256*sizeof(u32);
						if (ddsd.ddpfPixelFormat.dwRGBBitCount>16)
						{
							u16 *tmpGPU_Screen_src=(u16*)GPU_screen;
							u32	tmpGPU_screen[98304];
							for(i=0; i<98304; i++)
							tmpGPU_screen[i]=	(((tmpGPU_Screen_src[i]>>10)&0x1F)<<3)|
												(((tmpGPU_Screen_src[i]>>5)&0x1F)<<11)|
												(((tmpGPU_Screen_src[i])&0x1F)<<19);
							switch (GPU_rotation)
							{
								case 0:
								{
									for (i = 0; i < 98304; i+=256)  //384*256
									{
										memcpy(buffer,tmpGPU_screen+i,sz);
										buffer += ddsd.lPitch;
									}
									break;
								}
								case 90:
								{
									u32 start;
									memset(buffer,0,384*ddsd.lPitch);
									for (j=0; j<256; j++)
									{
										start=98304+j;
										for (i=0; i<384; i++)
										{
											start-=256;
											((u32*)buffer)[i]=((u32 *)tmpGPU_screen)[start];
										}
										buffer += ddsd.lPitch;
									}
									break;
								}
								case 180:
								{
									u32 start=98300;
									for (j=0; j<384; j++)
									{
										for (i=0; i<256; i++, --start)
											((u32*)buffer)[i]=((u32 *)tmpGPU_screen)[start];
										buffer += ddsd.lPitch;
									}
									break;
								}
								case 270:
								{
									u32 start;
									memset(buffer,0,384*ddsd.lPitch);
									for (j=0; j<256; j++)
									{
										start=256-j;
										for (i=0; i<384; i++)
										{
											((u32*)buffer)[i]=((u32 *)tmpGPU_screen)[start];
											start+=256;
										}
										buffer += ddsd.lPitch;
									}
									break;
								}
							}
						}
						else
							 printlog("16bit depth color not supported");
						IDirectDrawSurface7_Unlock(lpBackSurface,(LPRECT)ddsd.lpSurface);

						if (IDirectDrawSurface7_Blt(lpPrimarySurface,&MainWindowRect,lpBackSurface,0, DDBLT_WAIT,0)==DDERR_SURFACELOST)
						{
							printlog("DirectDraw buffers is lost\n");
							if (IDirectDrawSurface7_Restore(lpPrimarySurface)==DD_OK)
								IDirectDrawSurface7_Restore(lpBackSurface);
						}
					}
					else
					{
						if (res==DDERR_SURFACELOST)
						{
							printlog("DirectDraw buffers is lost\n");
							if (IDirectDrawSurface7_Restore(lpPrimarySurface)==DD_OK)
								IDirectDrawSurface7_Restore(lpBackSurface);
						}
					}

                  fpsframecount++;
                  QueryPerformanceCounter((LARGE_INTEGER *)&curticks);
                  if(curticks >= fpsticks + freq) // TODO: print fps on screen in DDraw
                  {
                     fps = fpsframecount;
					 sprintf(txt,"(%d) DeSmuME v%s", fps, VERSION);
                     SetWindowText(hwnd, txt);
                     fpsframecount = 0;
                     QueryPerformanceCounter((LARGE_INTEGER *)&fpsticks);
                  }

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

                  if ((onesecondticks+diffticks) > (OneFrameTime * (u64)framecount) &&
                      framesskipped < 9)
                  {                     
                     // Skip the next frame
                     skipnextframe = 1;
 
                     // How many frames should we skip?
                     framestoskip = 1;
                  }
                  else if ((onesecondticks+diffticks) < (OneFrameTime * (u64)framecount))
                  {
                     // Check to see if we need to limit speed at all
                     for (;;)
                     {
                        QueryPerformanceCounter((LARGE_INTEGER *)&curticks);
                        diffticks = curticks-lastticks;
                        if ((onesecondticks+diffticks) >= (OneFrameTime * (u64)framecount))
                           break;
                     }
                  }

                  onesecondticks += diffticks;
                  lastticks = curticks;
               }
               else
               {
                  if (framestoskip < 1)
                     framestoskip += frameskiprate;
               }
          }
          paused = TRUE;
          Sleep(500);
     }
	if (lpDDClipPrimary!=NULL) IDirectDraw7_Release(lpDDClipPrimary);
	if (lpPrimarySurface != NULL) IDirectDraw7_Release(lpPrimarySurface);
	if (lpBackSurface != NULL) IDirectDraw7_Release(lpBackSurface);
    if (lpDDraw != NULL) IDirectDraw7_Release(lpDDraw);
     return 1;
}

void NDS_Pause()
{
   execute = FALSE;
   SPU_Pause(1);
   while (!paused) {}
   printlog("Paused\n");
}

void NDS_UnPause()
{
   paused = FALSE;
   execute = TRUE;
   SPU_Pause(0);
   printlog("Unpaused\n");
}

void StateSaveSlot(int num)
{
	NDS_Pause();
	savestate_slot(num);
	NDS_UnPause();
	printlog("Saved %i state\n",num);
}

void StateLoadSlot(int num)
{
	NDS_Pause();
	loadstate_slot(num);
	NDS_UnPause();
	printlog("Loaded %i state\n",num);
}

BOOL LoadROM(char * filename, const char *cflash_disk_image)
{
    NDS_Pause();
	if (strcmp(filename,"")!=0) printlog("Loading ROM: %s\n",filename);

    if (NDS_LoadROM(filename, backupmemorytype, backupmemorysize, cflash_disk_image) > 0)
       return TRUE;

    return FALSE;
}

/*
 * The thread handling functions needed by the GDB stub code.
 */
extern "C" void *
createThread_gdb( void (*thread_function)( void *data),
				 void *thread_data) {
	void *new_thread = CreateThread( NULL, 0,
		(LPTHREAD_START_ROUTINE)thread_function, thread_data,
		0, NULL);

	return new_thread;
}

extern "C" void
joinThread_gdb( void *thread_handle) {
}


void SetLanguage(int langid)
{
   switch(langid)
   {
      case 1:
         // French
         SetThreadLocale(MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH),
                         SORT_DEFAULT));          
         break;
      case 2:
         // Danish
         SetThreadLocale(MAKELCID(MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT),
                         SORT_DEFAULT));          
         break;
      case 0:
         // English
         SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                         SORT_DEFAULT));
         break;
      default: break;
         break;
   }
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
   for (i = IDC_LANGENGLISH; i < IDC_LANGFRENCH+1; i++)
      CheckMenuItem(menu, i, MF_BYCOMMAND | MF_UNCHECKED);

   CheckMenuItem(menu, id, MF_BYCOMMAND | MF_CHECKED);
}

void ChangeLanguage(int id)
{
   HMENU newmenu;

   SetLanguage(id);
   newmenu = LoadMenu(hAppInst, "MENU_PRINCIPAL");
   SetMenu(hwnd, newmenu);
   DestroyMenu(menu);
   menu = newmenu;   
}

void InitCustomControls()
{
	ColorCtrl_Register();
}

int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
#ifdef GDB_STUB
	gdbstub_handle_t arm9_gdb_stub;
	gdbstub_handle_t arm7_gdb_stub;
	struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
	struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
	struct armcpu_ctrl_iface *arm9_ctrl_iface;
	struct armcpu_ctrl_iface *arm7_ctrl_iface;
#endif
	struct configured_features my_config;


	MSG messages;            /* Here messages to the application are saved */
    char text[80];
    cwindow_struct MainWindow;
    HACCEL hAccel;
    hAppInst=hThisInstance;

	InitCustomControls();

	OpenConsole();			// Init debug console

	/* default the firmware settings, they may get changed later */
	NDS_FillDefaultFirmwareConfigData( &win_fw_config);

    InitializeCriticalSection(&section);

    GetINIPath(IniName, MAX_PATH);
    GetPrivateProfileString("General", "Language", "-1", text, 80, IniName);
    SetLanguage(atoi(text));

    sprintf(text, "DeSmuME v%s", VERSION);

	init_configured_features( &my_config);
	if ( !fill_configured_features( &my_config, lpszArgument)) {
        MessageBox(NULL,"Unable to parse command line arguments","Error",MB_OK);
        return 0;
	}
	bad_glob_cflash_disk_image_file = my_config.cflash_disk_image_file;

    hAccel = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_MAIN_ACCEL));

#ifdef DEBUG
    LogStart();
#endif

    if (CWindow_Init(&MainWindow, hThisInstance, szClassName, text,
                     WS_CAPTION| WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                     256, 384, WindowProcedure) != 0)
    {
        MessageBox(NULL,"Unable to create main window","Error",MB_OK);
        return 0;
    }

    hwnd = MainWindow.hwnd;
    menu = LoadMenu(hThisInstance, "MENU_PRINCIPAL");
    SetMenu(hwnd, menu);
	CheckMenuItem(menu, IDC_FORCERATIO, MF_BYCOMMAND | MF_CHECKED);
    hdc = GetDC(hwnd);
    DragAcceptFiles(hwnd, TRUE);
    
    /* Make the window visible on the screen */
    CWindow_Show(&MainWindow);

    InitMemViewBox();
    InitDesViewBox();
    InitTileViewBox();
    InitOAMViewBox();
    printlog("Init NDS\n");
#ifdef GDB_STUB
	if ( my_config.arm9_gdb_port != 0) {
		arm9_gdb_stub = createStub_gdb( my_config.arm9_gdb_port,
			&arm9_memio, &arm9_direct_memory_iface);

		if ( arm9_gdb_stub == NULL) {
	       MessageBox(hwnd,"Failed to create ARM9 gdbstub","Error",MB_OK);
		   return -1;
		}
	}
	if ( my_config.arm7_gdb_port != 0) {
		arm7_gdb_stub = createStub_gdb( my_config.arm7_gdb_port,
                                    &arm7_memio,
                                    &arm7_base_memory_iface);

		if ( arm7_gdb_stub == NULL) {
	       MessageBox(hwnd,"Failed to create ARM7 gdbstub","Error",MB_OK);
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
	if ( my_config.arm9_gdb_port != 0) {
		activateStub_gdb( arm9_gdb_stub, arm9_ctrl_iface);
	}
	if ( my_config.arm7_gdb_port != 0) {
		activateStub_gdb( arm7_gdb_stub, arm7_ctrl_iface);
	}
#endif
    GetPrivateProfileString("General", "Language", "0", text, 80, IniName);
    CheckLanguage(IDC_LANGENGLISH+atoi(text));

    GetPrivateProfileString("Video", "FrameSkip", "AUTO", text, 80, IniName);

    if (strcmp(text, "AUTO") == 0)
    {
       autoframeskipenab=1;
       frameskiprate=0;
       CheckMenuItem(menu, IDC_FRAMESKIPAUTO, MF_BYCOMMAND | MF_CHECKED);
    }
    else
    {
       autoframeskipenab=0;
       frameskiprate=atoi(text);
       CheckMenuItem(menu, frameskiprate + IDC_FRAMESKIP0, MF_BYCOMMAND | MF_CHECKED);
    }

#ifdef BETA_VERSION
	EnableMenuItem (menu, IDM_SUBMITBUGREPORT, MF_GRAYED);
#endif
	printlog("Init sound core\n");
    sndcoretype = GetPrivateProfileInt("Sound","SoundCore", SNDCORE_DIRECTX, IniName);
    sndbuffersize = GetPrivateProfileInt("Sound","SoundBufferSize", 735 * 4, IniName);

    if (SPU_ChangeSoundCore(sndcoretype, sndbuffersize) != 0)
    {
       MessageBox(hwnd,"Unable to initialize DirectSound","Error",MB_OK);
       return -1;
    }

	sndvolume = GetPrivateProfileInt("Sound","Volume",100, IniName);
    SPU_SetVolume(sndvolume);

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

	/* Create the dummy firmware */
	NDS_CreateDummyFirmware( &win_fw_config);

	runthread_ready = CreateEvent(NULL,TRUE,FALSE,0);
    runthread = CreateThread(NULL, 0, run, NULL, 0, &threadID);

	//wait for the run thread to signal that it is initialized and ready to run
	WaitForSingleObject(runthread_ready,INFINITE);
	

    // Make sure any quotes from lpszArgument are removed
    if (lpszArgument[0] == '\"')
       sscanf(lpszArgument, "\"%[^\"]\"", lpszArgument);

    if(LoadROM(lpszArgument, bad_glob_cflash_disk_image_file))
    {
       EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
       EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
       EnableMenuItem(menu, IDM_RESET, MF_ENABLED);
       EnableMenuItem(menu, IDM_GAME_INFO, MF_ENABLED);
       EnableMenuItem(menu, IDM_IMPORTBACKUPMEMORY, MF_ENABLED);
       romloaded = TRUE;
       NDS_UnPause();
    }
    else
    {
       EnableMenuItem(menu, IDM_EXEC, MF_ENABLED);
       EnableMenuItem(menu, IDM_PAUSE, MF_GRAYED);
       EnableMenuItem(menu, IDM_RESET, MF_GRAYED);
       EnableMenuItem(menu, IDM_GAME_INFO, MF_GRAYED);
       EnableMenuItem(menu, IDM_IMPORTBACKUPMEMORY, MF_GRAYED);
    }

    CheckMenuItem(menu, IDC_SAVETYPE1, MF_BYCOMMAND | MF_CHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE2, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE3, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE4, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE5, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE6, MF_BYCOMMAND | MF_UNCHECKED);
    
    CheckMenuItem(menu, IDC_ROTATE0, MF_BYCOMMAND | MF_CHECKED);

    while (GetMessage (&messages, NULL, 0, 0))
    {
       if (TranslateAccelerator(hwnd, hAccel, &messages) == 0)
       {
          // Translate virtual-key messages into character messages
          TranslateMessage(&messages);
          // Send message to WindowProcedure 
          DispatchMessage(&messages);
       }  
    }
	{
	HRESULT hr=Input_DeInit();
#ifdef DEBUG 
	if(FAILED(hr)) LOG("DirectInput deinit failed (0x%08X)\n",hr);
	else LOG("DirectInput deinit\n");
#endif
	}
#ifdef DEBUG
    LogStop();
#endif
	CloseConsole();
    /* The program return-value is 0 - The value that PostQuitMessage() gave */
    return messages.wParam;
}

void GetRect(HWND hwnd)
{
	POINT ptClient;
	RECT rc;

	GetClientRect(hwnd,&rc);
	ptClient.x=rc.left;
	ptClient.y=rc.top;
	ClientToScreen(hwnd,&ptClient);
	MainWindowRect.left=ptClient.x;
	MainWindowRect.top=ptClient.y;
	ptClient.x=rc.right;
	ptClient.y=rc.bottom;
	ClientToScreen(hwnd,&ptClient);
	MainWindowRect.right=ptClient.x;
	MainWindowRect.bottom=ptClient.y;
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
	static int tmp_execute;
    switch (message)                  // handle the messages
    {
		/*case WM_ENTERMENULOOP:
			{
				if (execute)
				{
					NDS_Pause();
					tmp_execute=2;
				} else tmp_execute=-1;
				return 0;
			}
		case WM_EXITMENULOOP:
			{
				if (tmp_execute==2) NDS_UnPause();
				return 0;
			}*/

        case WM_CREATE:
	     {
	     	RECT clientSize, fullSize;
             	HRESULT hr=Input_Init(hwnd);
#ifdef DEBUG
				if(FAILED(hr)) LOG("DirectInput init failed (0x%08x)\n",hr);
				else LOG("DirectInput init\n");
#endif
	     	GetClientRect(hwnd, &clientSize);
			GetWindowRect(hwnd, &fullSize);
	     	DefaultWidth = clientSize.right - clientSize.left;
			DefaultHeight = clientSize.bottom - clientSize.top;
			widthTradeOff = (fullSize.right - fullSize.left) - (clientSize.right - clientSize.left);
			heightTradeOff = (fullSize.bottom - fullSize.top) - (clientSize.bottom - clientSize.top);
            return 0;
	     }
        case WM_DESTROY:
			{
             NDS_Pause();
             finished = TRUE;

             if (runthread != INVALID_HANDLE_VALUE)
             {
                if (WaitForSingleObject(runthread,INFINITE) == WAIT_TIMEOUT)
                {
                   // Couldn't close thread cleanly
                   TerminateThread(runthread,0);
                }
                CloseHandle(runthread);
             }

             NDS_DeInit();

             PostQuitMessage (0);       // send a WM_QUIT to the message queue 
             return 0;
			}
		case WM_MOVE:
				GetRect(hwnd);
				return 0;
		case WM_SIZE:
    			if (ForceRatio) {
					RECT fullSize;
					GetWindowRect(hwnd, &fullSize);
					ScaleScreen((fullSize.bottom - fullSize.top - heightTradeOff) / DefaultHeight);
				}
				GetRect(hwnd);
				return 0;
        case WM_CLOSE:
			{
             NDS_Pause();
             finished = TRUE;

             if (runthread != INVALID_HANDLE_VALUE)
             {
                if (WaitForSingleObject(runthread,INFINITE) == WAIT_TIMEOUT)
                {
                   // Couldn't close thread cleanly
                   TerminateThread(runthread,0);
                }
                CloseHandle(runthread);
             }

             NDS_DeInit();
             PostMessage(hwnd, WM_QUIT, 0, 0);
             return 0;
			}
        case WM_DROPFILES:
             {
                  char filename[MAX_PATH] = "";
                  DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
                  DragFinish((HDROP)wParam);
                  if(LoadROM(filename, bad_glob_cflash_disk_image_file))
                  {
                     EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
                     EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
                     EnableMenuItem(menu, IDM_RESET, MF_ENABLED);
                     EnableMenuItem(menu, IDM_GAME_INFO, MF_ENABLED);
                     EnableMenuItem(menu, IDM_IMPORTBACKUPMEMORY, MF_ENABLED);
                     romloaded = TRUE;
                     NDS_UnPause();
                  }
             }
             return 0;
        case WM_MOUSEMOVE:
                  if (wParam & MK_LBUTTON)
                  {
					   RECT r ;
                       s32 x = (s32)((s16)LOWORD(lParam));
                       s32 y = (s32)((s16)HIWORD(lParam));
						GetClientRect(hwnd,&r) ;
						/* translate from scaling (screen reoltution to 256x384 or 512x192) */
					   switch (GPU_rotation)
						{
							case 0:
							case 180:
								x = (x*256) / (r.right - r.left) ;
								y = (y*384) / (r.bottom - r.top) ;
								break ;
							case 90:
							case 270:
								x = (x*512) / (r.right - r.left) ;
								y = (y*192) / (r.bottom - r.top) ;
								break ;
						}
						/* translate for rotation */
                       if (GPU_rotation != 0)
                          translateXY(&x,&y);
                       else 
                          y-=192;
                       if(x<0) x = 0; else if(x>255) x = 255;
                       if(y<0) y = 0; else if(y>192) y = 192;
                       NDS_setTouchPos(x, y);
                       return 0;
                  }
				NDS_releasTouch();
             return 0;
        case WM_LBUTTONDOWN:
				if(HIWORD(lParam)>=192)
				{
						   RECT r ;
					s32 x = (s32)((s16)LOWORD(lParam));
					s32 y = (s32)((s16)HIWORD(lParam));
							GetClientRect(hwnd,&r) ;
							/* translate from scaling (screen reoltution to 256x384 or 512x192) */
						   switch (GPU_rotation)
							{
								case 0:
								case 180:
									x = (x*256) / (r.right - r.left) ;
									y = (y*384) / (r.bottom - r.top) ;
									break ;
								case 90:
								case 270:
									x = (x*512) / (r.right - r.left) ;
									y = (y*192) / (r.bottom - r.top) ;
									break ;
							}
							/* translate for rotation */
					if (GPU_rotation != 0)
					   translateXY(&x,&y);
					else
					  y-=192;
					if(y>=0)
					{
						 SetCapture(hwnd);
						 if(x<0) x = 0; else if(x>255) x = 255;
						 if(y<0) y = 0; else if(y>192) y = 192;
						 NDS_setTouchPos(x, y);
						 click = TRUE;
					}
				 }
             return 0;
        case WM_LBUTTONUP:
				if(click)
					  ReleaseCapture();
				NDS_releasTouch();
             return 0;
		case WM_COMMAND:
             switch(LOWORD(wParam))
             {
                  case IDM_OPEN:
                       {
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
                            
                            if(!GetOpenFileName(&ofn))
                            {
                                 if (romloaded)
                                    NDS_UnPause(); //Restart emulation if no new rom chosen
                                 return 0;
                            }
                            
                            LOG("%s\r\n", filename);

                            if(LoadROM(filename, bad_glob_cflash_disk_image_file))
                            {
                               EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
                               EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
                               EnableMenuItem(menu, IDM_RESET, MF_ENABLED);
                               EnableMenuItem(menu, IDM_GAME_INFO, MF_ENABLED);
                               EnableMenuItem(menu, IDM_IMPORTBACKUPMEMORY, MF_ENABLED);
                               romloaded = TRUE;
                               NDS_UnPause();
                            }
                       }
                  return 0;
                  case IDM_PRINTSCREEN:
                       {
                            OPENFILENAME ofn;
                            char filename[MAX_PATH] = "";
                            ZeroMemory(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = hwnd;
                            ofn.lpstrFilter = "Bmp file (*.bmp)\0*.bmp\0Any file (*.*)\0*.*\0\0";
                            ofn.nFilterIndex = 1;
                            ofn.lpstrFile =  filename;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.lpstrDefExt = "bmp";
                            ofn.Flags = OFN_OVERWRITEPROMPT;
                            GetSaveFileName(&ofn);
                            NDS_WriteBMP(filename);
                       }
                  return 0;
                  case IDM_QUICK_PRINTSCREEN:
                       {
                          NDS_WriteBMP("./printscreen.bmp");
                       }
                  return 0;
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
                            NDS_UnPause();
                       }
                  return 0;
                  case IDM_STATE_SAVE_F1:
                     StateSaveSlot(1);
                     return 0;
                  case IDM_STATE_SAVE_F2:
                     StateSaveSlot(2);
                     return 0;
                  case IDM_STATE_SAVE_F3:
                     StateSaveSlot(3);
                     return 0;
                  case IDM_STATE_SAVE_F4:
                     StateSaveSlot(4);
                     return 0;
                  case IDM_STATE_SAVE_F5:
                     StateSaveSlot(5);
                     return 0;
                  case IDM_STATE_SAVE_F6:
                     StateSaveSlot(6);
                     return 0;
                  case IDM_STATE_SAVE_F7:
                     StateSaveSlot(7);
                     return 0;
                  case IDM_STATE_SAVE_F8:
                     StateSaveSlot(8);
                     return 0;
                  case IDM_STATE_SAVE_F9:
                     StateSaveSlot(9);
                     return 0;
                  case IDM_STATE_SAVE_F10:
                     StateSaveSlot(10);
                     return 0;
                  case IDM_STATE_LOAD_F1:
                     StateLoadSlot(1);
                     return 0;
                  case IDM_STATE_LOAD_F2:
                     StateLoadSlot(2);
                     return 0;
                  case IDM_STATE_LOAD_F3:
                     StateLoadSlot(3);
                     return 0;
                  case IDM_STATE_LOAD_F4:
                     StateLoadSlot(4);
                     return 0;
                  case IDM_STATE_LOAD_F5:
                     StateLoadSlot(5);
                     return 0;
                  case IDM_STATE_LOAD_F6:
                     StateLoadSlot(6);
                     return 0;
                  case IDM_STATE_LOAD_F7:
                     StateLoadSlot(7);
                     return 0;
                  case IDM_STATE_LOAD_F8:
                     StateLoadSlot(8);
                     return 0;
                  case IDM_STATE_LOAD_F9:
                     StateLoadSlot(9);
                     return 0;
                  case IDM_STATE_LOAD_F10:
                     StateLoadSlot(10);
                     return 0;
                  case IDM_IMPORTBACKUPMEMORY:
                  {
                     OPENFILENAME ofn;
                     NDS_Pause();
                     ZeroMemory(&ofn, sizeof(ofn));
                     ofn.lStructSize = sizeof(ofn);
                     ofn.hwndOwner = hwnd;
                     ofn.lpstrFilter = "All supported types\0*.duc;*.sav\0Action Replay DS Save (*.duc)\0*.duc\0DS-Xtreme Save (*.sav)\0*.sav\0\0";
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
                  case IDM_SOUNDSETTINGS:
                  {
                      DialogBox(GetModuleHandle(NULL), "SoundSettingsDlg", hwnd, (DLGPROC)SoundSettingsDlgProc);                    
                  }
                  return 0;
                  case IDM_GAME_INFO:
                       {
                            CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GAME_INFO), hwnd, GinfoView_Proc);
                       }
                  return 0;
                  case IDM_PAL:
                       {
                            palview_struct *PalView;

                            if ((PalView = PalView_Init(hAppInst, HWND_DESKTOP)) != NULL)
                               CWindow_Show(PalView);
                       }
                  return 0;
                  case IDM_TILE:
                       {
                            tileview_struct *TileView;

                            if ((TileView = TileView_Init(hAppInst, HWND_DESKTOP)) != NULL)
                               CWindow_Show(TileView);
                       }
                  return 0;
                  case IDM_IOREG:
                       {
                            cwindow_struct *IoregView;

                            if ((IoregView = (cwindow_struct*)malloc(sizeof(cwindow_struct))) == NULL)
                               return 0;

                            if (CWindow_Init2(IoregView, hAppInst, HWND_DESKTOP, "IO REG VIEW", IDD_IO_REG, IoregView_Proc) == 0)
                            {
                               CWindow_AddToRefreshList(IoregView);
                               CWindow_Show(IoregView);
                            }
                       }
                  return 0;
                  case IDM_QUIT:
                       PostMessage(hwnd, WM_QUIT, 0, 0);
                  return 0;
                  case IDM_MEMORY:
                       {
                            memview_struct *MemView;

                            if ((MemView = MemView_Init(hAppInst, HWND_DESKTOP, "ARM7 memory viewer", 1)) != NULL)
                               CWindow_Show(MemView);

                            if ((MemView = MemView_Init(hAppInst, HWND_DESKTOP, "ARM9 memory viewer", 0)) != NULL)
                               CWindow_Show(MemView);
                       }
                  return 0;
                  case IDM_DISASSEMBLER:
                       {
                            disview_struct *DisView;

                            if ((DisView = DisView_Init(hAppInst, HWND_DESKTOP, "ARM7 Disassembler", &NDS_ARM7)) != NULL)
                               CWindow_Show(DisView);

                            if ((DisView = DisView_Init(hAppInst, HWND_DESKTOP, "ARM9 Disassembler", &NDS_ARM9)) != NULL)
                               CWindow_Show(DisView);
                        }
                  return 0;
                  case IDM_MAP:
                       {
                            mapview_struct *MapView;

                            if ((MapView = MapView_Init(hAppInst, HWND_DESKTOP)) != NULL)
                            {
                               CWindow_AddToRefreshList(MapView);
                               CWindow_Show(MapView);
                            }
                       }
                  return 0;
                  case IDM_OAM:
                       {
						   oamview_struct *OamView;

                            if ((OamView = OamView_Init(hAppInst, HWND_DESKTOP)) != NULL)
                            {
                               CWindow_AddToRefreshList(OamView);
                               CWindow_Show(OamView);
                            }
                       }
                  return 0;

				  case IDM_MATRIX_VIEWER:
                       {
                            matrixview_struct *MatrixView;

                            if ((MatrixView = MatrixView_Init(hAppInst, HWND_DESKTOP)) != NULL)
                            {
                               CWindow_Show(MatrixView);
                            }
                       }
                  return 0;

				  case IDM_LIGHT_VIEWER:
                       {
                            lightview_struct *LightView;

                            if ((LightView = LightView_Init(hAppInst, HWND_DESKTOP)) != NULL)
                            {
                               CWindow_Show(LightView);
                            }
                       }
                  return 0;

				  case IDM_MBG0 : 
                       if(MainScreen.gpu->dispBG[0])
                       {
                            GPU_remove(MainScreen.gpu, 0);
                            CheckMenuItem(menu, IDM_MBG0, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(MainScreen.gpu, 0);
                            CheckMenuItem(menu, IDM_MBG0, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_MBG1 : 
                       if(MainScreen.gpu->dispBG[1])
                       {
                            GPU_remove(MainScreen.gpu, 1);
                            CheckMenuItem(menu, IDM_MBG1, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(MainScreen.gpu, 1);
                            CheckMenuItem(menu, IDM_MBG1, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_MBG2 : 
                       if(MainScreen.gpu->dispBG[2])
                       {
                            GPU_remove(MainScreen.gpu, 2);
                            CheckMenuItem(menu, IDM_MBG2, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(MainScreen.gpu, 2);
                            CheckMenuItem(menu, IDM_MBG2, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_MBG3 : 
                       if(MainScreen.gpu->dispBG[3])
                       {
                            GPU_remove(MainScreen.gpu, 3);
                            CheckMenuItem(menu, IDM_MBG3, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(MainScreen.gpu, 3);
                            CheckMenuItem(menu, IDM_MBG3, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_SBG0 : 
                       if(SubScreen.gpu->dispBG[0])
                       {
                            GPU_remove(SubScreen.gpu, 0);
                            CheckMenuItem(menu, IDM_SBG0, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(SubScreen.gpu, 0);
                            CheckMenuItem(menu, IDM_SBG0, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_SBG1 : 
                       if(SubScreen.gpu->dispBG[1])
                       {
                            GPU_remove(SubScreen.gpu, 1);
                            CheckMenuItem(menu, IDM_SBG1, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(SubScreen.gpu, 1);
                            CheckMenuItem(menu, IDM_SBG1, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_SBG2 : 
                       if(SubScreen.gpu->dispBG[2])
                       {
                            GPU_remove(SubScreen.gpu, 2);
                            CheckMenuItem(menu, IDM_SBG2, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(SubScreen.gpu, 2);
                            CheckMenuItem(menu, IDM_SBG2, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  case IDM_SBG3 : 
                       if(SubScreen.gpu->dispBG[3])
                       {
                            GPU_remove(SubScreen.gpu, 3);
                            CheckMenuItem(menu, IDM_SBG3, MF_BYCOMMAND | MF_UNCHECKED);
                       }
                       else
                       {
                            GPU_addBack(SubScreen.gpu, 3);
                            CheckMenuItem(menu, IDM_SBG3, MF_BYCOMMAND | MF_CHECKED);
                       }
                       return 0;
                  //case IDM_EXEC:
                  //     EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
                  //     EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
                  //     NDS_UnPause();
				  // 	return 0;
				  case ACCEL_SPACEBAR:
				  case IDM_PAUSE:
					  if (emu_paused) NDS_UnPause();
					  else NDS_Pause();
					  emu_paused ^= 1;
					  CheckMenuItem(menu, IDM_PAUSE, emu_paused ? MF_CHECKED : MF_UNCHECKED);
					  //	EnableMenuItem(menu, IDM_EXEC, MF_ENABLED);
					  //  EnableMenuItem(menu, IDM_PAUSE, MF_GRAYED);
                  return 0;
                  
                  #define saver(one,two,three,four,five, six) \
                  CheckMenuItem(menu, IDC_SAVETYPE1, MF_BYCOMMAND | one); \
                  CheckMenuItem(menu, IDC_SAVETYPE2, MF_BYCOMMAND | two); \
                  CheckMenuItem(menu, IDC_SAVETYPE3, MF_BYCOMMAND | three); \
                  CheckMenuItem(menu, IDC_SAVETYPE4, MF_BYCOMMAND | four); \
                  CheckMenuItem(menu, IDC_SAVETYPE5, MF_BYCOMMAND | five); \
                  CheckMenuItem(menu, IDC_SAVETYPE6, MF_BYCOMMAND | six);
                  
                  case IDC_SAVETYPE1:
                       saver(MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mmu_select_savetype(0,&backupmemorytype,&backupmemorysize);
                  return 0;
                  case IDC_SAVETYPE2:
                       saver(MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mmu_select_savetype(1,&backupmemorytype,&backupmemorysize);
                  return 0;   
                  case IDC_SAVETYPE3:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mmu_select_savetype(2,&backupmemorytype,&backupmemorysize);
                  return 0;   
                  case IDC_SAVETYPE4:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mmu_select_savetype(3,&backupmemorytype,&backupmemorysize);
                  return 0;
                  case IDC_SAVETYPE5:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED);
                       mmu_select_savetype(4,&backupmemorytype,&backupmemorysize);
                  return 0; 
                  case IDC_SAVETYPE6:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED);
                       mmu_select_savetype(5,&backupmemorytype,&backupmemorysize);
                  return 0; 
                  
                  case IDM_RESET:
                       NDS_Reset();
                  return 0;
                  case IDM_CONFIG:
                       {
                            cwindow_struct ConfigView;

                            if (CWindow_Init2(&ConfigView, hAppInst, HWND_DESKTOP, "Configure Controls", IDD_CONFIG, ConfigView_Proc) == 0)
                               CWindow_Show(&ConfigView);

                       }
                  return 0;
                  case IDM_FIRMSETTINGS:
                       {
                            cwindow_struct FirmConfig;

                            if (CWindow_Init2(&FirmConfig, hAppInst, HWND_DESKTOP,
									"Configure Controls", IDD_FIRMSETTINGS, FirmConfig_Proc) == 0)
                               CWindow_Show(&FirmConfig);

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

                       CheckMenuItem(menu, IDC_FRAMESKIPAUTO, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP0, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP1, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP2, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP3, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP4, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP5, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP6, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP7, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP8, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_FRAMESKIP9, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, LOWORD(wParam), MF_BYCOMMAND | MF_CHECKED);
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
                  case IDC_LANGDANISH:
                     SaveLanguage(1);
                     ChangeLanguage(2);
                     CheckLanguage(LOWORD(wParam));
                  return 0;
                  case IDM_WEBSITE:
                       ShellExecute(NULL, "open", "http://desmume.sourceforge.net", NULL, NULL, SW_SHOWNORMAL);
                  return 0;

                  case IDM_FORUM:
                       ShellExecute(NULL, "open", "http://forums.desmume.org/index.php", NULL, NULL, SW_SHOWNORMAL);
                  return 0;

				  case IDM_ABOUT:
				  {
					  cwindow_struct aboutBox;

					  if (CWindow_Init2(&aboutBox, hAppInst, HWND_DESKTOP, "About desmume...", IDD_ABOUT_BOX, AboutBox_Proc) == 0)
							CWindow_Show(&aboutBox);

					  break;
				  }

#ifndef BETA_VERSION
                  case IDM_SUBMITBUGREPORT:
                       ShellExecute(NULL, "open", "http://sourceforge.net/tracker/?func=add&group_id=164579&atid=832291", NULL, NULL, SW_SHOWNORMAL);
                  return 0;
#endif

                  case IDC_ROTATE0:
                       GPU_rotation = 0;
                       GPU_width    = 256;
                       GPU_height   = 192*2;
                       rotationstartscan = 192;
                       rotationscanlines = 192*2;
                       SetWindowClientSize(hwnd, GPU_width, GPU_height);
                       CheckMenuItem(menu, IDC_ROTATE0,   MF_BYCOMMAND | MF_CHECKED);
                       CheckMenuItem(menu, IDC_ROTATE90,  MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE180, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE270, MF_BYCOMMAND | MF_UNCHECKED);
                  return 0;
                  case IDC_ROTATE90:
                       GPU_rotation = 90;
                       GPU_width    = 192*2;
                       GPU_height   = 256;
                       rotationstartscan = 0;
                       rotationscanlines = 256;
                       SetWindowClientSize(hwnd, GPU_width, GPU_height);
                       CheckMenuItem(menu, IDC_ROTATE0,   MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE90,  MF_BYCOMMAND | MF_CHECKED);
                       CheckMenuItem(menu, IDC_ROTATE180, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE270, MF_BYCOMMAND | MF_UNCHECKED);
                  return 0;
                  case IDC_ROTATE180:
                       GPU_rotation = 180;
                       GPU_width    = 256;
                       GPU_height   = 192*2;
                       rotationstartscan = 0;
                       rotationscanlines = 192*2;
                       SetWindowClientSize(hwnd, GPU_width, GPU_height);
                       CheckMenuItem(menu, IDC_ROTATE0,   MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE90,  MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE180, MF_BYCOMMAND | MF_CHECKED);
                       CheckMenuItem(menu, IDC_ROTATE270, MF_BYCOMMAND | MF_UNCHECKED);
                  return 0;
                  case IDC_ROTATE270:
                       GPU_rotation = 270;
                       GPU_width    = 192*2;
                       GPU_height   = 256;
                       rotationstartscan = 0;
                       rotationscanlines = 256;
                       SetWindowClientSize(hwnd, GPU_width, GPU_height);
                       CheckMenuItem(menu, IDC_ROTATE0,   MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE90,  MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE180, MF_BYCOMMAND | MF_UNCHECKED);
                       CheckMenuItem(menu, IDC_ROTATE270, MF_BYCOMMAND | MF_CHECKED);
                  return 0;
        case IDC_WINDOW1X:
			ScaleScreen(1);
			break;
		case IDC_WINDOW2X:
			ScaleScreen(2);
			break;
		case IDC_WINDOW3X:
			ScaleScreen(3);
			break;
		case IDC_WINDOW4X:
			ScaleScreen(4);
			break;
		case IDC_FORCERATIO:
			if (ForceRatio) {
				CheckMenuItem(menu, IDC_FORCERATIO, MF_BYCOMMAND | MF_UNCHECKED);
				ForceRatio = FALSE;
			}
			else {
				RECT fullSize;
				GetWindowRect(hwnd, &fullSize);
				ScaleScreen((fullSize.bottom - fullSize.top - heightTradeOff) / DefaultHeight);
				CheckMenuItem(menu, IDC_FORCERATIO, MF_BYCOMMAND | MF_CHECKED);
				ForceRatio = TRUE;
			}
			break;

        }
             return 0;
        default:                      /* for messages that we don't deal with */
            return DefWindowProc (hwnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK SoundSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                      LPARAM lParam)
{
   static int timerid=0;
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

               NDS_Pause();

               // Write Sound core type
               sndcoretype = SNDCoreList[SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_GETCURSEL, 0, 0)]->id;
               sprintf(tempstr, "%d", sndcoretype);
               WritePrivateProfileString("Sound", "SoundCore", tempstr, IniName);

               // Write Sound Buffer size
               GetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr, 6);
               sscanf(tempstr, "%d", &sndbuffersize);
               WritePrivateProfileString("Sound", "SoundBufferSize", tempstr, IniName);

               SPU_ChangeSoundCore(sndcoretype, sndbuffersize);

               // Write Volume
               sndvolume = SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_GETPOS, 0, 0);
               sprintf(tempstr, "%d", sndvolume);
               WritePrivateProfileString("Sound", "Volume", tempstr, IniName);
               SPU_SetVolume(sndvolume);
               NDS_UnPause();

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


