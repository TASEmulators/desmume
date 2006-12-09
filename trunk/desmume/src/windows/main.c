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

//#define RENDER3D

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "CWindow.h"
#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "resource.h"
#include "memView.h"
#include "disView.h"
#include "ginfo.h"
#include "IORegView.h"
#include "palView.h"
#include "tileView.h"
#include "oamView.h"
#include "../debug.h"
#include "mapview.h"
#include "../saves.h"
#include "../cflash.h"
#include "ConfigKeys.h"

#include "snddx.h"

#ifdef RENDER3D
     #include "OGLRender.h"
#endif

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

/*  Make the class name into a global variable  */
char SavName[MAX_PATH] = "";
char SavName2[MAX_PATH] = "";
char szClassName[ ] = "DeSmuME";
int romnum = 0;

DWORD threadID;

HWND hwnd;
HDC  hdc;
HINSTANCE hAppInst;  

volatile BOOL execute = FALSE;
volatile BOOL paused = TRUE;
u32 glock = 0;

BOOL click = FALSE;

BOOL finished = FALSE;
BOOL romloaded = FALSE;

HMENU menu;
HANDLE runthread=INVALID_HANDLE_VALUE;

const DWORD tabkey[48]={0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,VK_SPACE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_TAB,VK_SHIFT,VK_DELETE,VK_INSERT,VK_HOME,VK_END,0x0d};
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

int autoframeskipenab=1;
int frameskiprate=0;

LRESULT CALLBACK SoundSettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                      LPARAM lParam);

DWORD WINAPI run( LPVOID lpParameter)
{
     char txt[80];
     BITMAPV4HEADER bmi;
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

     //CreateBitmapIndirect(&bmi);
     memset(&bmi, 0, sizeof(bmi));
     bmi.bV4Size = sizeof(bmi);
     bmi.bV4Planes = 1;
     bmi.bV4BitCount = 16;
     bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
     bmi.bV4RedMask = 0x001F;
     bmi.bV4GreenMask = 0x03E0;
     bmi.bV4BlueMask = 0x7C00;
     bmi.bV4Width = 256;
     bmi.bV4Height = -192;

     #ifdef RENDER3D
     OGLRender::init(&hdc);
     #endif
     QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
     QueryPerformanceCounter((LARGE_INTEGER *)&lastticks);
     OneFrameTime = freq / 60;

     while(!finished)
     {
          while(execute)
          {
               cycles = NDS_exec((560190<<1)-cycles,FALSE);
               SPU_Emulate();

               if (!skipnextframe)
               {
                  #ifndef RENDER3D
                  SetDIBitsToDevice(hdc, 0, 0, 256, 192*2, 0, 0, 0, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
                  //SetDIBitsToDevice(hdc, 0, 192, 256, 192*2, 0, 0, 192, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
                  #else
                  SetDIBitsToDevice(hdc, 0, 0, 256, 192*2, 0, 0, 192, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
                  //SetDIBitsToDevice(hdc, 0, 0, 256, 192, 0, 0, 0, 192, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
                  #endif
                  fpsframecount++;
                  QueryPerformanceCounter((LARGE_INTEGER *)&curticks);
                  if(curticks >= fpsticks + freq)
                  {
                     fps = fpsframecount;
                     sprintf(txt,"DeSmuME %d", fps);
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
                     framestoskip = frameskiprate + 1;
               }

               CWindow_RefreshALL();
               Sleep(0);
          }
          paused = TRUE;
          Sleep(500);
     }
     return 1;
}

void NDS_Pause()
{
   execute = FALSE;
   SPU_Pause(1);
   while (!paused) {}
}

void NDS_UnPause()
{
   paused = FALSE;
   execute = TRUE;
   SPU_Pause(0);
}

BOOL LoadROM(char * filename)
{
    NDS_Pause();

    if (NDS_LoadROM(filename) > 0)
       return TRUE;

    return FALSE;
}

int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
    MSG messages;            /* Here messages to the application are saved */
    char text[80];
    cwindow_struct MainWindow;
    HACCEL hAccel;
    hAppInst=hThisInstance;

    InitializeCriticalSection(&section);
    sprintf(text, "DeSmuME v%s", VERSION);

    hAccel = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_MAIN_ACCEL));

    if (CWindow_Init(&MainWindow, hThisInstance, szClassName, text,
                     WS_CAPTION| WS_SYSMENU |WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                     256, 384, WindowProcedure) != 0)
    {
        MessageBox(NULL,"Unable to create main window","Error",MB_OK);
        return 0;
    }

    hwnd = MainWindow.hwnd;
    menu = LoadMenu(hThisInstance, "MENU_PRINCIPAL");
    SetMenu(hwnd, menu);
    hdc = GetDC(hwnd);
    DragAcceptFiles(hwnd, TRUE);
    
    /* Make the window visible on the screen */
    CWindow_Show(&MainWindow);

    InitMemViewBox();
    InitDesViewBox();
    InitTileViewBox();
    InitOAMViewBox();
    
#ifdef DEBUG
    LogStart();
#endif

    GetINIPath(IniName);

    NDS_Init();

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

    sndcoretype = GetPrivateProfileInt("Sound","SoundCore", SNDCORE_DIRECTX, IniName);
    sndbuffersize = GetPrivateProfileInt("Sound","SoundBufferSize", 735 * 4, IniName);

    if (SPU_ChangeSoundCore(sndcoretype, sndbuffersize) != 0)
    {
       MessageBox(hwnd,"Unable to initialize DirectSound","Error",MB_OK);
       return messages.wParam;
    }

    sndvolume = GetPrivateProfileInt("Sound","Volume",100, IniName);
    SPU_SetVolume(sndvolume);
    
    runthread = CreateThread(NULL, 0, run, NULL, 0, &threadID);
    
    if(LoadROM(lpszArgument))
    {
       EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
       EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
       EnableMenuItem(menu, IDM_RESET, MF_ENABLED);
       EnableMenuItem(menu, IDM_GAME_INFO, MF_ENABLED);
       romloaded = TRUE;
       NDS_UnPause();
    }
    else
    {
       EnableMenuItem(menu, IDM_EXEC, MF_ENABLED);
       EnableMenuItem(menu, IDM_PAUSE, MF_GRAYED);
       EnableMenuItem(menu, IDM_RESET, MF_GRAYED);
       EnableMenuItem(menu, IDM_GAME_INFO, MF_GRAYED);
    }

    CheckMenuItem(menu, IDC_SAVETYPE1, MF_BYCOMMAND | MF_CHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE2, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE3, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE4, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE5, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(menu, IDC_SAVETYPE6, MF_BYCOMMAND | MF_UNCHECKED);

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
    
#ifdef DEBUG
    LogStop();
#endif
    /* The program return-value is 0 - The value that PostQuitMessage() gave */
    return messages.wParam;
}

void StateSaveSlot(int num)
{
   char filename[MAX_PATH];
   NDS_Pause();
   strcpy(filename, szRomBaseName);
   sprintf(filename+strlen(filename), "%d.dst", num);
   savestate_save(filename);
   NDS_UnPause();
}

void StateLoadSlot(int num)
{
   char filename[MAX_PATH];
   NDS_Pause();
   strcpy(filename, szRomBaseName);
   sprintf(filename+strlen(filename), "%d.dst", num);
   savestate_load(filename);
   NDS_UnPause();
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
    switch (message)                  // handle the messages
    {
        case WM_CREATE:
             ReadConfig();
             return 0;
        case WM_DESTROY:
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
        case WM_CLOSE:
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
        case WM_DROPFILES:
             {
                  char filename[MAX_PATH] = "";
                  DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
                  DragFinish((HDROP)wParam);
                  if(LoadROM(filename))
                  {
                     EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
                     EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
                     EnableMenuItem(menu, IDM_RESET, MF_ENABLED);
                     EnableMenuItem(menu, IDM_GAME_INFO, MF_ENABLED);
                     romloaded = TRUE;
                     NDS_UnPause();
                  }
             }
             return 0;
        case WM_KEYDOWN:
             //if(wParam=='1'){PostMessage(hwnd, WM_DESTROY, 0, 0);return 0;}
             
             if(wParam==tabkey[ds_a]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFE;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFE;
             return 0; }
             if(wParam==tabkey[ds_b]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFD;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFD;
             return 0; }
             if(wParam==tabkey[ds_select]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFFB;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFFB;
             return 0; }
             if(wParam==tabkey[ds_start]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFF7;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFF7;
             return 0; }
             if(wParam==tabkey[ds_right]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFEF;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFEF;
             return 0; }
             if(wParam==tabkey[ds_left]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFDF;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFDF;
             return 0; }
             if(wParam==tabkey[ds_up]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFFBF;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFFBF;
             return 0; }
             if(wParam==tabkey[ds_down]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFF7F;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFF7F;
             return 0; }
             if(wParam==tabkey[ds_r]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFEFF;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFEFF;
             return 0; }
             if(wParam==tabkey[ds_l]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] &= 0xFDFF;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] &= 0xFDFF;
             return 0; }
             if(wParam==tabkey[ds_x]){
             ((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFE;
             return 0; }
             if(wParam==tabkey[ds_y]){
             ((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFD;
             return 0; }
             if(wParam==tabkey[ds_debug]){
             ((u16 *)MMU.ARM7_REG)[0x136>>1] &= 0xFFFB;
             return 0; }
             return 0;
                  /*case 0x1E :
                       MMU.ARM7_REG[0x136] &= 0xFE;
                       break;
                  case 0x1F :
                       MMU.ARM7_REG[0x136] &= 0xFD;
                       break;
                  case 0x21 :
                       NDS_ARM9.wIRQ = TRUE;
                       break; */
        case WM_KEYUP:
             if(wParam==tabkey[ds_a]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x1;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x1;
             return 0; }
             if(wParam==tabkey[ds_b]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x2;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x2;
             return 0; }
             if(wParam==tabkey[ds_select]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x4;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x4;
             return 0; }
             if(wParam==tabkey[ds_start]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x8;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x8;
             return 0; }
             if(wParam==tabkey[ds_right]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x10;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x10;
             return 0; }
             if(wParam==tabkey[ds_left]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x20;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x20;
             return 0; }
             if(wParam==tabkey[ds_up]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x40;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x40;
             return 0; }
             if(wParam==tabkey[ds_down]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x80;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x80;
             return 0; }
             if(wParam==tabkey[ds_r]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x100;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x100;
             return 0; }
             if(wParam==tabkey[ds_l]){
             ((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] |= 0x200;
             ((u16 *)MMU.ARM7_REG)[0x130>>1] |= 0x200;
             return 0; }
             if(wParam==tabkey[ds_x]){
             ((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x1;
             return 0; }
             if(wParam==tabkey[ds_y]){
             ((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x2;
             return 0; }
             if(wParam==tabkey[ds_debug]){
             ((u16 *)MMU.ARM7_REG)[0x136>>1] |= 0x4;
             return 0; }
             break;
                       
                  /*case 0x1E :
                       MMU.ARM7_REG[0x136] |= 1;
                       break;
                  case 0x1F :
                       MMU.ARM7_REG[0x136] |= 2;
                       break;*/
                  /*case 0x21 :
                       MMU.REG_IME[0] = 1;*/
        case WM_MOUSEMOVE:
             {
                  if (wParam & MK_LBUTTON)
                  {
                       s32 x = (s32)((s16)LOWORD(lParam));
                       s32 y = (s32)((s16)HIWORD(lParam)) - 192;
                       if(x<0) x = 0; else if(x>255) x = 255;
                       if(y<0) y = 0; else if(y>192) y = 192;
                       NDS_setTouchPos(x, y);
                       return 0;
                  }
             }
             NDS_releasTouch();
             return 0;
        case WM_LBUTTONDOWN:
             if(HIWORD(lParam)>=192)
             {
                  s32 x;
                  s32 y;
                  SetCapture(hwnd);
                  x = LOWORD(lParam);
                  y = HIWORD(lParam) - 192;
                  if(x<0) x = 0; else if(x>255) x = 255;
                  if(y<0) y = 0; else if(y>192) y = 192;
                  NDS_setTouchPos(x, y);
                  click = TRUE;
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
                            OPENFILENAME ofn;
                            char filename[MAX_PATH] = "";
                            NDS_Pause(); //Stop emulation while opening new rom
                            
                            ZeroMemory(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = hwnd;
                            ofn.lpstrFilter = "NDS ROM file (*.nds)\0*.nds\0NDS/GBA ROM File (*.ds.gba)\0*.ds.gba\0Any file (*.*)\0*.*\0\0";
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

                            if(LoadROM(filename))
                            {
                               EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
                               EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
                               EnableMenuItem(menu, IDM_RESET, MF_ENABLED);
                               EnableMenuItem(menu, IDM_GAME_INFO, MF_ENABLED);
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

                            if ((IoregView = malloc(sizeof(cwindow_struct))) == NULL)
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
                  case IDM_EXEC:
                       EnableMenuItem(menu, IDM_EXEC, MF_GRAYED);
                       EnableMenuItem(menu, IDM_PAUSE, MF_ENABLED);
                       NDS_UnPause();
                  return 0;
                  case IDM_PAUSE:
                       EnableMenuItem(menu, IDM_EXEC, MF_ENABLED);
                       EnableMenuItem(menu, IDM_PAUSE, MF_GRAYED);
                       NDS_Pause();
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
                       mc_realloc(&MMU.bupmem, MC_TYPE_AUTODETECT,1);
                  return 0;
                  case IDC_SAVETYPE2:
                       saver(MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mc_realloc(&MMU.bupmem, MC_TYPE_EEPROM1,MC_SIZE_4KBITS);
                  return 0;   
                  case IDC_SAVETYPE3:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mc_realloc(&MMU.bupmem, MC_TYPE_EEPROM2,MC_SIZE_64KBITS);
                  return 0;   
                  case IDC_SAVETYPE4:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED,MF_UNCHECKED);
                       mc_realloc(&MMU.bupmem, MC_TYPE_EEPROM2,MC_SIZE_512KBITS);
                  return 0;
                  case IDC_SAVETYPE5:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED,MF_UNCHECKED);
                       mc_realloc(&MMU.bupmem, MC_TYPE_FRAM,MC_SIZE_256KBITS);
                  return 0; 
                  case IDC_SAVETYPE6:
                       saver(MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_UNCHECKED,MF_CHECKED);
                       mc_realloc(&MMU.bupmem, MC_TYPE_FLASH,MC_SIZE_2MBITS);
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
                  case IDM_WEBSITE:
                       ShellExecute(NULL, "open", "http://desmume.sourceforge.net", NULL, NULL, SW_SHOWNORMAL);
                  return 0;
                  case IDM_FORUM:
                       ShellExecute(NULL, "open", "http://sourceforge.net/forum/?group_id=164579", NULL, NULL, SW_SHOWNORMAL);
                  return 0;
                  case IDM_SUBMITBUGREPORT:
                       ShellExecute(NULL, "open", "http://sourceforge.net/tracker/?func=add&group_id=164579&atid=832291", NULL, NULL, SW_SHOWNORMAL);
                  return 0;
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
   static timerid=0;

   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         int i;
         char tempstr[MAX_PATH];

         // Setup Sound Core Combo box
         SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_RESETCONTENT, 0, 0);
         SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_ADDSTRING, 0, (long)"None");

         for (i = 1; SNDCoreList[i] != NULL; i++)
            SendDlgItemMessage(hDlg, IDC_SOUNDCORECB, CB_ADDSTRING, 0, (long)SNDCoreList[i]->Name);

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

