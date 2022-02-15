/*
	Copyright (C) 2006 Theo Berkau
	Copyright (C) 2006-2016 DeSmuME team

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

#ifndef _MAIN_H_
#define _MAIN_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "types.h"
#include "CWindow.h"

extern WINCLASS	*MainWindow;
extern HINSTANCE hAppInst;
extern HMENU mainMenu; //Holds handle to the main DeSmuME menu
extern CToolBar* MainWindowToolbar;
extern CRITICAL_SECTION win_backbuffer_sync;

extern volatile bool execute, paused;
extern bool romloaded;
bool NDS_Pause(bool showMsg = true);
void NDS_UnPause(bool showMsg = true);
void LoadSaveStateInfo();
void Display();
void Pause();
void Unpause();
void TogglePause();
void FrameAdvance(bool state);
void ResetGame();	//Resets game (for the menu item & hotkey
void AviRecordTo();
void AviEnd();
void WavRecordTo(int wavmode);
void WavEnd();
void UpdateTitle();
void UpdateToolWindows();
bool DemandLua();
void SetRotate(HWND hwnd, int rot, bool user = true);
void SaveWindowPos(HWND hwnd);
void SaveWindowSize(HWND hwnd);
void SaveWindowSizePos(HWND hwnd);
void RestoreWindow(HWND hwnd);
void ShowFullScreen(HWND hwnd);
int GetValid3DIntSetting(char *settingName, const int defVal, const int arrName[], const int arrSize);

extern bool frameCounterDisplay;
extern bool FpsDisplay;
extern bool ShowLagFrameCounter;

#define GPU3D_OPENGL_3_2 1
#define GPU3D_SWRAST 2
#define GPU3D_OPENGL_OLD 3

extern void Change3DCoreWithFallbackAndSave(int newCore);

extern int backupmemorytype;
extern u32 backupmemorysize;
extern int gpu_bpp;

void WIN_InstallCFlash();
void WIN_InstallGBACartridge();

#define IDM_RECENT_RESERVED0                    65500
#define IDM_RECENT_RESERVED1                    65501
#define IDM_RECENT_RESERVED2                    65502
#define IDM_RECENT_RESERVED3                    65503
#define IDM_RECENT_RESERVED4                    65504
#define IDM_RECENT_RESERVED5                    65505
#define IDM_RECENT_RESERVED6                    65506
#define IDM_RECENT_RESERVED7                    65507
#define IDM_RECENT_RESERVED8                    65508
#define IDM_RECENT_RESERVED9                    65509
#define IDM_RECENT_RESERVED10                   65510
#define IDM_RECENT_RESERVED11                   65511
#define IDM_RECENT_RESERVED12                   65512
#define IDM_RECENT_RESERVED13                   65513

#define IDT_VIEW_DISASM7						50001
#define IDT_VIEW_DISASM9                		50002
#define IDT_VIEW_MEM7                   		50003
#define IDT_VIEW_MEM9                   		50004
#define IDT_VIEW_IOREG                  		50005
#define IDT_VIEW_PAL                    		50006
#define IDT_VIEW_TILE                   		50007
#define IDT_VIEW_MAP                    		50008
#define IDT_VIEW_OAM                    		50009
#define IDT_VIEW_MATRIX                 		50010
#define IDT_VIEW_LIGHTS                 		50011
#define IDM_EXEC								50112

#define WM_CUSTINVOKE							WM_USER+52

#endif
