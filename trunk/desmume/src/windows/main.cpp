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

#include "windriver.h"
#include <algorithm>
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
#include "inputdx.h"
#include "FirmConfig.h"
#include "AboutBox.h"
#include "OGLRender.h"
#include "../gfx3d.h"
#include "../render3D.h"
#include "../gdbstub.h"
#include "colorctrl.h"
#include "console.h"
#include "throttle.h"

#include "../common.h"

#include "snddx.h"

#include "directx/ddraw.h"

#define GPU3D_NULL 0
#define GPU3D_OPENGL 1

//------todo move these into a generic driver api
bool DRV_AviBegin(const char* fname);
void DRV_AviEnd();
void DRV_AviSoundUpdate(void* soundData, int soundLen);
bool DRV_AviIsRecording();
void DRV_AviVideoUpdate(const u16* buffer);
//------

CRITICAL_SECTION win_sync;
volatile int win_sound_samplecounter = 0;

//===================== DirectDraw vars
LPDIRECTDRAW7			lpDDraw=NULL;
LPDIRECTDRAWSURFACE7	lpPrimarySurface=NULL;
LPDIRECTDRAWSURFACE7	lpBackSurface=NULL;
DDSURFACEDESC2			ddsd;
LPDIRECTDRAWCLIPPER		lpDDClipPrimary=NULL;
LPDIRECTDRAWCLIPPER		lpDDClipBack=NULL;

//===================== Input vars
INPUTCLASS				*input = NULL;

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

WINCLASS	*MainWindow=NULL;

//HWND hwnd;
//HDC  hdc;
HACCEL hAccel;
HINSTANCE hAppInst;
RECT	MainWindowRect;

//=========================== view tools
TOOLSCLASS	*ViewDisasm_ARM7 = NULL;
TOOLSCLASS	*ViewDisasm_ARM9 = NULL;
TOOLSCLASS	*ViewMem_ARM7 = NULL;
TOOLSCLASS	*ViewMem_ARM9 = NULL;
TOOLSCLASS	*ViewRegisters = NULL;
TOOLSCLASS	*ViewPalette = NULL;
TOOLSCLASS	*ViewTiles = NULL;
TOOLSCLASS	*ViewMaps = NULL;
TOOLSCLASS	*ViewOAM = NULL;
TOOLSCLASS	*ViewMatrices = NULL;
TOOLSCLASS	*ViewLights = NULL;


volatile BOOL execute = FALSE;
volatile BOOL paused = TRUE;
u32 glock = 0;

BOOL click = FALSE;

BOOL finished = FALSE;
BOOL romloaded = FALSE;

void SetRotate(HWND hwnd, int rot);

BOOL ForceRatio = TRUE;
float DefaultWidth;
float DefaultHeight;
float widthTradeOff;
float heightTradeOff;

HMENU menu;
HANDLE runthread_ready=INVALID_HANDLE_VALUE;
HANDLE runthread=INVALID_HANDLE_VALUE;

//static char IniName[MAX_PATH];
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
unsigned int frameCounter=0;
bool frameAdvance = false;
bool frameCounterDisplay = false;
bool FpsDisplay = false;
unsigned short windowSize = 0;

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

void ResizingLimit(int wParam, RECT *rc)
{
	u32	width = (rc->right - rc->left);
	u32	height = (rc->bottom - rc->top);

	u32 minX = 256;
	u32 minY = 414;

	//printlog("width=%i; height=%i\n", width, height);

	if (GPU_rotation == 90 || GPU_rotation == 270)
	{
		minX = 390;
		minY = 272;
	}
	switch (wParam)
	{
		case WMSZ_LEFT:
			{
				if (width<minX) rc->left=rc->left+(width-minX);
				return;
			}

		case WMSZ_RIGHT:
			{
				if (width<minX) rc->right=rc->left+minX;
				return;
			}

		case WMSZ_TOP:
			{
				if (height<minY) rc->top=rc->top+(height-minY);
				return;
			}

		case WMSZ_BOTTOM:
			{
				if (height<minY) rc->bottom=rc->top+minY;
				return;
			}

		case WMSZ_TOPLEFT:
			{
				if (height<minY) rc->top=rc->top+(height-minY);
				if (width<minX) rc->left=rc->left+(width-minX);
				return;
			}
		case WMSZ_BOTTOMLEFT:
			{
				if (height<minY) rc->bottom=rc->top+minY;
				if (width<minX) rc->left=rc->left+(width-minX);
				return;
			}

		case WMSZ_TOPRIGHT:
			{
				if (height<minY) rc->top=rc->top+(height-minY);
				if (width<minX) rc->right=rc->left+minX;
				return;
			}

		case WMSZ_BOTTOMRIGHT:
			{
				if (height<minY) rc->bottom=rc->top+minY;
				if (width<minX) rc->right=rc->left+minX;
				return;
			}
	}

}

void ScallingScreen(HWND hwnd, int wParam, RECT *rc)
{
	float	aspect;
	u32	width;
	u32	height;
	u32	width2;
	u32	height2;

	width = (rc->right - rc->left - widthTradeOff);
	height = (rc->bottom - rc->top - heightTradeOff);

	if (width ==  height) return;

	if (GPU_rotation == 0 || GPU_rotation == 180)
	{
		aspect = DefaultWidth / DefaultHeight;
	}
	else
	{
		aspect = DefaultHeight / DefaultWidth;
	}

	switch (wParam)
	{
		case WMSZ_LEFT:
		case WMSZ_RIGHT:
			{
				height = (int)(width / aspect);
				rc->bottom=rc->top+height+heightTradeOff;
				return;
			}

		case WMSZ_TOP:
		case WMSZ_BOTTOM:
			{
				width = (int)(height * aspect);
				rc->right=rc->left+width+widthTradeOff;
				return;
			}

		case WMSZ_TOPLEFT:
			{
				width = (int)(height * aspect);
				rc->left=rc->right-width-widthTradeOff;
				return;
			}
		case WMSZ_BOTTOMLEFT:
			{
				height = (int)(width / aspect);
				rc->bottom=rc->top + height + heightTradeOff;
				return;
			}

		case WMSZ_TOPRIGHT:
		case WMSZ_BOTTOMRIGHT:
			{
				width = (int)(height * aspect);
				rc->right=rc->left+width+widthTradeOff;
				return;
			}
	}
}

void ScaleScreen(HWND hwnd, float factor)
{

	if ((GPU_rotation==0)||(GPU_rotation==180))
	{
		SetWindowPos(hwnd, NULL, 0, 0, widthTradeOff + DefaultWidth * factor, 
				heightTradeOff + DefaultHeight * factor, SWP_NOMOVE | SWP_NOZORDER);
	}
	else
	if ((GPU_rotation==90)||(GPU_rotation==270))
	{
		SetWindowPos(hwnd, NULL, 0, 0, heightTradeOff + DefaultHeight * factor, 
				widthTradeOff + DefaultWidth * factor, SWP_NOMOVE | SWP_NOZORDER);
	}
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
}

void CheckMessages()
{
	MSG msg;
	HWND hwnd = MainWindow->getHWnd();
	while( PeekMessage( &msg, 0, 0, 0, PM_NOREMOVE ) )
	{
		if( GetMessage( &msg, 0,  0, 0)>0 )
		{
			if(!TranslateAccelerator(hwnd,hAccel,&msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
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
	 HWND	hwnd = MainWindow->getHWnd();

	 DDCAPS	hw_caps, sw_caps;

	 InitSpeedThrottle();

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
			  EnterCriticalSection(&win_sync);
               cycles = NDS_exec((560190<<1)-cycles,FALSE);
				win_sound_samplecounter = 735;
			   	LeaveCriticalSection(&win_sync);
	
			   SPU_Emulate_core();
			   //avi writing
				DRV_AviSoundUpdate(SPU_core->outbuf,spu_core_samples);
			   DRV_AviVideoUpdate((u16*)GPU_screen);

               if (!skipnextframe)
               {
				   input->process();

				 if (FpsDisplay) osd->addFixed(10, 10, "%02d Fps", fps);
				 osd->update();
				   Display();
				   osd->clear();


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

				  if(ThrottleIsBehind() && framesskipped < 9)
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
				    execute = FALSE;
					SPU_Pause(1);
			   }
			   frameCounter++;
			   if (frameCounterDisplay) osd->addFixed(200, 30, "%d",frameCounter);

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
   execute = FALSE;
   paused = TRUE;
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
	if (!paused)
	{
		NDS_Pause();
		savestate_slot(num);
		NDS_UnPause();
	}
	else
		savestate_slot(num);
}

void StateLoadSlot(int num)
{
	BOOL wasPaused = paused;
	NDS_Pause();
	loadstate_slot(num);
	if(!wasPaused)
		NDS_UnPause();
	else
		Display();
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
      MainWindow->checkMenu(i, MF_BYCOMMAND | MF_UNCHECKED);

   MainWindow->checkMenu(id, MF_BYCOMMAND | MF_CHECKED);
}

void ChangeLanguage(int id)
{
   HMENU newmenu;

   SetLanguage(id);
   newmenu = LoadMenu(hAppInst, "MENU_PRINCIPAL");
   SetMenu(MainWindow->getHWnd(), newmenu);
   DestroyMenu(menu);
   menu = newmenu;   
}

int RegClass(WNDPROC Proc, LPCTSTR szName)
{
	WNDCLASS	wc;

	wc.style = CS_DBLCLKS;
	wc.cbClsExtra = wc.cbWndExtra=0;
	wc.lpfnWndProc=Proc;
	wc.hInstance=hAppInst;
	wc.hIcon=LoadIcon(hAppInst,MAKEINTRESOURCE(IDI_APPLICATION));
	//wc.hIconSm=LoadIcon(hAppInst,MAKEINTRESOURCE(IDI_APPLICATION));
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
	execute = FALSE;
}


int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
	InitializeCriticalSection(&win_sync);

#ifdef GDB_STUB
	gdbstub_handle_t arm9_gdb_stub;
	gdbstub_handle_t arm7_gdb_stub;
	struct armcpu_memory_iface *arm9_memio = &arm9_base_memory_iface;
	struct armcpu_memory_iface *arm7_memio = &arm7_base_memory_iface;
	struct armcpu_ctrl_iface *arm9_ctrl_iface;
	struct armcpu_ctrl_iface *arm7_ctrl_iface;
#endif
	struct configured_features my_config;

	extern bool windows_opengl_init();
	oglrender_init = windows_opengl_init;


	MSG messages;            /* Here messages to the application are saved */
    char text[80];
    hAppInst=hThisInstance;

	init_configured_features( &my_config);
	if ( !fill_configured_features( &my_config, lpszArgument)) {
        MessageBox(NULL,"Unable to parse command line arguments","Error",MB_OK);
        return 0;
	}
	ColorCtrl_Register();

	OpenConsole();			// Init debug console

	if (!RegClass(WindowProcedure, "DeSmuME"))
	{
		MessageBox(NULL, "Error registering windows class", "DeSmuME", MB_OK);
		exit(-1);
	}
	
	GetINIPath();
	windowSize = GetPrivateProfileInt("Video","Window Size", 0, IniName);
	GPU_rotation =  GetPrivateProfileInt("Video","Window Rotate", 0, IniName);
	ForceRatio = GetPrivateProfileInt("Video","Window Force Ratio", 1, IniName);
	FpsDisplay = GetPrivateProfileInt("Video","Display Fps", 0, IniName);

	//sprintf(text, "%s", DESMUME_NAME_AND_VERSION);
	MainWindow = new WINCLASS(CLASSNAME, hThisInstance);
	RECT clientRect = {0,0,256,384};
	DWORD dwStyle = WS_CAPTION| WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	AdjustWindowRect(&clientRect,dwStyle,TRUE);
	if (!MainWindow->create(DESMUME_NAME_AND_VERSION, CW_USEDEFAULT, CW_USEDEFAULT, clientRect.right-clientRect.left,clientRect.bottom-clientRect.top,
			WS_CAPTION| WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
				NULL))
	{
		MessageBox(NULL, "Error creating main window", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	/* default the firmware settings, they may get changed later */
	NDS_FillDefaultFirmwareConfigData( &win_fw_config);

    GetPrivateProfileString("General", "Language", "-1", text, 80, IniName);
    SetLanguage(atoi(text));

	bad_glob_cflash_disk_image_file = my_config.cflash_disk_image_file;

    hAccel = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_MAIN_ACCEL));

#ifdef DEBUG
    LogStart();
#endif

	if (!MainWindow->setMenu(LoadMenu(hThisInstance, "MENU_PRINCIPAL")))
	{
		MessageBox(NULL, "Error creating main menu", "DeSmuME", MB_OK);
		delete MainWindow;
		exit(-1);
	}

	// menu checks
	MainWindow->checkMenu(IDC_FORCERATIO, MF_BYCOMMAND | (ForceRatio==1?MF_CHECKED:MF_UNCHECKED));

	MainWindow->checkMenu(ID_VIEW_DISPLAYFPS, FpsDisplay ? MF_CHECKED : MF_UNCHECKED);

	MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND | ((windowSize==1)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND | ((windowSize==2)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND | ((windowSize==3)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND | ((windowSize==4)?MF_CHECKED:MF_UNCHECKED));

	MainWindow->checkMenu(IDC_ROTATE0, MF_BYCOMMAND | ((GPU_rotation==0)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_ROTATE90, MF_BYCOMMAND | ((GPU_rotation==90)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_ROTATE180, MF_BYCOMMAND | ((GPU_rotation==180)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_ROTATE270, MF_BYCOMMAND | ((GPU_rotation==270)?MF_CHECKED:MF_UNCHECKED));

    DragAcceptFiles(MainWindow->getHWnd(), TRUE);

	printlog("Init NDS\n");

	input = new INPUTCLASS();
	if (!input->Init(MainWindow->getHWnd(), &NDS_inputPost))
	{
		MessageBox(NULL, "Error DXInput init\n", "DeSmuME", MB_OK);
		exit(-1);
	}
	NDS_inputInit();

	ViewDisasm_ARM7 = new TOOLSCLASS(hThisInstance, IDD_DESASSEMBLEUR_VIEWER7, (DLGPROC) ViewDisasm_ARM7Proc);
	ViewDisasm_ARM9 = new TOOLSCLASS(hThisInstance, IDD_DESASSEMBLEUR_VIEWER9, (DLGPROC) ViewDisasm_ARM9Proc);
	ViewMem_ARM7 = new TOOLSCLASS(hThisInstance, IDD_MEM_VIEWER7, (DLGPROC) ViewMem_ARM7Proc);
	ViewMem_ARM9 = new TOOLSCLASS(hThisInstance, IDD_MEM_VIEWER9, (DLGPROC) ViewMem_ARM9Proc);
	ViewRegisters = new TOOLSCLASS(hThisInstance, IDD_IO_REG, (DLGPROC) IoregView_Proc);
	ViewPalette = new TOOLSCLASS(hThisInstance, IDD_PAL, (DLGPROC) ViewPalProc);
	ViewTiles = new TOOLSCLASS(hThisInstance, IDD_TILE, (DLGPROC) ViewTilesProc);
	ViewMaps = new TOOLSCLASS(hThisInstance, IDD_MAP, (DLGPROC) ViewMapsProc);
	ViewOAM = new TOOLSCLASS(hThisInstance, IDD_OAM, (DLGPROC) ViewOAMProc);
	ViewMatrices = new TOOLSCLASS(hThisInstance, IDD_MATRIX_VIEWER, (DLGPROC) ViewMatricesProc);
	ViewLights = new TOOLSCLASS(hThisInstance, IDD_LIGHT_VIEWER, (DLGPROC) ViewLightsProc);
   
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
    GetPrivateProfileString("General", "Language", "0", text, 80, IniName);	//================================================== ???
    CheckLanguage(IDC_LANGENGLISH+atoi(text));

    GetPrivateProfileString("Video", "FrameSkip", "AUTO", text, 80, IniName);

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

#ifdef BETA_VERSION
	EnableMenuItem (menu, IDM_SUBMITBUGREPORT, MF_GRAYED);
#endif
	printlog("Init sound core\n");
    sndcoretype = GetPrivateProfileInt("Sound","SoundCore", SNDCORE_DIRECTX, IniName);
    sndbuffersize = GetPrivateProfileInt("Sound","SoundBufferSize", 735 * 4, IniName);

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

	//runthread_ready = CreateEvent(NULL,TRUE,FALSE,0);
    //runthread = CreateThread(NULL, 0, run, NULL, 0, &threadID);

	//wait for the run thread to signal that it is initialized and ready to run
	//WaitForSingleObject(runthread_ready,INFINITE);

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

    MainWindow->checkMenu(IDC_SAVETYPE1, MF_BYCOMMAND | MF_CHECKED);
    MainWindow->checkMenu(IDC_SAVETYPE2, MF_BYCOMMAND | MF_UNCHECKED);
    MainWindow->checkMenu(IDC_SAVETYPE3, MF_BYCOMMAND | MF_UNCHECKED);
    MainWindow->checkMenu(IDC_SAVETYPE4, MF_BYCOMMAND | MF_UNCHECKED);
    MainWindow->checkMenu(IDC_SAVETYPE5, MF_BYCOMMAND | MF_UNCHECKED);
    MainWindow->checkMenu(IDC_SAVETYPE6, MF_BYCOMMAND | MF_UNCHECKED);

	MainWindow->Show(SW_NORMAL);
	run(0);
	DRV_AviEnd();

	//------SHUTDOWN

#ifdef DEBUG
    LogStop();
#endif
	if (input!=NULL) delete input;
	if (ViewLights!=NULL) delete ViewLights;
	if (ViewMatrices!=NULL) delete ViewMatrices;
	if (ViewOAM!=NULL) delete ViewOAM;
	if (ViewMaps!=NULL) delete ViewMaps;
	if (ViewTiles!=NULL) delete ViewTiles;
	if (ViewPalette!=NULL) delete ViewPalette;
	if (ViewRegisters!=NULL) delete ViewRegisters;
	if (ViewMem_ARM9!=NULL) delete ViewMem_ARM9;
	if (ViewMem_ARM7!=NULL) delete ViewMem_ARM7;
	if (ViewDisasm_ARM9!=NULL) delete ViewDisasm_ARM9;
	if (ViewDisasm_ARM7!=NULL) delete ViewDisasm_ARM7;

	delete MainWindow;
	
	CloseConsole();

    return 0;
}

void GetWndRect(HWND hwnd)
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

//========================================================================================
void SetRotate(HWND hwnd, int rot)
{
	GPU_rotation = rot;

	switch (rot)
	{
		case 0:
			GPU_width    = 256;
			GPU_height   = 192*2;
			rotationstartscan = 192;
			rotationscanlines = 192*2;
			break;

		case 90:
			GPU_rotation = 90;
			GPU_width    = 192*2;
			GPU_height   = 256;
			rotationstartscan = 0;
			rotationscanlines = 256;
			break;

		case 180:
			GPU_rotation = 180;
			GPU_width    = 256;
			GPU_height   = 192*2;
			rotationstartscan = 0;
			rotationscanlines = 192*2;
			break;

		case 270:
			GPU_rotation = 270;
			GPU_width    = 192*2;
			GPU_height   = 256;
			rotationstartscan = 0;
			rotationscanlines = 256;
			break;
	}
  
	SetWindowClientSize(hwnd, GPU_width, GPU_height);
	MainWindow->checkMenu(IDC_ROTATE0, MF_BYCOMMAND | ((GPU_rotation==0)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_ROTATE90, MF_BYCOMMAND | ((GPU_rotation==90)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_ROTATE180, MF_BYCOMMAND | ((GPU_rotation==180)?MF_CHECKED:MF_UNCHECKED));
	MainWindow->checkMenu(IDC_ROTATE270, MF_BYCOMMAND | ((GPU_rotation==270)?MF_CHECKED:MF_UNCHECKED));
	WritePrivateProfileInt("Video","Window Rotate",GPU_rotation,IniName);
}

static void AviEnd()
{
	NDS_Pause();
	DRV_AviEnd();
	NDS_UnPause();
}

//Shows an Open File menu and starts recording an AVI
static void AviRecordTo()
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

//========================================================================================
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
	static int tmp_execute;
    switch (message)                  // handle the messages
    {
		/*case WM_ENTERMENULOOP:		// temporally removed it (freezes)
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
	     	GetClientRect(hwnd, &clientSize);
			GetWindowRect(hwnd, &fullSize);
	     	DefaultWidth = clientSize.right - clientSize.left;
			DefaultHeight = clientSize.bottom - clientSize.top;
			widthTradeOff = (fullSize.right - fullSize.left) - (clientSize.right - clientSize.left);
			heightTradeOff = (fullSize.bottom - fullSize.top) - (clientSize.bottom - clientSize.top);

            if ( (windowSize < 1) || (windowSize > 4) )
			{
				int w=GetPrivateProfileInt("Video","Window width", 0, IniName);
				int h=GetPrivateProfileInt("Video","Window height", 0, IniName);
				if (w && h)	
				{
					RECT fullSize = {0, 0, w, h};
					ResizingLimit(WMSZ_RIGHT, &fullSize);

					if (ForceRatio)	
						ScallingScreen(hwnd, WMSZ_RIGHT, &fullSize);
					SetWindowPos(hwnd, NULL, 0, 0, fullSize.right - fullSize.left, 
												fullSize.bottom - fullSize.top, SWP_NOMOVE | SWP_NOZORDER);
				}
				else 
					windowSize=1;
			}
			if ( (windowSize > 0) && (windowSize < 5) )	ScaleScreen(hwnd, windowSize);
			
			return 0;
			
	     }
        case WM_DESTROY:
		case WM_CLOSE:
			{
             NDS_Pause();

			 WritePrivateProfileInt("Video","Window Size",windowSize,IniName);
			 if (windowSize==0)
			 {
				 WritePrivateProfileInt("Video","Window width",MainWindowRect.right-MainWindowRect.left+widthTradeOff,IniName);
				 WritePrivateProfileInt("Video","Window height",MainWindowRect.bottom-MainWindowRect.top+heightTradeOff,IniName);
			 }

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
             ExitRunLoop();;
             return 0;
			}
		case WM_MOVE:
				GetWndRect(hwnd);
				return 0;
		case WM_SIZING:
			{
				RECT *rc=(RECT *)lParam;

				windowSize=0;
				ResizingLimit(wParam, rc);
				if (ForceRatio)
					ScallingScreen(hwnd, wParam, rc);
				//printlog("sizing: width=%i; height=%i\n", rc->right - rc->left, rc->bottom - rc->top);
			}
			break;
		case WM_SIZE:
    			if (ForceRatio) {
					if ( windowSize != 0 )	ScaleScreen(hwnd, windowSize);
				}
				GetWndRect(hwnd);
				return 0;
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
				case IDM_QUIT:
					DestroyWindow(hwnd);
					return 0;
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
				  case IDM_FILE_RECORDAVI:
					  AviRecordTo();
					  break;
				case IDM_FILE_STOPAVI:
						AviEnd();
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
						bool tpaused=false;
						if (execute) 
						{
							tpaused=true;
							NDS_Pause();
						}
						DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SOUNDSETTINGS), hwnd, (DLGPROC)SoundSettingsDlgProc);
						if (tpaused)
							NDS_UnPause();
                  }
                  return 0;
                  case IDM_GAME_INFO:
                       {
                            CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_GAME_INFO), hwnd, GinfoView_Proc);
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
						return 0;
                  case IDM_MEMORY:
						   ViewMem_ARM7->regClass("MemViewBox7", ViewMem_ARM7BoxProc);
						   if (!ViewMem_ARM7->open())
							   ViewMem_ARM7->unregClass();
						   ViewMem_ARM9->regClass("MemViewBox9", ViewMem_ARM9BoxProc);
						   if (!ViewMem_ARM9->open())
							   ViewMem_ARM9->unregClass();
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
                  
				  case ACCEL_SPACEBAR:
				  case IDM_PAUSE:
					  if (emu_paused) NDS_UnPause();
					  else NDS_Pause();
					  emu_paused ^= 1;
					  MainWindow->checkMenu(IDM_PAUSE, emu_paused ? MF_CHECKED : MF_UNCHECKED);
				  return 0;
				  
				  case ACCEL_N: //Frame Advance
					  frameAdvance = true;
					  execute = TRUE;
					  emu_paused = 1;
					  MainWindow->checkMenu(IDM_PAUSE, emu_paused ? MF_CHECKED : MF_UNCHECKED);
				  return 0;

				  case ID_VIEW_FRAMECOUNTER:
					  frameCounterDisplay ^= 1;
					  MainWindow->checkMenu(ID_VIEW_FRAMECOUNTER, frameCounterDisplay ? MF_CHECKED : MF_UNCHECKED);
				  return 0;

				  case ID_VIEW_DISPLAYFPS:
					  FpsDisplay ^= 1;
					  MainWindow->checkMenu(ID_VIEW_DISPLAYFPS, FpsDisplay ? MF_CHECKED : MF_UNCHECKED);
					  WritePrivateProfileInt("Video", "Display Fps", FpsDisplay, IniName);
					  osd->clear();
				  return 0;

                  #define saver(one,two,three,four,five, six) \
                  MainWindow->checkMenu(IDC_SAVETYPE1, MF_BYCOMMAND | one); \
                  MainWindow->checkMenu(IDC_SAVETYPE2, MF_BYCOMMAND | two); \
                  MainWindow->checkMenu(IDC_SAVETYPE3, MF_BYCOMMAND | three); \
                  MainWindow->checkMenu(IDC_SAVETYPE4, MF_BYCOMMAND | four); \
                  MainWindow->checkMenu(IDC_SAVETYPE5, MF_BYCOMMAND | five); \
                  MainWindow->checkMenu(IDC_SAVETYPE6, MF_BYCOMMAND | six);
                  
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
					   frameCounter=0;
                  return 0;
                  case IDM_CONFIG:
                       {
							bool tpaused=false;
							if (execute) 
							{
								tpaused=true;
								NDS_Pause();
							}
							InputConfig(hwnd);
							if (tpaused)
								NDS_UnPause();
                       }
                  return 0;
                  case IDM_FIRMSETTINGS:
					  {
				  		bool tpaused=false;
						if (execute) 
						{
							tpaused=true;
							NDS_Pause();
						}
						DialogBox(hAppInst,MAKEINTRESOURCE(IDD_FIRMSETTINGS), hwnd, (DLGPROC) FirmConfig_Proc);
						if (tpaused)
							NDS_UnPause();
		
						return 0;
					  }
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

                       MainWindow->checkMenu(IDC_FRAMESKIPAUTO, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP0, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP1, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP2, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP3, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP4, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP5, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP6, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP7, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP8, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(IDC_FRAMESKIP9, MF_BYCOMMAND | MF_UNCHECKED);
                       MainWindow->checkMenu(LOWORD(wParam), MF_BYCOMMAND | MF_CHECKED);
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

        case IDC_WINDOW1X:
			windowSize=1;
			ScaleScreen(hwnd, windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);

			MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND | ((windowSize==1)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND | ((windowSize==2)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND | ((windowSize==3)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND | ((windowSize==4)?MF_CHECKED:MF_UNCHECKED));
			break;
		case IDC_WINDOW2X:
			windowSize=2;
			ScaleScreen(hwnd, windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);

			MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND | ((windowSize==1)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND | ((windowSize==2)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND | ((windowSize==3)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND | ((windowSize==4)?MF_CHECKED:MF_UNCHECKED));
			break;
		case IDC_WINDOW3X:
			windowSize=3;
			ScaleScreen(hwnd, windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);

			MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND | ((windowSize==1)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND | ((windowSize==2)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND | ((windowSize==3)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND | ((windowSize==4)?MF_CHECKED:MF_UNCHECKED));
			break;
		case IDC_WINDOW4X:
			windowSize=4;
			ScaleScreen(hwnd, windowSize);
			WritePrivateProfileInt("Video","Window Size",windowSize,IniName);

			MainWindow->checkMenu(IDC_WINDOW1X, MF_BYCOMMAND | ((windowSize==1)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW2X, MF_BYCOMMAND | ((windowSize==2)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW3X, MF_BYCOMMAND | ((windowSize==3)?MF_CHECKED:MF_UNCHECKED));
			MainWindow->checkMenu(IDC_WINDOW4X, MF_BYCOMMAND | ((windowSize==4)?MF_CHECKED:MF_UNCHECKED));
			break;

		case IDC_FORCERATIO:
			if (ForceRatio) {
				MainWindow->checkMenu(IDC_FORCERATIO, MF_BYCOMMAND | MF_UNCHECKED);
				ForceRatio = FALSE;
				WritePrivateProfileInt("Video","Window Force Ratio",0,IniName);
			}
			else {
				RECT fullSize;
				GetWindowRect(hwnd, &fullSize);
				ScallingScreen(hwnd, WMSZ_RIGHT, &fullSize);
				SetWindowPos(hwnd, NULL, 0, 0, fullSize.right - fullSize.left, 
												fullSize.bottom - fullSize.top, SWP_NOMOVE | SWP_NOZORDER);
				//ScaleScreen(hwnd, (fullSize.bottom - fullSize.top - heightTradeOff) / DefaultHeight);
				MainWindow->checkMenu(IDC_FORCERATIO, MF_BYCOMMAND | MF_CHECKED);
				ForceRatio = TRUE;
				WritePrivateProfileInt("Video","Window Force Ratio",1,IniName);
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

               // Write Sound core type
               sndcoretype = SNDCoreList[SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_GETCURSEL, 0, 0)]->id;
               sprintf(tempstr, "%d", sndcoretype);
               WritePrivateProfileString("Sound", "SoundCore", tempstr, IniName);

               // Write Sound Buffer size
               GetDlgItemText(hDlg, IDC_SOUNDBUFFERET, tempstr, 6);
               sscanf(tempstr, "%d", &sndbuffersize);
               WritePrivateProfileString("Sound", "SoundBufferSize", tempstr, IniName);

			   EnterCriticalSection(&win_sync);
               SPU_ChangeSoundCore(sndcoretype, sndbuffersize);
			   LeaveCriticalSection(&win_sync);

               // Write Volume
               sndvolume = SendDlgItemMessage(hDlg, IDC_SLVOLUME, TBM_GETPOS, 0, 0);
               sprintf(tempstr, "%d", sndvolume);
               WritePrivateProfileString("Sound", "Volume", tempstr, IniName);
               SPU_SetVolume(sndvolume);

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


