/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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
#include "cflash.h"
#include "ConfigKeys.h"

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

BOOL execute = FALSE;
u32 glock = 0;

BOOL click = FALSE;

BOOL finished = FALSE;

HMENU menu;

const DWORD tabkey[48]={0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,VK_SPACE,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_TAB,VK_SHIFT,VK_DELETE,VK_INSERT,VK_HOME,VK_END,0x0d};
DWORD ds_up,ds_down,ds_left,ds_right,ds_a,ds_b,ds_x,ds_y,ds_l,ds_r,ds_select,ds_start,ds_debug;

DWORD WINAPI run( LPVOID lpParameter)
{
     u64 count;
     u64 freq;
     u64 nextcount=0;
     u32 nbframe = 0;
     char txt[80];
     BITMAPV4HEADER bmi;
     u32 cycles = 0;
     int wait=0;

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
     QueryPerformanceCounter((LARGE_INTEGER *)&count);
     nextcount = count + freq;
     
     while(!finished)
     {
          while(execute)
          {
               cycles = NDS_exec((560190<<1)-cycles,FALSE);
               ++nbframe;
               QueryPerformanceCounter((LARGE_INTEGER *)&count);
               if(nextcount<=count)
               {
                    if(wait>0)
                       Sleep(wait);
                    sprintf(txt,"DeSmuME %d", (int)nbframe);
                    if(nbframe>60)
                       wait += (nbframe-60)*2;
                    else if(nbframe<60 && wait>0)
                       wait -= (60-nbframe);
                    SetWindowText(hwnd, txt);
                    nbframe = 0;
                    nextcount += freq;
               }
               #ifndef RENDER3D
               SetDIBitsToDevice(hdc, 0, 0, 256, 192*2, 0, 0, 0, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
               //SetDIBitsToDevice(hdc, 0, 192, 256, 192*2, 0, 0, 192, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
               #else
               SetDIBitsToDevice(hdc, 0, 0, 256, 192*2, 0, 0, 192, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
               //SetDIBitsToDevice(hdc, 0, 0, 256, 192, 0, 0, 0, 192, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
               #endif
               CWindow_RefreshALL();
               Sleep(0);
               //execute = FALSE;
          }
          Sleep(500);
     }
     return 1;
}

BOOL LoadROM(char * filename)
{
    HANDLE File;
    u32 FileSize;
    u32 nblu = 0;
    u32 mask;
    u8 *ROM;

    if(!strlen(filename)) return FALSE;
    
    if((*filename)=='\"')
    {
         ++filename;
         filename[strlen(filename)-1] = '\0';
    }
         
    if(strncmp(filename + strlen(filename)-3, "nds", 3))
    {
         MessageBox(hwnd,"Error, not a nds file","Error",MB_OK);
         return FALSE;
    }
    
    File = CreateFile(filename,
                      GENERIC_READ | FILE_FLAG_OVERLAPPED,
                      FILE_SHARE_READ, 
                      NULL,OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, 
                      NULL);
                                 
    if(File == INVALID_HANDLE_VALUE)
    {
         MessageBox(hwnd,"Error opening the file","Error",MB_OK);
         return FALSE;
    }
    
    execute = FALSE;
                
    mask = FileSize = GetFileSize(File, NULL) - 1;
    mask |= (mask >>1);
    mask |= (mask >>2);
    mask |= (mask >>4);
    mask |= (mask >>8);
    mask |= (mask >>16);

    if ((ROM = (u8 *)malloc((mask+1)*sizeof(u8))) == NULL)
    {
       CloseHandle(File);
       return FALSE;
    }

    ReadFile(File, ROM, FileSize, &nblu, NULL);
    NDS_loadROM(ROM, mask);
    CloseHandle(File);
    return TRUE;
}

int WriteBMP(const char *filename,u16 *bmp){
    BITMAPFILEHEADER fileheader;
    BITMAPV4HEADER imageheader;
    FILE *file;
    int i,j,k;

    memset(&fileheader, 0, sizeof(fileheader));
    fileheader.bfType = 'B' | ('M' << 8);
    fileheader.bfSize = sizeof(fileheader);
    fileheader.bfOffBits = sizeof(fileheader)+sizeof(imageheader);
    
    memset(&imageheader, 0, sizeof(imageheader));
    imageheader.bV4Size = sizeof(imageheader);
    imageheader.bV4Width = 256;
    imageheader.bV4Height = 192*2;
    imageheader.bV4Planes = 1;
    imageheader.bV4BitCount = 24;
    imageheader.bV4V4Compression = BI_RGB;
    imageheader.bV4SizeImage = imageheader.bV4Width * imageheader.bV4Height * sizeof(RGBTRIPLE);
    
    if ((file = fopen(filename,"wb")) == NULL)
       return 0;

    fwrite(&fileheader, 1, sizeof(fileheader), file);
    fwrite(&imageheader, 1, sizeof(imageheader), file);

    for(j=0;j<192*2;j++)
    {
       for(i=0;i<256;i++)
       {
          u8 r,g,b;
          u16 pixel = bmp[(192*2-j-1)*256+i];
          r = pixel>>10;
          pixel-=r<<10;
          g = pixel>>5;
          pixel-=g<<5;
          b = pixel;
          r*=255/31;
          g*=255/31;
          b*=255/31;
          fwrite(&r, 1, sizeof(u8), file); 
          fwrite(&g, 1, sizeof(u8), file); 
          fwrite(&b, 1, sizeof(u8), file);
       }
    }
    fclose(file);

    return 1;
}
int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
    MSG messages;            /* Here messages to the application are saved */
    hAppInst=hThisInstance;
    char text[80];
    cwindow_struct MainWindow;

    InitializeCriticalSection(&section);
    sprintf(text, "DeSmuME v%s", VERSION);

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

    NDSInit();

    //ARM7 BIOS IRQ HANDLER
    MMU_writeWord(1, 0x00, 0xE25EF002);
    MMU_writeWord(1, 0x04, 0xEAFFFFFE);
    MMU_writeWord(1, 0x18, 0xEA000000);
    MMU_writeWord(1, 0x20, 0xE92D500F);
    MMU_writeWord(1, 0x24, 0xE3A00301);
    MMU_writeWord(1, 0x28, 0xE28FE000);
    MMU_writeWord(1, 0x2C, 0xE510F004);
    MMU_writeWord(1, 0x30, 0xE8BD500F);
    MMU_writeWord(1, 0x34, 0xE25EF004);
    
    //ARM9 BIOS IRQ HANDLER
    MMU_writeWord(0, 0xFFF0018, 0xEA000000);
    MMU_writeWord(0, 0xFFF0020, 0xE92D500F);
    MMU_writeWord(0, 0xFFF0024, 0xEE190F11);
    MMU_writeWord(0, 0xFFF0028, 0xE1A00620);
    MMU_writeWord(0, 0xFFF002C, 0xE1A00600);
    MMU_writeWord(0, 0xFFF0030, 0xE2800C40);
    MMU_writeWord(0, 0xFFF0034, 0xE28FE000);
    MMU_writeWord(0, 0xFFF0038, 0xE510F004);
    MMU_writeWord(0, 0xFFF003C, 0xE8BD500F);
    MMU_writeWord(0, 0xFFF0040, 0xE25EF004);
        
    MMU_writeWord(0, 0x0000004, 0xE3A0010E);
    MMU_writeWord(0, 0x0000008, 0xE3A01020);
//    MMU_writeWord(0, 0x000000C, 0xE1B02110);
    MMU_writeWord(0, 0x000000C, 0xE1B02040);
    MMU_writeWord(0, 0x0000010, 0xE3B02020);
//    MMU_writeWord(0, 0x0000010, 0xE2100202);
    
    CreateThread(NULL, 0, run, NULL, 0, &threadID);
    
    if(LoadROM(lpszArgument)) execute = TRUE;
    
    while (GetMessage (&messages, NULL, 0, 0))
    {
        // Translate virtual-key messages into character messages
        TranslateMessage(&messages);
        // Send message to WindowProcedure 
        DispatchMessage(&messages);
    }
    
#ifdef DEBUG
    LogStop();
#endif
    /* The program return-value is 0 - The value that PostQuitMessage() gave */
    return messages.wParam;
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
    switch (message)                  // handle the messages
    {
        case WM_CREATE:
             ReadConfig();
             return 0;
        case WM_DESTROY:
             execute = FALSE;
             finished = TRUE;
             PostQuitMessage (0);       // send a WM_QUIT to the message queue 
             return 0;
        case WM_CLOSE:
             execute = FALSE;
             finished = TRUE;
             PostMessage(hwnd, WM_QUIT, 0, 0);
             return 0;
        case WM_DROPFILES:
             {
                  char filename[MAX_PATH] = "";
                  DragQueryFile((HDROP)wParam,0,filename,MAX_PATH);
                  DragFinish((HDROP)wParam);
                  if(LoadROM(filename)) execute = TRUE;
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
             break;
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
                  SetCapture(hwnd);
                  s32 x = LOWORD(lParam);
                  s32 y = HIWORD(lParam) - 192;
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
                            ZeroMemory(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = hwnd;
                            ofn.lpstrFilter = "Nds file (*.nds)\0*.nds\0Any file (*.*)\0*.*\0\0";
                            ofn.nFilterIndex = 1;
                            ofn.lpstrFile =  filename;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.lpstrDefExt = "nds";
                            
                            if(!GetOpenFileName(&ofn))
                            {
                                 return 0;
                            }
                            
                            LOG("%s\r\n", filename);
                            
                            // Added for FAT generation
                            // Mic
 					        if (ofn.nFileOffset>0) {
					           	strncpy(szRomPath,filename,ofn.nFileOffset-1);
                	            cflash_close();
             	                cflash_init();
                            } 
                           
                            strcpy(SavName,filename);
                            
                            romnum+=1;
                            
                            if(LoadROM(filename)) execute = FALSE;
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
                            WriteBMP(filename,GPU_screen);
                       }
                  return 0;
                  case IDM_QUICK_PRINTSCREEN:
                       {
                          WriteBMP("./printscreen.bmp",GPU_screen);
                       }
                  return 0;
                  case IDM_STATE_LOAD:
                       {
                            execute = FALSE;
                            OPENFILENAME ofn;
                            //char nomFichier[MAX_PATH] = "";
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
                                 return 0;
                            }
                            
                            //log::ajouter(SavName);
                            
                            savestate_load(SavName);
                            execute = TRUE;
                       }
                  return 0;
                  case IDM_STATE_SAVE:
                       {
                            execute = FALSE;
                            OPENFILENAME ofn;
                            //char nomFichier[MAX_PATH] = "";
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
                            
                            //strncpy(SavName + "dst", FileName, strlen(FileName)-3);
                            //strcpy(SavName, SavName + "dst");
                            //log = "sram saved to: ";
                            //strcat(log, (const char*)SavName);
                            //log::ajouter(log);
                            
                            //strncpy(SavName2, SavName, strlen(SavName)-3);
                            //strncat(SavName2, "dst", 3);
                            //strcpy(SavName, SavName + "dst");
                            
                            savestate_save(SavName);
                            execute = TRUE;
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
                            cwindow_struct IoregView;

                            if (CWindow_Init2(&IoregView, hAppInst, HWND_DESKTOP, "IO REG VIEW", IDD_IO_REG, IoregView_Proc) == 0)
                               CWindow_Show(&IoregView);
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
                       execute = TRUE;
                  return 0;
                  case IDM_PAUSE:
                       execute = FALSE;
                  return 0;
                  case IDM_RESET:
                  return 0;
                  case IDM_CONFIG:
                       {
                            cwindow_struct ConfigView;

                            if (CWindow_Init2(&ConfigView, hAppInst, HWND_DESKTOP, "Configure Controls", IDD_CONFIG, ConfigView_Proc) == 0)
                               CWindow_Show(&ConfigView);

                       }
                  return 0;
             }
             return 0;
        default:                      /* for messages that we don't deal with */
            return DefWindowProc (hwnd, message, wParam, lParam);
    }

    NDSDeInit();

    return 0;
}
